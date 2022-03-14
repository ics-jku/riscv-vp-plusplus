/*
 * gpio-client.hpp
 *
 *  Created on: 7 Nov 2018
 *      Author: dwd
 */

#pragma once

#include "gpiocommon.hpp"
#include <functional>

class GpioClient : public GpioCommon {
public:
	typedef std::function<gpio::SPI_Response(gpio::SPI_Command byte)> OnChange_SPI;
	typedef std::function<void(gpio::Tristate val)> OnChange_PIN;

private:
	int fd;

public:
	GpioClient();
	~GpioClient();
	bool setupConnection(const char* host, const char* port);
	bool update();
	bool setBit(gpio::PinNumber pos, gpio::Tristate val);

	//TODO: update functions not yet stable
	bool registerSPIOnChange(OnChange_SPI fun);
	bool registerPINOnChange(OnChange_PIN fun);
};
