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

	void handleSPIchannel(int socket, OnChange_SPI fun);

	// @return filedescriptor, -1 else
	static int connectToHost(const char* host, const char* port);

public:
	GpioClient();
	~GpioClient();
	bool setupConnection(const char* host, const char* port);
	bool update();
	bool setBit(gpio::PinNumber pos, gpio::Tristate val);

	//TODO: onchange functions not yet stable
	bool registerSPIOnChange(gpio::PinNumber pin, OnChange_SPI fun);
	bool registerPINOnChange(gpio::PinNumber pin, OnChange_PIN fun);

	void closeIOFunction(gpio::PinNumber pin);
};
