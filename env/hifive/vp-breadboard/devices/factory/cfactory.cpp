#include "cfactory.h"

bool CFactory::registerDeviceType(DeviceClass classname, Creator creator) {
	return devices.insert(std::make_pair(classname, creator)).second;
}

void CFactory::printAvailableDevices() {
	std::cout << "Available Devices: " << std::endl;
	for(std::pair<DeviceClass, Creator> device : devices) {
		std::cout << device.first << std::endl;
	}
}

bool CFactory::deviceExists(DeviceClass classname) {
	return devices.find(classname) != devices.end();
}

CDevice* CFactory::instantiateDevice(DeviceID id, DeviceClass classname) {
	if(!deviceExists(classname)) {
		throw (std::runtime_error("Device " + classname + " does not exist"));
	}
	else {
		return devices.find(classname)->second(id);
	}
}

CFactory& getCFactory() { static CFactory CF; return CF; }
