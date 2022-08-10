#pragma once

#include "luaEngine.hpp"
#include "cfactory.h"


class Factory {
	LuaEngine lua_factory;
	CFactory c_factory;

public:

	void scanAdditionalDir(std::string dir, bool overwrite_existing = false);
	void printAvailableDevices();

	bool deviceExists(DeviceClass classname);
	Device* instantiateDevice(DeviceID id, DeviceClass classname);
};
