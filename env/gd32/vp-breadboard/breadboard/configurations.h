#pragma once

#include <QImage>
#include <QPoint>
#include <gpio/gpio-client.hpp>

#include "devices/factory/factory.h"

struct SPI_IOF_Request {
	gpio::PinNumber global_pin;
	gpio::Port port;
	bool noresponse;
	GpioClient::OnChange_SPI fun;
};

struct PIN_IOF_Request {
	gpio::PinNumber global_pin;
	gpio::PinNumber device_pin;
	gpio::Port port;
	GpioClient::OnChange_PIN fun;
};

struct EXMC_IOF_Request {
	gpio::PinNumber global_pin;
	gpio::Port port;
	GpioClient::OnChange_EXMC fun;
};

struct DeviceGraphic {
	QImage image;
	QPoint offset;
	unsigned scale;
};

struct PinMapping {
	gpio::PinNumber global_pin;
	gpio::PinNumber device_pin;
	gpio::Port port;
	std::string name;
	Device* dev;
};
