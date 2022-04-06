/*
 * gpio-client.hpp
 *
 *  Created on: 7 Nov 2018
 *      Author: dwd
 */

#pragma once

#include "gpiocommon.hpp"
#include <functional>
#include <list>
#include <thread>

class GpioClient : public GpioCommon {
public:
	typedef std::function<gpio::SPI_Response(gpio::SPI_Command byte)> OnChange_SPI;
	typedef std::function<void(gpio::Tristate val)> OnChange_PIN;

private:
	int fd;
	const char* currentHost;

	struct DataChannelDescription {
		gpio::PinNumber pin;
		int fd;
		std::thread thread;
	};
	std::list<DataChannelDescription> dataChannelThreads;

	// todo: Templateify? (command, response)
	void handleSPIchannel(int socket, OnChange_SPI fun);

	// @return port number, 0 on error
	uint16_t requestIOFchannel(gpio::PinNumber pin);
	void stopIOFchannel(gpio::PinNumber pin);
	// @return filedescriptor, < 0 on error
	static int connectToHost(const char* host, const char* port);

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
	bool registerPINOnChange(gpio::PinNumber pin, OnChange_PIN fun);
	// registerI2C...
	// registerUART...

	void closeIOFunction(gpio::PinNumber pin);
};
