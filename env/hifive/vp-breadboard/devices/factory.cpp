#include "factory.h"
#include <iostream>

void Factory::scanAdditionalDir(std::string dir, bool overwrite_existing) {
	lua_factory.scanAdditionalDir(dir, overwrite_existing);
}

void Factory::printAvailableDevices() {
	std::cout << "All Available Devices: " << std::endl;
	std::cout << "LUA: " << std::endl;
	lua_factory.printAvailableDevices();
}

bool Factory::deviceExists(DeviceClass classname) {
	return lua_factory.deviceExists(classname);
}

Device* Factory::instantiateDevice(DeviceID id, DeviceClass classname) {
	return lua_factory.instantiateDevice(id, classname);
}
