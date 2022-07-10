#pragma once

#include "luaEngine.hpp"
#include "cdevice.h"


class Factory {
	LuaEngine lua_factory;
	// TODO C factory

public:

	void scanAdditionalDir(std::string dir, bool overwrite_existing = false);
	void printAvailableDevices();

	bool deviceExists(DeviceClass classname);
	Device* instantiateDevice(DeviceID id, DeviceClass classname);
};
