#ifndef _WEBSOCKET_SERVER
#define _WEBSOCKET_SERVER

#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>
#include <json/json.h>
#include <boost/asio/io_context.hpp>

#include <functional>
#include <string>
#include <vector>
#include <mutex>
#include <map>
#include <algorithm>
#include <functional>
#include <iostream>
#include <thread>

using std::map;
using std::string;
using std::vector;

typedef websocketpp::server<websocketpp::config::asio_tls> WebsocketEndpoint;
typedef websocketpp::connection_hdl ClientConnection;
typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> ssl;
typedef boost::asio::io_context io_context;

// See https://wiki.mozilla.org/Security/Server_Side_TLS for more details about
// the TLS modes. The code below demonstrates how to implement both the modern
enum tls_mode {
    MOZILLA_INTERMEDIATE = 1,
    MOZILLA_MODERN = 2
};

class websocketserver
{
public:
    websocketserver();
    ~websocketserver();

    // monitor the port
    void run(int port);

    // Returns the number of currently connected clients
    size_t numConnections();

    // Sends a message to an individual client
    //(Note: the data transmission will take place on the thread that called WebsocketServer::run())
    void sendMessage(ClientConnection conn, const string &messageType, const Json::Value &arguments);

    // Sends a message to all connected clients
    //(Note: the data transmission will take place on the thread that called WebsocketServer::run())
    void broadcastMessage(const string &messageType, const Json::Value &arguments);

    //Registers a callback for when a client connects
	template <typename CallbackTy>
	void connect(CallbackTy handler)
	{
		//Make sure we only access the handlers list from the networking thread
		this->eventLoop.post([this, handler]() {
			this->connectHandlers.push_back(handler);
		});
	}

    //Registers a callback for when a client disconnects
	template <typename CallbackTy>
	void disconnect(CallbackTy handler)
	{
		//Make sure we only access the handlers list from the networking thread
		this->eventLoop.post([this, handler]() {
			this->disconnectHandlers.push_back(handler);
		});
	}
		
	//Registers a callback for when a particular type of message is received
	template <typename CallbackTy>
	void message(const string& messageType, CallbackTy handler)
	{
		//Make sure we only access the handlers list from the networking thread
		this->eventLoop.post([this, messageType, handler]() {
			this->messageHandlers[messageType].push_back(handler);
		});
	}

protected:
    io_context eventLoop;

    static Json::Value parseJson(const string& json);
	static string stringifyJson(const Json::Value& val);
	
	void onOpen(ClientConnection conn);
	void onClose(ClientConnection conn);
	void onMessage(ClientConnection conn, WebsocketEndpoint::message_ptr msg);
    ssl onTlsInit(ClientConnection conn, tls_mode mode);
    std::string get_password();
		
	WebsocketEndpoint endpoint;
	vector<ClientConnection> openConnections;
	std::mutex connectionListMutex;
		
	vector<std::function<void(ClientConnection)>> connectHandlers;
	vector<std::function<void(ClientConnection)>> disconnectHandlers;
	map<string, vector<std::function<void(ClientConnection, const Json::Value&)>>> messageHandlers;
};

#endif