#include "cdevice.h"

#include <stdexcept>
#include <iostream>

CDevice::CDevice(DeviceID id, PinLayout pin_layout, bool implements_SPI, Config config, Layout graph_layout) : Device(id) {
	pin = std::make_unique<PIN_Interface_C>(pin_layout);
	spi = std::make_unique<SPI_Interface_C>();
	conf = std::make_unique<Config_Interface_C>(config);
	graph = std::make_unique<Graphbuf_Interface_C>(graph_layout);
}

CDevice::~CDevice() {}

/* PIN Interface */

CDevice::PIN_Interface_C::PIN_Interface_C(PinLayout layout) : layout(layout) {

}

CDevice::PIN_Interface_C::~PIN_Interface_C() {}

PinLayout CDevice::PIN_Interface_C::getPinLayout() {
	return layout;
}

void CDevice::PIN_Interface_C::setPin(PinNumber num, bool val) {
	std::cout << "Warning: setPin was not implemented" << std::endl;
}

bool CDevice::PIN_Interface_C::getPin(PinNumber num) {
	std::cout << "Warning: getPin was not implemented" << std::endl;
	return false;
}

/* SPI Interface */

CDevice::SPI_Interface_C::SPI_Interface_C() {}
CDevice::SPI_Interface_C::~SPI_Interface_C() {}

uint8_t CDevice::SPI_Interface_C::send(uint8_t byte) {
	return 0;
}

/* Config Interface */

CDevice::Config_Interface_C::Config_Interface_C(Config config) : config(config) {}

CDevice::Config_Interface_C::~Config_Interface_C() {}

Config CDevice::Config_Interface_C::getConfig() {
	return config;
}

bool CDevice::Config_Interface_C::setConfig(const Config conf) {
	config = conf;
	return true;
}

/* Graph Buf Interface */

CDevice::Graphbuf_Interface_C::Graphbuf_Interface_C(Layout layout) : layout(layout) {}
CDevice::Graphbuf_Interface_C::~Graphbuf_Interface_C() {}

Layout CDevice::Graphbuf_Interface_C::getLayout() { return layout; }
void CDevice::Graphbuf_Interface_C::initializeBufferMaybe() {
	std::cout << "Warning: initialize graph buffer was not implemented" << std::endl;
}

void CDevice::Graphbuf_Interface_C::registerSetBuf(const SetBuf_fn setBuf) {
	this->set_buf = setBuf;
}
void CDevice::Graphbuf_Interface_C::registerGetBuf(const GetBuf_fn getBuf) {
	this->get_buf = getBuf;
}
