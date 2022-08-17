/*
 * scriptloader.hpp
 *
 *  Created on: Apr 29, 2022
 *      Author: pp
 */

#pragma once

#include <iostream>
#include <string>
#include <unordered_map>

#include "devices/interface/luaDevice.hpp"

class LuaEngine {
	const std::string builtin_scripts = ":/devices/lua/";
	const std::string scriptloader = ":/devices/factory/loadscript.lua";

	std::unordered_map<std::string,std::string> available_devices;
public:

	LuaEngine();

	void scanAdditionalDir(std::string dir, bool overwrite_existing = false);
	void printAvailableDevices();

	bool deviceExists(DeviceClass classname);
	LuaDevice* instantiateDevice(DeviceID id, DeviceClass classname);
};

