/*
 * gpio-client.hpp
 *
 *  Created on: 7 Nov 2018
 *      Author: dwd
 */

#pragma once

#include "gpiocommon.hpp"
#include <functional>
#include <unordered_map>
#include <thread>
#include <iostream>

class GpioClient : public GpioCommon {
public:
	typedef std::function<gpio::SPI_Response(gpio::SPI_Command byte)> OnChange_SPI;
	typedef std::function<void(gpio::Tristate val)> OnChange_PIN;

private:
	int server_connection;
	const char* currentHost;

	struct DataChannelDescription {
		int fd;
		std::thread thread;
	};
	std::unordered_map<gpio::PinNumber, DataChannelDescription> dataChannelThreads;

	// TODO: Add IOF-parameter as specific request
	// @return port number, 0 on error
	uint16_t requestIOFport(gpio::PinNumber pin);

	template<typename OnChange_handler>
		bool setupIOFhandler(gpio::PinNumber pin, OnChange_handler onChange);
	void notifyEndIOFchannel(gpio::PinNumber pin);

	// @return filedescriptor, < 0 on error
	static int connectToHost(const char* host, const char* port);


	void handleSPIchannel(gpio::PinNumber pin, OnChange_SPI fun);
	void handlePINchannel(gpio::PinNumber pin, OnChange_PIN fun);

public:
	GpioClient();
	~GpioClient();
	bool setupConnection(const char* host, const char* port);
	void destroyConnection();
	bool update();
	bool setBit(gpio::PinNumber pos, gpio::Tristate val);

	// Intended to be used by the external peripherals in simulation
	// TODO: Somehow unify for code deduplication
	bool registerSPIOnChange(gpio::PinNumber pin, OnChange_SPI fun);
	bool registerPINOnChange(gpio::PinNumber pin, OnChange_PIN fun = [](gpio::Tristate){});
	// registerI2C...
	// registerUART...

	bool isIOFactive(gpio::PinNumber pin);
	void closeIOFunction(gpio::PinNumber pin);
};

template<typename OnChange_handler>
bool GpioClient::setupIOFhandler(gpio::PinNumber pin, OnChange_handler onChange) {
	if(isIOFactive(pin)) {
		std::cerr << "[gpio-client] Pin " << (int)pin << " was already registered" << std::endl;
		return false;
	}

	auto port = requestIOFport(pin);

	if(port == 0) {
		std::cerr << "[gpio-client] IOF port request unsuccessful" << std::endl;
		return false;
	}

	char port_c[10]; // may or may not be enough, but we are not planning for failure!
	sprintf(port_c, "%6d", port);

	//cout << "Got offered port " << port_c << " for pin " << (int) pin << endl;

	int dataChannel = connectToHost(currentHost, port_c);
	if(dataChannel < 0) {
		std::cerr << "[gpio-client] Could not connect to offered port " << currentHost << ":" << port_c << std::endl;
		return false;
	}

	dataChannelThreads.emplace(pin,
			DataChannelDescription{dataChannel, std::thread(onChange)}
	);
	return true;
}
