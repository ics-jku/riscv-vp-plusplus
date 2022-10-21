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
	std::cerr << "[CDevice] Warning: setPin was not implemented "
			"for device " << device->getClass() << "." << std::endl;
}

gpio::Tristate CDevice::PIN_Interface_C::getPin(PinNumber num) {
	std::cerr << "[CDevice] Warning: getPin was not implemented "
			"for device " << device->getClass() << "." << std::endl;
	return gpio::Tristate::LOW;
}

/* SPI Interface */

CDevice::SPI_Interface_C::SPI_Interface_C(CDevice* device) : device(device) {}
CDevice::SPI_Interface_C::~SPI_Interface_C() {}

gpio::SPI_Response CDevice::SPI_Interface_C::send(gpio::SPI_Command byte) {
	std::cerr << "[CDevice] Warning: SPI::send was not implemented "
			"for device " << device->getClass() << "." << std::endl;
	return 0;
}

/* Config Interface */

CDevice::Config_Interface_C::Config_Interface_C(CDevice* device) : device(device) {}
CDevice::Config_Interface_C::~Config_Interface_C() {}

Config CDevice::Config_Interface_C::getConfig() {
	return config;
}

bool CDevice::Config_Interface_C::setConfig(Config conf) {
	config = conf;
	return true;
}

/* Graph Buf Interface */

CDevice::Graphbuf_Interface_C::Graphbuf_Interface_C(CDevice* device) : device(device) {}
CDevice::Graphbuf_Interface_C::~Graphbuf_Interface_C() {}

Layout CDevice::Graphbuf_Interface_C::getLayout() { return layout; }
void CDevice::Graphbuf_Interface_C::initializeBuffer() {
	std::cerr << "[CDevice] Warning: initialize graph buffer was not "
			"implemented for device " << device->getClass() << "." << std::endl;
}

/* Input interface */

CDevice::Input_Interface_C::Input_Interface_C(CDevice* device) : device(device) {}
CDevice::Input_Interface_C::~Input_Interface_C() {}

void CDevice::Input_Interface_C::onClick(bool active) {
	std::cerr << "[CDevice] Warning: onClick was not implemented "
			"for device " << device->getClass() << "." << std::endl;
}

void CDevice::Input_Interface_C::onKeypress(Key key, bool active) {
	std::cerr << "[CDevice] Warning: onKeypress was not implemented "
			"for device " << device->getClass() << "." << std::endl;
}
