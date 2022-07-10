#include "device.hpp"

Device::Device(const DeviceID id) : m_id(id) {}

Device::~Device() {}

const DeviceID& Device::getID() const {
	return m_id;
}

Device::PIN_Interface::~PIN_Interface() {}

Device::SPI_Interface::~SPI_Interface() {}

Device::Config_Interface::~Config_Interface() {}

Device::Graphbuf_Interface::~Graphbuf_Interface() {}
