#include "factory.h"

#include <iostream>

#include "configurations.h"

void Factory::scanAdditionalDir(std::string dir, bool overwrite_existing) {
	lua_factory.scanAdditionalDir(dir, overwrite_existing);
}

void Factory::printAvailableDevices() {
	std::cout << "All Available Devices: " << std::endl;
	std::cout << "LUA: " << std::endl;
	lua_factory.printAvailableDevices();
	std::cout << "C: " << std::endl;
	c_factory.printAvailableDevices();
}

bool Factory::deviceExists(DeviceClass classname) {
	return lua_factory.deviceExists(classname) || c_factory.deviceExists(classname);
}

std::unique_ptr<Device> Factory::instantiateDevice(DeviceID id, DeviceClass classname) {
	if (c_factory.deviceExists(classname))
		return c_factory.instantiateDevice(id, classname);
	else if (lua_factory.deviceExists(classname))
		return lua_factory.instantiateDevice(id, classname);
	else
		throw(device_not_found_error(classname));
}
