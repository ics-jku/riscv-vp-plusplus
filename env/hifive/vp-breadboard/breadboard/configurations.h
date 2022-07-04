#pragma once

#include <gpio/gpio-client.hpp>
#include <QImage>
#include <QPoint>

#include "devices/luaEngine.hpp"

struct SPI_IOF_Request {
	gpio::PinNumber gpio_offs;	// calculated from "global pin"
	gpio::PinNumber global_pin;
	bool noresponse;
	GpioClient::OnChange_SPI fun;
};

struct PIN_IOF_Request {
	gpio::PinNumber gpio_offs;	// calculated from "global pin"
	gpio::PinNumber global_pin;
	GpioClient::OnChange_PIN fun;
};

struct DeviceGraphic {
	QImage image;
	QPoint offset;
	unsigned scale;
};

struct PinMapping{
	gpio::PinNumber gpio_offs;	// calculated from "global pin"
	gpio::PinNumber global_pin;
	gpio::PinNumber device_pin;
	std::string name;
	Device* dev;
};
