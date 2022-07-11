#include "cdevice.h"

#include <stdexcept>
#include <iostream>

CDevice::CDevice(DeviceID id) : Device(id) {}
CDevice::~CDevice() {}

void CDevice::setPIN_Interface(PinLayout layout) {
	if(!pin) {
		pin = std::make_unique<PIN_Interface_C>(this);
		layout_pin = layout;
	}
}

void CDevice::setSPI_Interface() {
	if(!spi) {
		spi = std::make_unique<SPI_Interface_C>(this);
	}
}

void CDevice::setConfig_Interface(Config config) {
	if(!conf) {
		conf = std::make_unique<Config_Interface_C>(this);
		this->config = config;
	}
}

void CDevice::setGraphbuf_Interface(Layout layout) {
	if(!graph) {
		graph = std::make_unique<Graphbuf_Interface_C>(this);
		layout_graph = layout;
	}
}

/* PIN Interface */

CDevice::PIN_Interface_C::PIN_Interface_C(CDevice* device) : device(device) {}
CDevice::PIN_Interface_C::~PIN_Interface_C() {}

PinLayout CDevice::PIN_Interface_C::getPinLayout() {
	return device->layout_pin;
}

void CDevice::PIN_Interface_C::setPin(PinNumber num, bool val) {
	std::cout << "Warning: setPin was not implemented" << std::endl;
}

bool CDevice::PIN_Interface_C::getPin(PinNumber num) {
	std::cout << "Warning: getPin was not implemented" << std::endl;
	return false;
}

/* SPI Interface */

CDevice::SPI_Interface_C::SPI_Interface_C(CDevice* device) : device(device) {}
CDevice::SPI_Interface_C::~SPI_Interface_C() {}

uint8_t CDevice::SPI_Interface_C::send(uint8_t byte) {
	return 0;
}

/* Config Interface */

CDevice::Config_Interface_C::Config_Interface_C(CDevice* device) : device(device) {}
CDevice::Config_Interface_C::~Config_Interface_C() {}

Config CDevice::Config_Interface_C::getConfig() {
	return device->config;
}

bool CDevice::Config_Interface_C::setConfig(const Config conf) {
	device->config = conf;
	return true;
}

/* Graph Buf Interface */

CDevice::Graphbuf_Interface_C::Graphbuf_Interface_C(CDevice* device) : device(device) {}
CDevice::Graphbuf_Interface_C::~Graphbuf_Interface_C() {}

Layout CDevice::Graphbuf_Interface_C::getLayout() { return device->layout_graph; }
void CDevice::Graphbuf_Interface_C::initializeBufferMaybe() {
	std::cout << "Warning: initialize graph buffer was not implemented" << std::endl;
}

void CDevice::Graphbuf_Interface_C::registerSetBuf(const SetBuf_fn setBuf) {
	device->set_buf = setBuf;
}
void CDevice::Graphbuf_Interface_C::registerGetBuf(const GetBuf_fn getBuf) {
	device->get_buf = getBuf;
}
