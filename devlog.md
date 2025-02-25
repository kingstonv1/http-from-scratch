### v0.1: Echo Server

Before even concerning myself with the fine details of the HTTP specification, I need to know the basic details of socket programming. I've chosen C++ for its access to the basic C Unix APIs and common usage for this kind of application. Socket programming in C/C++ isn't too unlike it is in Python. The following commands have to be used to host the server:
1. Create a socket file handler.
2. Bind that socket to a port on the local machine.
3. Listen for incoming connections on the socket.
4. Accept an incoming connection and store that information as a buffer.
5. Parse the data sent to you and respond accordingly.
6. Close the socket after use.

The following API calls are used to perform these jobs on Unix:

| Function   | Parameters                                                                                                                                                                                                                                                                                                                                                                                                                                 | Return Value                                            |
| ---------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------- |
| `socket()` | **domain**: The communication domain (protocol) that will be used to conduct connections. This takes an enum.<br>**socket type**: The semantic rules/implementation that the socket communication will follow. Typically `SOCK_STREAM`.<br>**protocol**: An integer denoting which protocol of the available protocols in the domain will be used. Most communication domains only support one protocol, so this parameter is typically 0. | A file handler for the socket.                          |
| `bind()`   | **socket**: The socket file handler that will bind to the port.<br>**address**: A sockaddr (or sockaddr_in for internet connections, found in `<netinet/in.h>`) struct with its fields properly set. It has to be dereferenced to a pointer.<br>**address size**: `sizeof()` the previous address struct.                                                                                                                                  | Zero.                                                   |
| `listen()` | **socket**: The socket file handler to listen with.<br>**queue size**: The maximum amount of queued requests that will be accepted before the server turns them down.                                                                                                                                                                                                                                                                      | Zero.                                                   |
| `accept()` | **socket**: The socket file handler to accept the connection. Must already be bound and listening.<br>**addr**: The address that the incoming connection is expected to come from, in the form of a `sockaddr` pointer. `nullptr` if you want to accept from any address.<br>**addrlen**: The length of the previous address struct. Also a `nullptr` if the previous argument was a `nullptr`.                                            | A file handler for the accepted (not listening) socket. |
| `recv()`   | **socket**: The *client* socket file handler to receive information from. This is the handler provided by the above `accept()` call.<br>**buf**: A pointer to a buffer that will contain the information.<br><br>**len**: The size of the above buffer.<br>**flags**: A binary OR'd integer of flags described in the API documentation that describe how the connection should behave.                                                    | The number of bytes recieved.                           |
| `close()`  | **handler**: The file handler to close.                                                                                                                                                                                                                                                                                                                                                                                                    | Zero.                                                   |
*You can find details about these individual functions by looking at the man pages for them.*

##### Error Handling

When an error is thrown by a Unix API call, the program sets the system `errno` based on the possible errors laid out in the API and returns a value of -1 from the function. Therefore, to handle an error, you should check if the function returned a value of -1, and if it did, display the current `errno` and exit.
The `msg` parameter in my function (later) is set by the programmer when calling the function to give some context about where the error occurred. It's not actually necessary for the functionality of handling the Unix errors.

##### File Handlers

The way that Unix handles system functions is by creating files for the interfaces to interact with. When you open a file on your system, whether to interact with it as a socket or read/write data to it, you open a file handler. In C/C++, you're expected to close file handlers after you're done with them.  To accomplish this at this point in the project, I keep a namespace with my file handlers and bind a cleanup function to std::atexit. The cleanup function logs some information to a file for debugging purposes and closes the file handlers.

There is an issue with closing duplicate file handlers - if you close a file handler twice, it causes undefined behavior, as another thread or program may have opened up that file handler since you close it. To alleviate this, you should set your file handlers to `-1` after you close them. Closing a file handler of `-1` has no adverse affects, and it also provides an easy way to check if the handler was closed by us in the past.

##### Finished v0.1 Code
```cpp
#include <iostream>
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
	addr.sin_port = htons(80); // HTTP is typically hosted on port 80.
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

	fhandler::client = accept(fhandler::server, nullptr, nullptr);
	if (fhandler::client == -1) { handle_error("Error accepting an incoming connection."); }

	while (true) {
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

		send(fhandler::client, buf, sizeof(buf), 0);
	}

	close(fhandler::client);
	fhandler::client = -1;

	return 0;
}
```

### v0.2: Basic HTTP GET Server

Now, we need to expand this server to handle simple GET requests. We will worry about other verbs later - but we will try to build with the consideration that we will need to handle them. Here are our requirements for v0.2:
- [ ] Parse out bare minimum HTTP request headers CONNECTION, KEEP-ALIVE, HOST, and ACCEPT
- [ ] Respond appropriately to an ACCEPT: `text/*` or `text/html` request with a web page
- [ ] Handle errors caused either on the client side or server side by sending back an appropriate response code and closing the connection

