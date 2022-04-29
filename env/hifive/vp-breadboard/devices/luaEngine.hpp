/*
 * scriptloader.hpp
 *
 *  Created on: Apr 29, 2022
 *      Author: pp
 */

#pragma once

#include <iostream>
#include <string>
#include <list>

#include "device.hpp"

class LuaEngine {
	const std::string builtin_scripts = ":/devices/lua/";
	const std::string scriptloader = ":/devices/scriptloader.lua";
public:

	LuaEngine();

	void scanAdditionalDir(std::string dir);

	bool deviceExists(std::string name);
	Device& instantiateDevice(std::string name);
};

