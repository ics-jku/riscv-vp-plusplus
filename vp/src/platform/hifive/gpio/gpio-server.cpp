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
static void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

GpioServer::GpioServer() : listener_fd(-1), current_connection_fd(-1), port(""), stop(false), fun(nullptr){}

GpioServer::~GpioServer() {
	if (listener_fd >= 0) {
		DEBUG("closing gpio-server socket: %d\n", listener_fd);
		close(listener_fd);
		listener_fd = -1;
	}

	if (this->port)
		free((void*)this->port);
}

bool GpioServer::setupConnection(const char *port) {
	if (!(this->port = strdup(port))) {
		perror("gpio-server: strdup");
		return 1;
	}

	struct addrinfo hints, *servinfo, *p;
	int yes = 1;
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;  // use my IP

	if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((listener_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("gpio-server: socket");
			continue;
		}

		if (setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			perror("setsockopt");
			return false;
		}

		if (::bind(listener_fd, p->ai_addr, p->ai_addrlen) == -1) {
			close(listener_fd);
			perror("gpio-server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo);  // all done with this structure

	if (p == NULL) {
		fprintf(stderr, "gpio-server: failed to bind\n");
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
	if (port) client.setupConnection(NULL, port);
}

bool GpioServer::isStopped() {
	return stop;
}

void GpioServer::registerOnChange(UpdateFunction fun) {
	this->fun = fun;
}

void GpioServer::startListening() {
	if (listen(listener_fd, 1) < 0) {
		cerr << "fd " << listener_fd << " ";
		perror("listen");
		stop = true;
		return;
	}
	// printf("gpio-server: accepting connections (%d)\n", fd);

	struct sockaddr_storage their_addr;  // connector's address information
	socklen_t sin_size = sizeof their_addr;
	char s[INET6_ADDRSTRLEN];

	while (!stop)  // this would block a bit
	{
		current_connection_fd = accept(listener_fd, (struct sockaddr *)&their_addr, &sin_size);
		if (current_connection_fd < 0) {
			cerr << "gpio-server accept return " << current_connection_fd << endl;
			perror("accept");
			stop = true;
			return;
		}

		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
		DEBUG("gpio-server: got connection from %s\n", s);
		handleConnection(current_connection_fd);
	}
}

void GpioServer::handleConnection(int conn) {
	Request req;
	memset(&req, 0, sizeof(Request));
	int bytes;
	while ((bytes = read(conn, &req, sizeof(Request))) == sizeof(Request)) {
		// hexPrint(reinterpret_cast<char*>(&req), bytes);
		switch (req.op) {
			case Request::Type::GET_BANK:
				if (write(conn, &state, sizeof(State)) != sizeof(State)) {
					cerr << "could not write answer" << endl;
					close(conn);
					return;
				}
				break;
			case Request::Type::SET_BIT: {
				// printRequest(&req);
				if (req.setBit.pin >= max_num_pins) {
					cerr << "invalid request setbit pinnumber" << endl;
					return;
				}
				if (req.setBit.val != Tristate::LOW &&
					req.setBit.val != Tristate::HIGH &&
					req.setBit.val != Tristate::UNSET) {
					cerr << "invalid request setbit Tristate" << endl;
					return;
				}

				if (fun != nullptr) {
					fun(req.setBit.pin, req.setBit.val);
				} else {
					state.pins[req.setBit.pin] = req.setBit.val;
 				}
				break;
			}
			case Request::Type::REQ_IOF:
				// TODO
				cerr << "Logstate start not yet implemented" << endl;
				break;
			case Request::Type::END_IOF:
				// TODO
				cerr << "Logstate end not yet implemented" << endl;
				break;
			default:
				cerr << "invalid request operation" << endl;
				return;
		}
	}

	DEBUG("gpio-client disconnected (%d)\n", bytes);
	close(conn);
}