Given that these are our design requirements, there are a few specific problems I'd like to address in the fundamental architecture of the server:
- [ ] The server should never hang or fail to respond to a request, instead returning an error. This may be accomplished with good exception handling.
- [ ] The server should never experience downtime as a result of a maliciously or poorly constructed resource request.
- [ ] The server, in the interest of security, should err on the side of caution when responding to slightly malformed requests. I'm considering path traversal and query injection specifically here - if it seems likely a flaw is being exploited, elect not to respond instead of risking malicious behavior.
- [ ] The program should be written in such a way that we can arbitrarily add new functionality like headers and status codes.

My most intuitive implementation that follows these requirements and considerations looks like this:
- The server receives a request and parses it into an `http_request` object. We should be careful to ensure that the parsing logic throws exceptions as little as possible. Instead, processing of the request should be immediately interrupted and the server should send back an appropriate error status.
- Based on the path (and perhaps `content-type`) provided in the request, the server will fetch the desired resource and parse it into an `http_response` object, which encapsulates the data to be sent and determines which headers will be sent in the response.
- The response object will be compared against the request headers to determine if the response complies with the specifics of the client's request. If the resource is incorrect, stop processing during the first error and return the proper status code.
- Finally, if the request processing has not been interrupted at this point, the resource should be what was requested. Send the resource to the client.


----

##### 02/25/25 Update 1: Request line parsing & http_request objects!

```
Listening for incoming connections...
Recieved a request of length 73
GET / HTTP/1.1
Host: localhost
User-Agent: curl/7.88.1
Accept: */*


Incoming HTTP Request.
GET / HTTP/1.1
```

My server can parse out the very first line of a GET request!! Yippee!!!
There's been a lot of time that's gone by since planning for the basic GET server, though - so why did a simple parser take so long? Perfectionism. Which might be a good thing here, even if it makes things a bit slow.

To begin, I finally decided that I was going to do fully-fledged headers for my classes, which were a bit tricky to get the grasp of. No other language that I've used really has this setup - forward declarations in a header and actual definitions in a code file. So it took a decent amount of shuffling around between http_request.h and http_request.cpp to figure out where and how I should define each thing. But I'm fairly sure what I have now is serviceable and won't cause me too much headache in the future.

Here were the decisions I made that took some deliberation, and why I wound up going with what I went with:
- **Enum for HTTP Verbs**: The type I used for encapsulating the HTTP Verb used in the request is an enum. This made sense to me because a proper request can only take one of a handful of methods, and storing them in something too malleable like a string feels like an issue waiting to happen. My only concern with this is that I have no way to enforce that the enum parsing happens in *only* http_request.parse_request(). I also parse the enum out into a string in the http_request.get_method() getter - I can see myself maybe wanting to parse it out in a similar way elsewhere? So long as that getter stays the only place where I parse out that enum value, I'll keep it an enum, but if I have to write too many switch statements I'll consider refactoring it out.
- **Simple Parsing for the HTTP request line**: To parse out the request line at the beginning of an HTTP message, I just used a simple for loop and an incrementor at spaces. I considered making something more fault-tolerable - as it stands, duplicate spaces in your request are fine, but a lack of spaces or too many trailing spaces might explode my parser. But at some point you do have to put the fault on the user - after all, dealing with people tinkering with their web request / sending slightly bad requests isn't really a heavy consideration for 99% of users. So long as the algorithm can consistently parse out what it needs to from a *good* request, I'm not stressing.
- **Deferring error handling**: I'm not handling any kind of parsing errors/malformed request errors in the http_request object. I'm just parsing the request the best I possibly can and then handing that over to a response generator object of some sort later. The way that object will be able to tell that something went wrong during parsing is that field will be set to a strong, sensible default at initialization. The strings start empty, the buffer pointer starts as a nullptr, and the http verb starts as an `http_verb::BAD`. This certainly feels like it will work for now, but I am a bit worried that there's no actual mechanism enforcing this practice. If the class grows too big, it might be helpful to have some sort of more explicit way to signal a parsing problem for any individual field. But perhaps I'm thinking too hard about this - I'm going to leave it as it is until it becomes a problem.

All of that being said, the object's current functionality works well. I haven't had my server crash (yet!), though I haven't exploded it with bad HTTP requests yet. I also haven't built anything on top of it. I think the way I've built it out is sensible, but I don't doubt there will be issues later.

All I should have left to do in the http_request object is a header parser - which seems simpler than the request line parser! Later on, I will need to handle request content, especially for POSTs, but I can figure that out after the GET functionality is done.

Bye bye ^-^