#include "cfactory.h"

#include "devices/c/all_devices.hpp"

void CFactory::printAvailableDevices() {
	std::cout << "Available Devices: " << std::endl;
	std::cout << "sevensegment" << std::endl;
	std::cout << "oled" << std::endl;
	std::cout << "button" << std::endl;
}

bool CFactory::deviceExists(DeviceClass classname) {
	return classname == "sevensegment" || classname == "oled" || classname == "button";
}

CDevice* CFactory::instantiateDevice(DeviceID id, DeviceClass classname) {
	if(!deviceExists(classname)) {
		throw (std::runtime_error("Device " + classname + " does not exist"));
	}
	if(classname == "sevensegment") {
		return new Sevensegment(id);
	}
	else if(classname == "oled") {
		return new OLED(id);
	}
	else if(classname == "button") {
		return new Button(id);
	}
	return nullptr;
}
