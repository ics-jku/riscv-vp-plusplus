/*
 * devices.hpp
 *
 *  Created on: Sep 29, 2021
 *      Author: dwd
 */

#pragma once

extern "C" {
#if __has_include(<lua5.3/lua.h>)
#include <lua5.3/lauxlib.h>
#include <lua5.3/lua.h>
#include <lua5.3/lualib.h>
#elif __has_include(<lua.h>)
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#else
#error("No lua libraries found")
#endif
}
#include <LuaBridge/LuaBridge.h>

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "devices/device.hpp"

class LuaDevice : public Device {
	luabridge::LuaRef m_env;

   public:
	const DeviceClass getClass() const;

	class PIN_Interface_Lua : public Device::PIN_Interface {
		luabridge::LuaRef m_getPinLayout;
		luabridge::LuaRef m_getPin;
		luabridge::LuaRef m_setPin;

	   public:
		PIN_Interface_Lua(luabridge::LuaRef& ref);
		~PIN_Interface_Lua();
		PinLayout getPinLayout();
		gpio::Tristate getPin(PinNumber num);
		void setPin(PinNumber num, gpio::Tristate val);
		static bool implementsInterface(const luabridge::LuaRef& ref);
	};

	class SPI_Interface_Lua : public Device::SPI_Interface {
		luabridge::LuaRef m_send;

	   public:
		SPI_Interface_Lua(luabridge::LuaRef& ref);
		~SPI_Interface_Lua();
		gpio::SPI_Response send(gpio::SPI_Command byte);
		static bool implementsInterface(const luabridge::LuaRef& ref);
	};

	class Config_Interface_Lua : public Device::Config_Interface {
		luabridge::LuaRef m_getConf;
		luabridge::LuaRef m_setConf;
		luabridge::LuaRef& m_env;  // for building table
	   public:
		Config_Interface_Lua(luabridge::LuaRef& ref);
		~Config_Interface_Lua();
		Config* getConfig();
		bool setConfig(Config* conf);
		static bool implementsInterface(const luabridge::LuaRef& ref);
	};

	class Graphbuf_Interface_Lua : public Device::Graphbuf_Interface {
		luabridge::LuaRef m_getGraphBufferLayout;
		luabridge::LuaRef m_initializeGraphBuffer;
		luabridge::LuaRef m_env;
		DeviceID m_deviceId;  // Redundant to Device's ID member
		lua_State* L;         // to register functions and Format

		static void declarePixelFormat(lua_State* L);

	   public:
		Graphbuf_Interface_Lua(luabridge::LuaRef& ref, DeviceID device_id, lua_State* L);
		~Graphbuf_Interface_Lua();
		Layout getLayout();
		void initializeBufferMaybe();
		void registerBuffer(QImage& image);

		template <typename FunctionFootprint>
		void registerGlobalFunctionAndInsertLocalAlias(const std::string name, FunctionFootprint fun);

		static bool implementsInterface(const luabridge::LuaRef& ref);
	};

	class Input_Interface_Lua : public Device::Input_Interface {
		luabridge::LuaRef m_onClick;
		luabridge::LuaRef m_onKeypress;
		luabridge::LuaRef& m_env;  // for building table
	   public:
		Input_Interface_Lua(luabridge::LuaRef& ref);
		~Input_Interface_Lua();
		void onClick(bool active);
		void onKeypress(Key key, bool active);

		static bool implementsInterface(const luabridge::LuaRef& ref);
	};

	LuaDevice(DeviceID id, luabridge::LuaRef env, lua_State* L);
	~LuaDevice();
};
