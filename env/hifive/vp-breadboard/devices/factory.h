#pragma once

#include "luaEngine.hpp"


class Factory {
	LuaEngine lua_factory;
	// TODO C factory

public:

	void scanAdditionalDir(std::string dir, bool overwrite_existing = false);
	void printAvailableDevices();

	bool deviceExists(std::string classname);
	Device* instantiateDevice(std::string id, std::string classname);
};
