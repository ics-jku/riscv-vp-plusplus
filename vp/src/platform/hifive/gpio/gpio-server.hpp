/*
 * gpio-server.hpp
 *
 *  Created on: 7 Nov 2018
 *      Author: dwd
 */

#pragma once

#include "gpiocommon.hpp"

#include <functional>
#include <atomic>

class GpioServer : public GpioCommon {
public:
	typedef std::function<void(gpio::PinNumber pin, gpio::Tristate val)> UpdateFunction;
private:
	int listener_fd;
	int current_connection_fd;
	const char *port;
	std::atomic<bool> stop;
	UpdateFunction fun;
	void handleConnection(int conn);

   public:
	GpioServer();
	~GpioServer();
	bool setupConnection(const char* port);
	void quit();
	bool isStopped();
	void registerOnChange(UpdateFunction fun);
	void startListening();

	// pin number may be CS? If that works.
	gpio::SPI_Response pushSPI(gpio::PinNumber pin, gpio::SPI_Command byte);
};
