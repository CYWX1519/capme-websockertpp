#include "websocketserver.hpp"

//The name of the special JSON field that holds the message type for messages
#define MESSAGE_FIELD "__MESSAGE__"

websocketserver::websocketserver()
{
    //Wire up our event handlers
	this->endpoint.set_open_handler(std::bind(&websocketserver::onOpen, this, std::placeholders::_1));
	this->endpoint.set_close_handler(std::bind(&websocketserver::onClose, this, std::placeholders::_1));
	this->endpoint.set_message_handler(std::bind(&websocketserver::onMessage, this, std::placeholders::_1, std::placeholders::_2));
	this->endpoint.set_tls_init_handler(std::bind(&websocketserver::onTlsInit, this, std::placeholders::_1, MOZILLA_INTERMEDIATE));

	//Initialise the Asio library, using our own event loop object
	this->endpoint.init_asio(&(this->eventLoop));
}

websocketserver::~websocketserver()
{
}

void websocketserver::run(int port)
{
    //Listen on the specified port number and start accepting connections
	this->endpoint.listen(port);
	this->endpoint.start_accept();
	
	//Start the Asio event loop
	this->endpoint.run();
}

size_t websocketserver::numConnections()
{
    //Prevent concurrent access to the list of open connections from multiple threads
	std::lock_guard<std::mutex> lock(this->connectionListMutex);
	
	return this->openConnections.size();
}

void websocketserver::sendMessage(ClientConnection conn, const string& messageType, const Json::Value& arguments)
{
	//Copy the argument values, and bundle the message type into the object
	Json::Value messageData = arguments;
	messageData[MESSAGE_FIELD] = messageType;
	
	//Send the JSON data to the client (will happen on the networking thread's event loop)
	this->endpoint.send(conn, websocketserver::stringifyJson(messageData), websocketpp::frame::opcode::text);
}

void websocketserver::broadcastMessage(const string& messageType, const Json::Value& arguments)
{
	//Prevent concurrent access to the list of open connections from multiple threads
	std::lock_guard<std::mutex> lock(this->connectionListMutex);
	
	for (auto conn : this->openConnections) {
		this->sendMessage(conn, messageType, arguments);
	}
}

Json::Value websocketserver::parseJson(const string& json)
{
	Json::Value root;
	Json::Reader reader;
	reader.parse(json, root);
	return root;
}

string websocketserver::stringifyJson(const Json::Value& val)
{
	//When we transmit JSON data, we omit all whitespace
	Json::StreamWriterBuilder wbuilder;
	wbuilder["commentStyle"] = "None";
	wbuilder["indentation"] = "";
	
	return Json::writeString(wbuilder, val);
}

void websocketserver::onOpen(ClientConnection conn)
{
	{
		//Prevent concurrent access to the list of open connections from multiple threads
		std::lock_guard<std::mutex> lock(this->connectionListMutex);
		
		//Add the connection handle to our list of open connections
		this->openConnections.push_back(conn);
	}
	
	//Invoke any registered handlers
	for (auto handler : this->connectHandlers) {
		handler(conn);
	}
}

void websocketserver::onClose(ClientConnection conn)
{
	{
		//Prevent concurrent access to the list of open connections from multiple threads
		std::lock_guard<std::mutex> lock(this->connectionListMutex);
		
		//Remove the connection handle from our list of open connections
		auto connVal = conn.lock();
		auto newEnd = std::remove_if(this->openConnections.begin(), this->openConnections.end(), [&connVal](ClientConnection elem)
		{
			//If the pointer has expired, remove it from the vector
			if (elem.expired() == true) {
				return true;
			}
			
			//If the pointer is still valid, compare it to the handle for the closed connection
			auto elemVal = elem.lock();
			if (elemVal.get() == connVal.get()) {
				return true;
			}
			
			return false;
		});
		
		//Truncate the connections vector to erase the removed elements
		this->openConnections.resize(std::distance(openConnections.begin(), newEnd));
	}

	//Invoke any registered handlers
	for (auto handler : this->disconnectHandlers) {
		handler(conn);
	}
}

void websocketserver::onMessage(ClientConnection conn, WebsocketEndpoint::message_ptr msg)
{
	//Validate that the incoming message contains valid JSON
	Json::Value messageObject = websocketserver::parseJson(msg->get_payload());
	if (messageObject.isNull() == false)
	{
		//Validate that the JSON object contains the message type field
		if (messageObject.isMember(MESSAGE_FIELD))
		{
			//Extract the message type and remove it from the payload
			std::string messageType = messageObject[MESSAGE_FIELD].asString();
			messageObject.removeMember(MESSAGE_FIELD);
			
			//If any handlers are registered for the message type, invoke them
			auto& handlers = this->messageHandlers[messageType];
			for (auto handler : handlers) {
				handler(conn, messageObject);
			}
		}
	}
}

std::string websocketserver::get_password()
{
    return "";
}

ssl websocketserver::onTlsInit(ClientConnection comm, tls_mode mode)
{
    namespace asio = websocketpp::lib::asio;

    ssl ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::sslv23);

    try {
        if (mode == MOZILLA_MODERN) {
            // Modern disables TLSv1
            ctx->set_options(asio::ssl::context::default_workarounds |
                             asio::ssl::context::no_sslv2 |
                             asio::ssl::context::no_sslv3 |
                             asio::ssl::context::no_tlsv1 |
                             asio::ssl::context::single_dh_use);
        } else {
            ctx->set_options(asio::ssl::context::default_workarounds |
                             asio::ssl::context::no_sslv2 |
                             asio::ssl::context::no_sslv3 |
                             asio::ssl::context::single_dh_use);
        }
        // ctx->set_password_callback(bind(&websocketserver::get_password));
        ctx->use_certificate_chain_file("/home/rane/project/cpp/ManagerServer/server.pem");
        ctx->use_private_key_file("/home/rane/project/cpp/ManagerServer/server.pem", asio::ssl::context::pem);
        
        // Example method of generating this file:
        // `openssl dhparam -out dh.pem 2048`
        // Mozilla Intermediate suggests 1024 as the minimum size to use
        // Mozilla Modern suggests 2048 as the minimum size to use.
        ctx->use_tmp_dh_file("/home/rane/project/cpp/ManagerServer/dh.pem");
        
        std::string ciphers;
        
        if (mode == MOZILLA_MODERN) {
            ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!3DES:!MD5:!PSK";
        } else {
            ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA256:AES256-SHA256:AES128-SHA:AES256-SHA:AES:CAMELLIA:DES-CBC3-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!MD5:!PSK:!aECDH:!EDH-DSS-DES-CBC3-SHA:!EDH-RSA-DES-CBC3-SHA:!KRB5-DES-CBC3-SHA";
        }
        
        if (SSL_CTX_set_cipher_list(ctx->native_handle() , ciphers.c_str()) != 1) {
            std::cout << "Error setting cipher list" << std::endl;
        }
    } catch (std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }
    return ctx;
}




