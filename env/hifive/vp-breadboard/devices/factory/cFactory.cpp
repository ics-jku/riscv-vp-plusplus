#include "cFactory.h"

#include "configurations.h"

std::list<DeviceClass> CFactory::getAvailableDevices() {
	std::list<DeviceClass> devices;
	for(auto const& [classname, creator] : deviceCreators) {
		devices.push_back(classname);
	}
	return devices;
}

bool CFactory::deviceExists(DeviceClass classname) {
	return deviceCreators.find(classname) != deviceCreators.end();
}

std::unique_ptr<CDevice> CFactory::instantiateDevice(DeviceID id, DeviceClass classname) {
	if(!deviceExists(classname)) {
		throw (device_not_found_error(classname));
	}
	else {
		return deviceCreators.find(classname)->second(id);
	}
}

CFactory& getCFactory() { static CFactory CF; return CF; }
