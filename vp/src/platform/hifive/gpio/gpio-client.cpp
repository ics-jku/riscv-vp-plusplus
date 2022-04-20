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
#include <netinet/tcp.h>	// for TCP_NODELAY
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

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

void GpioClient::closeAndInvalidate(Socket& fd) {
	close(fd);
	fd = -1;
}

GpioClient::GpioClient() : control_channel(-1), data_channel(-1),
		currentHost("localhost") {}

GpioClient::~GpioClient() {
	destroyConnection();
}

bool GpioClient::update() {
	Request req;
	memset(&req, 0, sizeof(Request));
	req.op = Request::Type::GET_BANK;
	if (!writeStruct(control_channel, &req)) {
		cerr << "[gpio-client] Error or closed socket in update request: " << strerror(errno) << endl;
		close(control_channel); // probably closed anyway
		control_channel = -1;
		destroyConnection();
		return false;
	}
	if (!readStruct(control_channel, &state)) {
		cerr << "[gpio-client] Error or closed socket in update read: " << strerror(errno) << endl;
		close(control_channel); // probably closed anyway
		control_channel = -1;
		destroyConnection();
		return false;
	}

	// TODO: Housekeeping for closed IOFunctions?
	return true;
}

bool GpioClient::setBit(uint8_t pos, Tristate val) {
	Request req;
	memset(&req, 0, sizeof(Request));
	req.op = Request::Type::SET_BIT;
	req.setBit.pin = pos;
	req.setBit.val = val;

	if (!writeStruct(control_channel, &req)) {
		cerr << "[gpio-client] Error in setBit" << endl;
		return false;
	}
	return true;
}

gpio::Req_IOF_Response GpioClient::requestIOFchannel(gpio::PinNumber pin, gpio::IOFunction iof_type) {
	Request req;
	memset(&req, 0, sizeof(Request));
	req.op = Request::Type::REQ_IOF;
	req.reqIOF.pin = pin;
	req.reqIOF.iof = iof_type;

	if (!writeStruct(control_channel, &req)) {
		cerr << "[gpio-client] Error in write of SPI IOF register request" << endl;
		closeAndInvalidate(control_channel);
		return {0,0};
	}

	Req_IOF_Response resp;

	if (!readStruct(control_channel, &resp)) {
		cerr << "[gpio-client] Error in read of SPI IOF register response" << endl;
		closeAndInvalidate(control_channel);
		return {0,0};
	}

	return resp;
}


void GpioClient::notifyEndIOFchannel(PinNumber pin) {
	Request req;
	memset(&req, 0, sizeof(Request));
	req.op = Request::Type::END_IOF;
	req.reqIOF.pin = pin;

	if (!writeStruct(control_channel, &req)) {
		cerr << "[gpio-client] Error in write 'Stop SPI IOF' for pin " << (int)pin << endl;
		return ;
	}

	cout << "[gpio-client] Sent end-iof for pin " << (int)pin << endl;
}

bool GpioClient::isIOFactive(gpio::PinNumber pin) {
	return dataChannels.find(pin) != dataChannels.end();
}

void GpioClient::closeIOFunction(gpio::PinNumber pin) {
	auto item = activeIOFs.find(pin);
	cout << "Closing iof of pin " << (int)pin << endl;

	if(item == activeIOFs.end())
		return;

	if(control_channel > 0)
		notifyEndIOFchannel(pin);

	dataChannels.erase(item->second);
	activeIOFs.erase(item);
}

bool GpioClient::registerSPIOnChange(PinNumber pin, OnChange_SPI fun, bool noResponse){
	if(state.pins[pin] != Pinstate::IOF_SPI) {
		cerr << "[gpio-client] WARN: Register SPI onchange on pin " << (int)pin << " with no SPI io-function" << endl;
	}

	DataChannelDescription desc;
	if(!noResponse)
		desc.iof = IOFunction::SPI;
	else
		desc.iof = IOFunction::SPI_NORESPONSE;

	desc.pin = pin;
	desc.onchange.spi = fun;

	return addIOFchannel(desc);
}

