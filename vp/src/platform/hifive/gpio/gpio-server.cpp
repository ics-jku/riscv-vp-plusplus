/*
 * gpio-server.cpp
 *
 *  Created on: 5 Nov 2018
 *      Author: dwd
 */

#include "gpio-server.hpp"
#include "gpio-client.hpp" // for force quit of listening switch

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
#include <thread>

#define ENABLE_DEBUG (0)
#include "debug.h"

using namespace std;
using namespace gpio;

// get sockaddr, IPv4 or IPv6:
/*
static void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}
*/

template<typename T>
bool writeStruct(int handle, T* s){
	return write(handle, s, sizeof(T)) == sizeof(T);
}
template<typename T>
bool readStruct(int handle, T* s){
	return read(handle, s, sizeof(T)) == sizeof(T);
}

GpioServer::GpioServer() : listener_socket_fd(-1), current_connection_fd(-1), base_port(""), stop(false), fun(nullptr){}

GpioServer::~GpioServer() {
	if (listener_socket_fd >= 0) {
		DEBUG("closing gpio-server socket: %d\n", listener_socket_fd);
		close(listener_socket_fd);
		listener_socket_fd = -1;
	}

	if (this->base_port)
		free((void*)this->base_port);
}

int GpioServer::openSocket(const char *port) {
	int new_fd = -1;

	struct addrinfo hints, *servinfo, *p;
	int yes = 1;
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;	// use my IP

	if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	// loop through all the results and bind to the first we can
	bool found = false;
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((new_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			cerr << "[gpio-server] opening of socket unsuccessful " << strerror(errno) << endl;
			continue;
		}

		if (setsockopt(new_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			close(new_fd);
			cerr << "[gpio-server] setsockopt unsuccessful " << strerror(errno) << endl;
			continue;
		}

		if (::bind(new_fd, p->ai_addr, p->ai_addrlen) == -1) {
			close(new_fd);
			cerr << "[gpio-server] could not bind to addr " << p->ai_addr->sa_data << endl;
			continue;
		}

		found = true;
		break;
	}

	if(!found) {
		return -1;
	}

	freeaddrinfo(servinfo);  // all done with this structure

	if (p == NULL) {
		fprintf(stderr, "gpio-server: failed to bind\n");
		return -3;
	}

	if (listen(new_fd, 1) < 0) {
		cerr << "[gpio-server] Could not start listening on new socket " << new_fd << " ";
		close(new_fd);
		return -4;
	}

	return new_fd;
}

bool GpioServer::setupConnection(const char *port) {
	if (!(this->base_port = strdup(port))) {
		perror("[gpio-server] strdup");
		return false;
	}

	if((listener_socket_fd = openSocket(base_port)) < 0) {
		cerr << "[gpio-server] Could not setup control channel" << endl;
		return false;
	}

	return true;
}

void GpioServer::quit() {
	stop = true;

	// this should force read command to return
	if(current_connection_fd >= 0){
		close(current_connection_fd);
	}

	/* The startListening() loop only checks the stop member
	* variable after accept() returned. However, accept() is a
	* blocking system call and may not return unless a new
	* connection is established. For this reason, we set the stop
	* variable and afterwards connect() to the server socket to make
	* sure the receive loop terminates. */

	GpioClient client;
	if (base_port) client.setupConnection(NULL, base_port);
}

bool GpioServer::isStopped() {
	return stop;
}

void GpioServer::registerOnChange(OnChangeCallback fun) {
	this->fun = fun;
}

int GpioServer::awaitConnection(int socket) {
	struct sockaddr_storage their_addr;  // connector's address information
	socklen_t sin_size = sizeof their_addr;
	// char s[INET6_ADDRSTRLEN];

	int new_connection = accept(socket, (struct sockaddr *)&their_addr, &sin_size);
	if (new_connection < 0) {
		cerr << "[gpio-server] accept return " << strerror(errno) << endl;
		return -1;
	}

	//inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
	//DEBUG("gpio-server: got connection from %s\n", s);

	return new_connection;
}


void GpioServer::startAccepting() {
	// printf("gpio-server: accepting connections (%d)\n", fd);

	while (!stop)  // this would block a bit
	{
		if((current_connection_fd = awaitConnection(listener_socket_fd)) < 0) {
			cerr << "[gpio-server] could not accept new connection" << endl;
			continue;
		}
		handleConnection(current_connection_fd);
	}
}

void GpioServer::handleConnection(int conn) {
	Request req;
	memset(&req, 0, sizeof(Request));
	int bytes;
	while (readStruct(conn, &req)) {
		// hexPrint(reinterpret_cast<char*>(&req), bytes);
		switch (req.op) {
			case Request::Type::GET_BANK:
				if (!writeStruct(conn, &state)) {
					cerr << "could not write answer" << endl;
					close(conn);
					return;
				}
				break;
			case Request::Type::SET_BIT: {
				// printRequest(&req);
				if (req.setBit.pin >= max_num_pins) {
					cerr << "[gpio-server] invalid request setbit pin number " << req.setBit.pin << endl;
					return;
				}

				if (isIOF(state.pins[req.setBit.pin])){
					cerr << "[gpio-server] Ignoring setPin on IOF" << endl;
					return;
				}

				// sanity checks ok

				if (fun != nullptr) {
					fun(req.setBit.pin, req.setBit.val);
				} else {
					state.pins[req.setBit.pin] = toPinstate(req.setBit.val);
				}
				break;
			}
			case Request::Type::REQ_IOF:
			{
				Req_IOF_Response response{0};
				if(!isIOF(state.pins[req.reqIOF.pin])){
					cerr << "[gpio-server] IOF request on non-iof pin " << (int)req.reqIOF.pin << endl;
					if (!writeStruct(conn, &response)) {
						cerr << "[gpio-server] could not write IOF-Req answer" << endl;
						close(conn);
						return;
					}
					// Not a major fault, so break instead of return
					break;
				}

				int new_data_socket = openSocket("0");	// zero shall indicate random port
				if (new_data_socket < 0) {
					cerr << "[gpio-server] Could not setup IOF data channel socket" << endl;
					// no return, so that response is 0
				}

				{
					struct sockaddr_in sin;
					socklen_t len = sizeof(sin);
					if (getsockname(new_data_socket, (struct sockaddr *)&sin, &len) == -1) {
						cerr << "[gpio-server] Could not get port with sockname" << endl;
						close(new_data_socket);
					}
					response.port = ntohs(sin.sin_port);
				}

				if (!writeStruct(conn, &response)) {
					cerr << "[gpio-server] could not write IOF-Req answer" << endl;
					if (new_data_socket < 0)
						close(conn);
					return;
				}

				// TODO: set socket nonblock with timeout (~1s)
				int data_channel = awaitConnection(new_data_socket);

				close(new_data_socket);	// accepting just the first connection

				if(data_channel < 0) {
					cerr << "[gpio-server] IOF data channel connection not successful" << endl;
					// TODO: really close normal connection for reset?
					close(conn);
					return;
				}

				active_IOF_channels.emplace(req.reqIOF.pin, data_channel);

				break;
			}
			case Request::Type::END_IOF:
				if(!isIOF(state.pins[req.reqIOF.pin])){
					//cerr << "[gpio-server] IOF quit on non-IOF pin " << (int)req.reqIOF.pin << endl;
					// If pin state changed, we still need to quit channel
				}

				{
				auto channel = active_IOF_channels.find(req.reqIOF.pin);
				if(channel == active_IOF_channels.end()) {
					cerr << "[gpio-server] IOF quit on non followed pin " << (int)req.reqIOF.pin << endl;
					return;
				}

				close(channel->second);
				active_IOF_channels.erase(channel);
				}
				break;
			default:
				cerr << "[gpio-server] invalid request operation" << endl;
				return;
		}
	}

	DEBUG("gpio-client disconnected (%d)\n", bytes);
	close(conn);
}

SPI_Response GpioServer::pushSPI(gpio::PinNumber pin, gpio::SPI_Command byte) {
	auto channel = active_IOF_channels.find(pin);
	if(channel == active_IOF_channels.end()) {
		return 0;
	}
	/*
	 * TODO: If registered on SPI data pins (miso, mosi, clk) and not on CS,
	 * It should receive all SPI data, not just the CS activated ones
	 */
	int sock = channel->second;

	if(!writeStruct(sock, &byte)) {
		cerr << "[gpio-server] Could not write SPI command to cs " << (int)pin << endl;
		close(sock);
		active_IOF_channels.erase(channel);
		return 0;
	}

	SPI_Response response = 0;
	if(!readStruct(sock, &response)) {
		cerr << "[gpio-server] Could not read SPI response to cs " << (int)pin << endl;
		close(sock);
		active_IOF_channels.erase(channel);
	}
	return response;
}
