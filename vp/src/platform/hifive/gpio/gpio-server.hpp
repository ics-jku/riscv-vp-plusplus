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
	int listener_fd;
	int current_connection_fd;
	const char *port;
	std::atomic<bool> stop;
	std::function<void(uint8_t bit, Tristate val)> fun;
	void handleConnection(int conn);

   public:
	GpioServer();
	~GpioServer();
	bool setupConnection(const char* port);
	void quit();
	bool isStopped();
	void registerOnChange(std::function<void(uint8_t bit, Tristate val)> fun);
	void startListening();
};
