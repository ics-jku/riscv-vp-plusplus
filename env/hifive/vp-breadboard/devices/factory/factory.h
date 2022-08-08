#pragma once

#include "luaEngine.hpp"
#include "devices/c/all_devices.hpp"


class Factory {
	LuaEngine lua_factory;

public:

	void scanAdditionalDir(std::string dir, bool overwrite_existing = false);
	void printAvailableDevices();

	bool deviceExists(DeviceClass classname);
	Device* instantiateDevice(DeviceID id, DeviceClass classname);
};
