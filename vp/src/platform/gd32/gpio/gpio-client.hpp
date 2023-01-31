/*
 * gpio-client.hpp
 *
 *  Created on: 7 Nov 2018
 *      Author: dwd
 */

#pragma once

#include <functional>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "gpiocommon.hpp"

class GpioClient : public GpioCommon {
   public:
	typedef std::function<gpio::SPI_Response(gpio::SPI_Command byte)> OnChange_SPI;
	typedef std::function<void(gpio::Tristate val)> OnChange_PIN;
	typedef std::function<gpio::EXMC_Data(gpio::EXMC_Data data)> OnChange_EXMC;

   private:
	typedef int Socket;
	Socket control_channel;
	Socket data_channel;

	const char* currentHost;

	std::thread iof_dispatcher;

	struct IOFChannelDescription {
		gpio::IOFunction iof;
		gpio::PinNumber pin;
		// This is somewhat memory wasteful as only one of the onchanges are used,
		// but we are not expecting a huge count of open channels.
		struct {
			OnChange_SPI spi;
			OnChange_PIN pin;
			OnChange_EXMC exmc;
		} onchange;
	};
	std::unordered_map<gpio::IOF_Channel_ID, IOFChannelDescription> activeIOFs;
	std::mutex activeIOFs_m;

	static void closeAndInvalidate(Socket& fd);

	static Socket connectToHost(const char* host, const char* port);

	// Wrapper for server request
	gpio::Req_IOF_Response requestIOFchannel(gpio::PinNumber pin, gpio::IOFunction iof_type);
	void notifyEndIOFchannel(gpio::PinNumber pin);

	// starts the data channel thread if necessary, and inserts given callback
	bool addIOFchannel(IOFChannelDescription desc);

	// Main IOF-Dispatcher
	void handleDataChannel();

   public:
	GpioClient();
	~GpioClient();
	bool setupConnection(const char* host, const char* port);
	void destroyConnection();
	bool update();
	bool setBit(gpio::PinNumber pos, gpio::Tristate val);

	// Intended to be used by the devices in the environment model
	bool registerSPIOnChange(gpio::PinNumber pin, OnChange_SPI fun, bool noResponse = false);
	bool registerPINOnChange(
	    gpio::PinNumber pin, OnChange_PIN fun = [](gpio::Tristate) {});
	// registerI2C...
	// registerUART...
	bool registerEXMCOnChange(gpio::PinNumber pin, OnChange_EXMC fun);

	bool isIOFactive(gpio::PinNumber pin);
	void closeIOFunction(gpio::PinNumber pin);
};
