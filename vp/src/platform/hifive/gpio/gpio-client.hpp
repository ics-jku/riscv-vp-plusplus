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
#include <mutex>
#include <iostream>

class GpioClient : public GpioCommon {
public:
	typedef std::function<gpio::SPI_Response(gpio::SPI_Command byte)> OnChange_SPI;
	typedef std::function<void(gpio::Tristate val)> OnChange_PIN;

private:
	typedef int Socket;
	Socket control_channel;
	Socket data_channel;

	const char* currentHost;

	std::thread iof_dispatcher;

	struct DataChannelDescription {
		gpio::IOFunction iof;
		gpio::PinNumber pin;
		// is this a good idea? Not expecting a huge count of open channels
		struct {
		OnChange_SPI spi;
		OnChange_PIN pin;
		} onchange;
	};
	std::unordered_map<gpio::IOF_Channel_ID, DataChannelDescription> dataChannels;
	std::mutex dataChannel_m;
	std::unordered_map<gpio::PinNumber, gpio::IOF_Channel_ID> activeIOFs;

	static void closeAndInvalidate(Socket& fd);

	static Socket connectToHost(const char* host, const char* port);

	// Wrapper for server request
	gpio::Req_IOF_Response requestIOFchannel(gpio::PinNumber pin, gpio::IOFunction iof_type);
	void notifyEndIOFchannel(gpio::PinNumber pin);

	// starts the data channel thread if necessary, and inserts given callback
	bool addIOFchannel(DataChannelDescription desc);

	// Main IOF-Dispatcher
	void handleDataChannel();

public:
	GpioClient();
	~GpioClient();
	bool setupConnection(const char* host, const char* port);
	void destroyConnection();
	bool update();
	bool setBit(gpio::PinNumber pos, gpio::Tristate val);

	// Intended to be used by the external peripherals in simulation
	bool registerSPIOnChange(gpio::PinNumber pin, OnChange_SPI fun, bool noResponse = false);
	bool registerPINOnChange(gpio::PinNumber pin, OnChange_PIN fun = [](gpio::Tristate){});
	// registerI2C...
	// registerUART...

	bool isIOFactive(gpio::PinNumber pin);
	void closeIOFunction(gpio::PinNumber pin);
};
