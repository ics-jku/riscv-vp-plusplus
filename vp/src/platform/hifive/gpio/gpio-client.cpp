/*
 * gpio-client.cpp
 *
 *  Created on: 5 Nov 2018
 *      Author: dwd
 */

#include "gpio-client.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

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

template<typename T>
bool writeStruct(int handle, T* s){
	return write(handle, s, sizeof(T)) == sizeof(T);
}

template<typename T>
bool readStruct(int handle, T* s){
	return read(handle, s, sizeof(T)) == sizeof(T);
}

GpioClient::GpioClient() : fd(-1), currentHost("localhost") {}

GpioClient::~GpioClient() {
	if (fd >= 0) {
		close(fd);
	}

	for (const auto& thread : dataChannelThreads) {
		close(thread.fd); // closing socket should signal quit
	}

	for (auto& thread : dataChannelThreads) {
		if(thread.thread.joinable()) {
			thread.thread.join();
		}
	}
}

bool GpioClient::update() {
	Request req;
	memset(&req, 0, sizeof(Request));
	req.op = Request::Type::GET_BANK;
	if (!writeStruct(fd, &req)) {
		cerr << "[gpio-client] Error or closed socket in update request: " << strerror(errno) << endl;
		return false;
	}
	if (!readStruct(fd, &state)) {
		cerr << "[gpio-client] Error or closed socket in update read: " << strerror(errno) << endl;
		return false;
	}
	return true;
}

bool GpioClient::setBit(uint8_t pos, Tristate val) {
	Request req;
	memset(&req, 0, sizeof(Request));
	req.op = Request::Type::SET_BIT;
	req.setBit.pin = pos;
	req.setBit.val = val;

	if (!writeStruct(fd, &req)) {
		cerr << "[gpio-client] Error in setBit" << endl;
		return false;
	}
	return true;
}

uint16_t GpioClient::requestIOFchannel(PinNumber pin) {
	Request req;
	memset(&req, 0, sizeof(Request));
	req.op = Request::Type::REQ_IOF;
	req.reqIOF.pin = pin;

	if (!writeStruct(fd, &req)) {
		cerr << "[gpio-client] Error in write SPI IOF register request" << endl;
		return 0;
	}

	Req_IOF_Response resp;

	if (!readStruct(fd, &resp)) {
		cerr << "[gpio-client] Error in read SPI IOF register response" << endl;
		return 0;
	}

	if(resp.port < 1024) {
		cerr << "[gpio-client] Invalid port " << resp.port << " given from IOF register response" << endl;
		return 0;
	}
	return resp.port;
}

bool GpioClient::registerSPIOnChange(PinNumber pin, OnChange_SPI fun){

	auto port = requestIOFchannel(pin);

	if(port == 0) {
		cerr << "[gpio-client] [SPI channel] IOF port request unsuccessful" << endl;
		return false;
	}

	char port_c[10]; // may or may not be enough, but we are not planning for failure!
	sprintf(port_c, "%6d", port);

	//cout << "Got offered port " << port_c << " for pin " << (int) pin << endl;

	int dataChannel = connectToHost(currentHost, port_c);
	if(dataChannel < 0) {
		cerr << "[gpio-client] [SPI channel] Could not connect to offered port " << currentHost << ":" << port_c << endl;
		return false;
	}

	dataChannelThreads.emplace_back(DataChannelDescription{
		pin, dataChannel, std::thread(std::bind(&GpioClient::handleSPIchannel, this, dataChannel, fun))
	});

	return true;
}

void GpioClient::handleSPIchannel(int socket, OnChange_SPI fun) {
	// open and running connection
	SPI_Command spi_in;
	while(readStruct(socket, &spi_in)) {
		SPI_Response resp = fun(spi_in);
		if(!writeStruct(socket, &resp)) {
			cerr << "[gpio-client] [SPI channel] Error in SPI write answer from fd " << socket << endl;
			close(socket);
			return;
		}
	}
	close(socket);
	cerr << "[gpio-client] [SPI channel] Error or closed socket" << endl;
}

int GpioClient::connectToHost(const char *host, const char *port) {
	int fd = -1;

	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	// loop through all the results and connect to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			cerr << "[gpio-client] opening of socket unsuccessful " << strerror(errno) << endl;
			continue;
		}

		if (connect(fd, p->ai_addr, p->ai_addrlen) == -1) {
			close(fd);
			//perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		//fprintf(stderr, "client: failed to connect\n");
		freeaddrinfo(servinfo);
		return -1;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	DEBUG("[gpio-client] connecting to %s\n", s);

	freeaddrinfo(servinfo);  // all done with this structure

	return fd;
}

bool GpioClient::setupConnection(const char *host, const char *port) {
	if((fd = connectToHost(host, port)) < 0) {
		//cerr << "[gpio-client] Could not connect to " << host << ":" << port << endl;
		return false;
	}
	currentHost = host;
	return true;
}
