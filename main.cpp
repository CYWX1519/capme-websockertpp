#include "main.hpp"

// The port number the WebSocket server listens on
#define PORT_NUMBER 8080

int main(int, char **)
{
	// Initialize the logger and pass our formatter as a template parameter to init function.
	plog::init<plog::MyFormatter<false>>(plog::debug, "runningStatus.log");
	PLOGI << "log config initialized succeed. starting the program...";

	mysqlOperator::mysqlOperator mysqlOperators;

	std::cout << "Hello, world!\n";
	websocketserver server;
	io_context mainEventLoop;

	// Register our network callbacks, ensuring the logic is run on the main thread's event loop
	server.connect([&mainEventLoop, &server](ClientConnection conn)
				   { mainEventLoop.post([conn, &server]()
										{
			std::clog << "Connection opened." << std::endl;
			std::clog << "There are now " << server.numConnections() << " open connections." << std::endl;
			
			//Send a hello message to the client
			server.sendMessage(conn, "hello", Json::Value()); }); });
	server.disconnect([&mainEventLoop, &server](ClientConnection conn)
					  { mainEventLoop.post([conn, &server]()
										   {
			std::clog << "Connection closed." << std::endl;
			std::clog << "There are now " << server.numConnections() << " open connections." << std::endl; }); });
	server.message("message", [&mainEventLoop, &server](ClientConnection conn, const Json::Value &args)
				   { mainEventLoop.post([conn, args, &server]()
										{
			std::clog << "message handler on the main thread" << std::endl;
			std::clog << "Message payload:" << std::endl;
			for (auto key : args.getMemberNames()) {
				std::clog << "\t" << key << ": " << args[key].asString() << std::endl;
			}
			
			//Echo the message pack to the client
			server.sendMessage(conn, "message", args); }); });

	// Start the networking thread
	std::thread serverThread([&server]()
							 { server.run(PORT_NUMBER); });

	// Start a keyboard input thread that reads from stdin
	std::thread inputThread([&server, &mainEventLoop]()
							{
		string input;
		while (1)
		{
			//Read user input from stdin
			std::getline(std::cin, input);
			
			//Broadcast the input to all connected clients (is sent on the network thread)
			Json::Value payload;
			payload["input"] = input;
			server.broadcastMessage("userInput", payload);
			
			//Debug output on the main thread
			mainEventLoop.post([]() {
				std::clog << "User input debug output on the main thread" << std::endl;
			});
		} });

	// Start the event loop for the main thread
	io_context::work work(mainEventLoop);
	mainEventLoop.run();

	return EXIT_SUCCESS;
}
