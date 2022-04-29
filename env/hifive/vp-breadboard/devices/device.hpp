/*
 * devices.hpp
 *
 *  Created on: Sep 29, 2021
 *      Author: dwd
 */

#pragma once

#include <string>
#include <map>

extern "C"
{
	#if __has_include(<lua5.3/lua.h>)
		#include <lua5.3/lua.h>
		#include <lua5.3/lualib.h>
		#include <lua5.3/lauxlib.h>
	#elif  __has_include(<lua.h>)
		#include <lua.h>
		#include <lualib.h>
		#include <lauxlib.h>
	#else
		#error("No lua libraries found")
	#endif
}
#include <LuaBridge/LuaBridge.h>

#include <cstring>

class Device {
	std::string name;
	luabridge::LuaRef env;

public:

	struct ConfigElem {
		enum class Type {
			invalid = 0,
			integer,
			boolean,
			//string,
		} type;
		union Value {
			int64_t integer;
			bool boolean;
		} value;

		ConfigElem() : type(Type::invalid){};

		ConfigElem(int64_t val){
			type = Type::integer;
			value.integer = val;
		};
		ConfigElem(bool val) {
			type = Type::boolean;
			value.boolean = val;
		};
	};

	const std::string& getName() const;

	typedef std::map<std::string,ConfigElem> Config;
	Config getConfig();

	bool setConfig(const Config conf);

	friend class SPI_Interface;

	class SPI_Interface{
		luabridge::LuaRef m_setCS;
		luabridge::LuaRef m_send;

	public:

		SPI_Interface(luabridge::LuaRef& ref);
		void setCS(bool val);
		uint8_t send(uint8_t byte);
		static bool implementsInterface(const luabridge::LuaRef& ref);
	};

	std::unique_ptr<SPI_Interface> spi;
	// TODO: others?

	Device(std::string name, luabridge::LuaRef env);
};

