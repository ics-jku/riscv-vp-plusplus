#include "cFactory.h"

void CFactory::printAvailableDevices() {
	std::cout << "Available Devices: " << std::endl;
	for(std::pair<DeviceClass, Creator> device : deviceCreators) {
		std::cout << device.first << std::endl;
	}
}

bool CFactory::deviceExists(DeviceClass classname) {
	return deviceCreators.find(classname) != deviceCreators.end();
}

std::unique_ptr<CDevice> CFactory::instantiateDevice(DeviceID id, DeviceClass classname) {
	if(!deviceExists(classname)) {
		throw (std::runtime_error("Device " + classname + " does not exist"));
	}
	else {
		return deviceCreators.find(classname)->second(id);
	}
}

CFactory& getCFactory() { static CFactory CF; return CF; }
