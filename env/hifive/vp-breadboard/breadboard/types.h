#pragma once

#include <gpio/gpio-client.hpp>
#include "devices/device.hpp"

typedef unsigned Row;
typedef unsigned Index;

struct SPI_IOF_Request {
	gpio::PinNumber gpio_offs;	// calculated from "global pin"
	gpio::PinNumber global_pin;
	bool noresponse;
	GpioClient::OnChange_SPI fun;
};

struct PIN_IOF_Request {
	gpio::PinNumber gpio_offs;	// calculated from "global pin"
	gpio::PinNumber global_pin;
	gpio::PinNumber device_pin;
	GpioClient::OnChange_PIN fun;
};

struct PinMapping{
	gpio::PinNumber gpio_offs;	// calculated from "global pin"
	gpio::PinNumber global_pin;
	gpio::PinNumber device_pin;
	std::string name;
	Device* dev;
};