bool GpioClient::registerPINOnChange(PinNumber pin, OnChange_PIN fun){
	if(isIOF(state.pins[pin])) {
		cerr << "[gpio-client] WARN: Register PIN onchange on pin " << (int)pin << " with an io-function" << endl;
	}

	DataChannelDescription desc;
	desc.iof = IOFunction::BITSYNC;
	desc.pin = pin;
	desc.onchange.pin = fun;

	return addIOFchannel(desc);
}

bool GpioClient::addIOFchannel(DataChannelDescription desc){
	//cout << "Requesting IOF " << (int)desc.iof << " for pin " << (int)desc.pin << endl;

	lock_guard<std::mutex> guard(dataChannel_m);
	const auto response = requestIOFchannel(desc.pin, desc.iof);

	if(response.port < 1024) {
		cerr << "[gpio-client] [data channel] Server returned invalid port " << (int)response.port << endl;
		return false;
	}

	if(data_channel < 0) {
		char port_c[10]; // may or may not be enough, but we are not planning for failure!
		sprintf(port_c, "%6d", response.port);
		//cout << "Got offered new port " << port_c << " for pin " << (int) desc.pin << endl;
		data_channel = connectToHost(currentHost, port_c);

		dataChannels.emplace(response.id, desc);

		//start handler thread
		iof_dispatcher = std::thread(bind(&GpioClient::handleDataChannel, this));
	} else {
		dataChannels.emplace(response.id, desc);
	}
	//cout << "pin " << (int) desc.pin << " got ID " << (int)response.id << endl;
	return true;
}

void GpioClient::handleDataChannel() {
	IOF_Update update;
	while(readStruct(data_channel, &update)) {
		lock_guard<std::mutex> guard(dataChannel_m);
		auto channel = dataChannels.find(update.id);
		if(channel == dataChannels.end()) {
			cerr << "[gpio-client] [data channel] got non-registered IOF-ID " << (int)update.id << endl;
			// TODO Decide if this is an error condition that resets everything, as we may have lost Sync
			break;
		}
		auto& desc = channel->second;
		switch(desc.iof) {
		case IOFunction::SPI:
		{
			SPI_Response resp = desc.onchange.spi(update.payload.spi);
			if(!writeStruct(data_channel, &resp)) {
				cerr << "[gpio-client] [data channel] Error in SPI write answer" << endl;
				break;
			}
			break;
		}
		case IOFunction::SPI_NORESPONSE:
		{
			SPI_Response resp = desc.onchange.spi(update.payload.spi);
			if(resp != 0) {
				cerr << "[gpio-client] [data channel] Warn: Wrote SPI response '" << (int)resp << "' in NORESPONSE mode" << endl;
				break;
			}
			break;
		}
		case IOFunction::BITSYNC:
		{
			state.pins[desc.pin] = toPinstate(update.payload.pin);
			desc.onchange.pin(update.payload.pin);
			break;
		}

		default:
			cerr << "[gpio-client] [data channel] Unhandled IO-Function " << (int) desc.iof << endl;
			break;
		}
	}
	cerr << "[gpio-client] [data channel] Error or closed socket" << endl;
	closeAndInvalidate(data_channel);
}

GpioClient::Socket GpioClient::connectToHost(const char *host, const char *port) {
	Socket fd = -1;

	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	const int disable_nagle_buffering = 1;

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

		if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &disable_nagle_buffering, sizeof(int)) == -1){
			cerr << "[gpio-client] WARN: setup TCP_NODELAY unsuccessful " << strerror(errno) << endl;
		}

		break;
	}

	freeaddrinfo(servinfo);

	if (p == NULL) {
		//fprintf(stderr, "client: failed to connect\n");
		return -1;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	DEBUG("[gpio-client] connecting to %s\n", s);

	return fd;
}

bool GpioClient::setupConnection(const char *host, const char *port) {
	if((control_channel = connectToHost(host, port)) < 0) {
		//cerr << "[gpio-client] Could not connect to " << host << ":" << port << endl;
		return false;
	}
	currentHost = host;
	return true;
}

void GpioClient::destroyConnection(){
	dataChannels.clear();
	closeAndInvalidate(data_channel);
	closeAndInvalidate(control_channel);
}
