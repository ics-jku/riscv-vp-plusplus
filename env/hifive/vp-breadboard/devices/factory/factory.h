#pragma once

#include "cFactory.h"
#include "luaFactory.hpp"


class Factory {
	LuaEngine lua_factory;
	CFactory c_factory = getCFactory();

public:
	void scanAdditionalDir(std::string dir, bool overwrite_existing = false);
	void printAvailableDevices();

	bool deviceExists(DeviceClass classname);
	Device* instantiateDevice(DeviceID id, DeviceClass classname);
};
