#include "http_request.h"
#include "http_response.h"

#include <iostream>


#include <string>
#include <map>



#include <fstream>

#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

bool debug = true;

namespace fhandler {
	int server;
	int client;
}

namespace server_log {
	int empty_requests = 0;
}

void handle_error(std::string msg) {
	perror(msg.c_str());
	std::cout << "errno " << errno << std::endl;
	exit(EXIT_FAILURE);
}

void update_log() {
	std::ofstream logfile("latest.log");

	if (logfile.is_open()) {
		logfile << "--------------------------" << '\n';
		logfile << "Blank requests recieved: " << server_log::empty_requests << '\n';
	} else {
		std::cerr << "Unable to open logfile";
	}
}

// Only call this function once at exit. 
void cleanup(void) {
	if (fhandler::server != -1) {
		int close_msg = close(fhandler::server);
		if (close_msg == -1) { handle_error("Error closing the server file handler."); }
	}

	if (fhandler::client != -1) {
		int close_msg = close(fhandler::client);
		if (close_msg == -1) { handle_error("Error closing the client file handler."); }
	}

	if (debug) { update_log(); }
}

int main() {
	// Intercept the program exit to clean up so we don't 
	// leave sockets open after we're done.
	std::atexit(cleanup);

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(4459); // HTTP is typically hosted on port 80.
	addr.sin_addr.s_addr = INADDR_ANY;

	fhandler::server = socket(AF_INET, SOCK_STREAM, 0);
	if (fhandler::server == -1) { handle_error("Error initializing socket."); }

	// Tell the socket to terminate the socket if it's sat in TIME_WAIT for too long.
	// Prevents resource leaks / the socket already being in use if we reboot the server.
	int opt = 1;
	setsockopt(fhandler::server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	int bind_res = bind(fhandler::server, (const sockaddr *) &addr, sizeof(addr));
	if (bind_res == -1) { handle_error("Error binding to socket address."); }

	int list_res = listen(fhandler::server, 3);
	if (list_res == -1) { handle_error("Error listening for connections."); }
	std::cout << "Listening for incoming connections..." << std::endl;

	while (true) {
		fhandler::client = accept(fhandler::server, nullptr, nullptr);
		if (fhandler::client == -1) { handle_error("Error accepting an incoming connection."); }

		char buf[1024];
		// Clear the buffer before we receive a message to prevent past requests being re-server_logged
		// if the client sends a message of length 0.
		memset(buf, 0, sizeof(buf));
		int msg_size = recv(fhandler::client, buf, sizeof(buf), 0);
		if (msg_size == -1) { handle_error("Error reciving a message."); }

		// We recieved no message. Don't print anything.
		// Maybe a bad check depending on the type of data sent ? Look into this
		if (buf[0] == '\0') { 
			server_log::empty_requests++;	
			continue; 
		}

		std::cout << "Recieved a request of length " << msg_size / sizeof(char) << std::endl;
		// Add a null at the end of our http method so we don't print unset memory.
		if (msg_size > 0) {
			buf[msg_size] = '\0';
		}

		std::cout << buf << std::endl;

		http_request current_request = http_request(buf);
		http_response res = http_response(&current_request);
		const char* arr = res.get_response();
		std::string response(res.get_response());

		std::cout << "Here's what's being sent: " << arr << std::endl;
		
		send(fhandler::client, arr, strlen(arr), 0);
		std::cout << "Sent" << std::endl;
	}

	close(fhandler::client);
	fhandler::client = -1;

	return 0;
}
