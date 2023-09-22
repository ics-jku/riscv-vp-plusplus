#pragma once

#include "cFactory.h"
#include "luaFactory.hpp"

class Factory {
	LuaFactory lua_factory;
	CFactory c_factory = getCFactory();

   public:
	void scanAdditionalDir(std::string dir, bool overwrite_existing = false);
	void printAvailableDevices();

	bool deviceExists(DeviceClass classname);
	std::unique_ptr<Device> instantiateDevice(DeviceID id, DeviceClass classname);
};
