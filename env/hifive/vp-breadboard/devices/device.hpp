/*
 * devices.hpp
 *
 *  Created on: Sep 29, 2021
 *      Author: dwd
 */

// TODO: Make this a generic (C or Lua) device

#pragma once

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
#include <string>
#include <vector>
#include <unordered_map>

class Device {
	std::string m_id;
	luabridge::LuaRef m_env;

public:

	const std::string& getID() const;
	const std::string getClass() const;

	class PIN_Interface {
		luabridge::LuaRef m_getPinLayout;
		luabridge::LuaRef m_getPin;
		luabridge::LuaRef m_setPin;
	public:
		typedef unsigned PinNumber;
		struct PinDesc {
			enum class Dir {
				input,
				output,
				inout
			} dir;
			// TODO: In future, add 'type' for analog values/pwm?
			std::string name;
		};
		typedef std::unordered_map<PinNumber,PinDesc> PinLayout;

		PIN_Interface(luabridge::LuaRef& ref);
		PinLayout getPinLayout();
		bool getPin(PinNumber num);
		void setPin(PinNumber num, bool val);	// TODO Tristate?
		static bool implementsInterface(const luabridge::LuaRef& ref);
	};

	class SPI_Interface {
		luabridge::LuaRef m_send;
	public:
		SPI_Interface(luabridge::LuaRef& ref);
		uint8_t send(uint8_t byte);
		static bool implementsInterface(const luabridge::LuaRef& ref);
	};

	class Config_Interface {
		luabridge::LuaRef m_getConf;
		luabridge::LuaRef m_setConf;
		luabridge::LuaRef& m_env;	// for building table
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
		typedef std::string ConfigDescription;
		typedef std::unordered_map<ConfigDescription,ConfigElem> Config;

		Config_Interface(luabridge::LuaRef& ref);
		Config getConfig();
		bool setConfig(const Config conf);
		static bool implementsInterface(const luabridge::LuaRef& ref);
	};

	class Graphbuf_Interface {
		luabridge::LuaRef m_getGraphBufferLayout;
		luabridge::LuaRef m_env;
		std::string m_deviceId;		// Redundant to Device's ID member
		lua_State* L;				// to register functions and Format

		static void declarePixelFormat(lua_State* L);
	public:

		struct Layout {
			unsigned width;
			unsigned height;
			std::string data_type;	// Currently ignored and always RGBA8888
		};

		// TODO: Add a scheme that only alpha channel is changed?
		//       either rgb may be negative (don't change)
		//       or just another function (probably better)
		typedef unsigned Xoffset;
		typedef unsigned Yoffset;
		struct Pixel {
			uint8_t r;
			uint8_t g;
			uint8_t b;
			uint8_t a;
		};
		typedef std::function<void(const Xoffset, const Yoffset, Pixel)> SetBuf_fn;
		typedef std::function<Pixel(const Xoffset, const Yoffset)> GetBuf_fn;

		Graphbuf_Interface(luabridge::LuaRef& ref, std::string device_id, lua_State* L);
		Layout getLayout();
		void registerSetBuf(const SetBuf_fn setBuf);
		void registerGetBuf(const GetBuf_fn getBuf);

		template<typename FunctionFootprint>
		void registerGlobalFunctionAndInsertLocalAlias(const std::string name, FunctionFootprint fun);

		static bool implementsInterface(const luabridge::LuaRef& ref);
	};


	std::unique_ptr<PIN_Interface> pin;
	std::unique_ptr<SPI_Interface> spi;
	std::unique_ptr<Config_Interface> conf;
	std::unique_ptr<Graphbuf_Interface> graph;

	Device(const std::string id, luabridge::LuaRef env, lua_State* L);
};

