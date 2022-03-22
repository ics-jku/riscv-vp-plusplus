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
#include <map>

class GpioServer : public GpioCommon {
public:
	typedef std::function<void(gpio::PinNumber pin, gpio::Tristate val)> OnChangeCallback;


private:
	int listener_socket_fd;
	int current_connection_fd;
	const char *base_port;
	std::atomic<bool> stop;
	OnChangeCallback fun;
	void handleConnection(int conn);

	std::map<gpio::PinNumber,int> active_IOF_channels;

	static int openSocket(const char* port);

	static int awaitConnection(int socket);

public:
	GpioServer();
	~GpioServer();
	bool setupConnection(const char* port);
	void quit();
	bool isStopped();
	void registerOnChange(OnChangeCallback fun);
	void startAccepting();

	// pin number may be CS? If that works.
	gpio::SPI_Response pushSPI(gpio::PinNumber pin, gpio::SPI_Command byte);
};
