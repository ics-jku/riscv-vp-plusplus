#pragma once

#include <gpio/gpio-client.hpp>
#include <QImage>
#include <QPoint>

#include "devices/factory/factory.h"

typedef unsigned Row;
typedef unsigned Index;

const unsigned BB_ROWS = 80;
const unsigned BB_ONE_ROW = BB_ROWS/2;
const unsigned BB_INDEXES = 5;

const QString DEFAULT_PATH = ":/img/virtual_breadboard.png";
const QSize DEFAULT_SIZE = QSize(486, 233);

const QString DEVICE_DRAG_TYPE = "device";

const unsigned BB_ICON_SIZE = DEFAULT_SIZE.width()/BB_ONE_ROW;
const unsigned BB_ROW_X = 5;
const unsigned BB_ROW_Y = 50;

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
