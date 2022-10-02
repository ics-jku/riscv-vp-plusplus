#include <stdexcept>
#include <iostream>
#include "cDevice.h"

CDevice::CDevice(DeviceID id) : Device(id) {}
CDevice::~CDevice() {}

/* PIN Interface */

CDevice::PIN_Interface_C::PIN_Interface_C(CDevice* device) : device(device) {}
CDevice::PIN_Interface_C::~PIN_Interface_C() {}

PinLayout CDevice::PIN_Interface_C::getPinLayout() {
	return layout;
}

void CDevice::PIN_Interface_C::setPin(PinNumber num, gpio::Tristate val) {
	std::cout << "Warning: setPin was not implemented" << std::endl;
}

gpio::Tristate CDevice::PIN_Interface_C::getPin(PinNumber num) {
	std::cout << "Warning: getPin was not implemented" << std::endl;
	return gpio::Tristate::LOW;
}

/* SPI Interface */

CDevice::SPI_Interface_C::SPI_Interface_C(CDevice* device) : device(device) {}
CDevice::SPI_Interface_C::~SPI_Interface_C() {}

gpio::SPI_Response CDevice::SPI_Interface_C::send(gpio::SPI_Command byte) {
	return 0;
}

/* Config Interface */

CDevice::Config_Interface_C::Config_Interface_C(CDevice* device) : device(device) {}
CDevice::Config_Interface_C::~Config_Interface_C() {}

Config* CDevice::Config_Interface_C::getConfig() {
	return config;
}

bool CDevice::Config_Interface_C::setConfig(Config* conf) {
	config = conf;
	return true;
}

/* Graph Buf Interface */

CDevice::Graphbuf_Interface_C::Graphbuf_Interface_C(CDevice* device) : device(device) {}
CDevice::Graphbuf_Interface_C::~Graphbuf_Interface_C() {}

Layout CDevice::Graphbuf_Interface_C::getLayout() { return layout; }
void CDevice::Graphbuf_Interface_C::initializeBuffer() {
	std::cout << "Warning: initialize graph buffer was not implemented" << std::endl;
}

/* Input interface */

CDevice::Input_Interface_C::Input_Interface_C(CDevice* device) : device(device) {}
CDevice::Input_Interface_C::~Input_Interface_C() {}

void CDevice::Input_Interface_C::onClick(bool active) {
	std::cout << "Warning: mouse was not implemented" << std::endl;
}

void CDevice::Input_Interface_C::onKeypress(Key key, bool active) {
	std::cout << "Warning: key was not implemented" << std::endl;
}
