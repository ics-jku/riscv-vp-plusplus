/*
 * scriptloader.cpp
 *
 *  Created on: Apr 29, 2022
 *      Author: pp
 */

#include "luaFactory.hpp"

#include <QDirIterator>
#include <exception>
#include <filesystem>

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

#include "configurations.h"

using namespace std;
using namespace luabridge;
using std::filesystem::directory_iterator;

static lua_State* L;

/**
 * @return [false, ...] if invalid
 */
LuaRef loadScriptFromFile(lua_State* L, filesystem::path p) {
	LuaRef scriptloader = getGlobal(L, "scriptloader_file");
	try {
		LuaResult r = scriptloader(p.c_str());
		if (!r.wasOk()) {
			cerr << p << ": " << r.errorMessage() << endl;
			return LuaRef(L);
		}
		if (r.size() != 1) {
			// cerr << p << " failed." << endl;
			return LuaRef(L);
		}
		if (!r[0].isTable()) {
			cerr << p << ": " << r[0] << endl;
			return LuaRef(L);
		}
		return r[0];

	} catch (LuaException& e) {
		cerr << "serious shit got down in file " << p << endl;
		cerr << e.what() << endl;
		return LuaRef(L);
	}
}

/**
 * @return [false, ...] if invalid
 */
LuaRef loadScriptFromString(lua_State* L, std::string p, std::string name = "external script") {
	LuaRef scriptloader = getGlobal(L, "scriptloader_string");
	try {
		LuaResult r = scriptloader(p, name);
		if (!r.wasOk()) {
			cerr << p << ": " << r.errorMessage() << endl;
			return LuaRef(L);
		}
		if (r.size() != 1) {
			// cerr << p << " failed." << endl;
			return LuaRef(L);
		}
		if (!r[0].isTable()) {
			cerr << p << ": " << r[0] << endl;
			return LuaRef(L);
		}
		return r[0];

	} catch (LuaException& e) {
		cerr << "serious shit got down in string \n" << p << endl;
		cerr << e.what() << endl;
		return LuaRef(L);
	}
}

bool isScriptValidDevice(LuaRef& chunk, std::string name = "") {
	if (chunk.isNil()) {
		cerr << "[lua]\tScript " << name << " could not be loaded" << endl;
		return false;
	}
	if (chunk["classname"].isNil() || !chunk["classname"].isString()) {
		cerr << "[lua]\tScript " << name << " does not contain a (valid) classname" << endl;
		return false;
	}
	return true;
}

LuaFactory::LuaFactory() {
	L = luaL_newstate();
	luaL_openlibs(L);

	QFile loader(scriptloader.c_str());
	if (!loader.open(QIODevice::ReadOnly)) {
		throw(runtime_error("Could not open scriptloader at " + scriptloader));
	}
	QByteArray loader_content = loader.readAll();

	if (luaL_dostring(L, loader_content)) {
		cerr << "Error loading loadscript:\n" << lua_tostring(L, lua_gettop(L)) << endl;
		lua_pop(L, 1);
		throw(runtime_error("Loadscript not valid"));
	}

	// cout << "Scanning built-in devices..." << endl;

	QDirIterator it(":/devices/lua");
	while (it.hasNext()) {
		it.next();
		// cout << "\t" << it.fileName().toStdString() << endl;
		QFile script_file(it.filePath());
		if (!script_file.open(QIODevice::ReadOnly)) {
			throw(runtime_error("Could not open file " + it.fileName().toStdString()));
		}
		QByteArray script = script_file.readAll();
		const auto filepath = it.filePath().toStdString();

		auto chunk = loadScriptFromString(L, script.toStdString(), it.fileName().toStdString());
		if (!isScriptValidDevice(chunk, filepath))
			continue;
		const auto classname = chunk["classname"].cast<string>();
		if (available_devices.find(classname) != available_devices.end()) {
			cerr << "[lua] Warn: '" << classname << "' from '" << filepath
			     << "' "
			        "would overwrite device from '"
			     << available_devices.at(classname) << "'" << endl;
			continue;
		}
		available_devices.emplace(classname, filepath);
	}
}

// TODO: Reduce code duplication for file/string load
void LuaFactory::scanAdditionalDir(std::string dir, bool overwrite_existing) {
	cout << "[lua] Scanning additional devices at '" << dir << "'." << endl;

	QDirIterator it(dir.c_str(), QStringList() << "*.lua", QDir::Files, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		it.next();
		// cout << "\t" << it.fileName().toStdString() << endl;
		const auto filepath = it.filePath().toStdString();
		auto chunk = loadScriptFromFile(L, filepath);
		if (!isScriptValidDevice(chunk, filepath))
			continue;
		const auto classname = chunk["classname"].cast<string>();
		if (available_devices.find(classname) != available_devices.end()) {
			if (!overwrite_existing) {
				cerr << "[lua] Warn: '" << classname << "' from '" << filepath
				     << "' "
				        "would overwrite device from '"
				     << available_devices.at(classname) << "'" << endl;
				continue;
			} else {
				cout << "[lua] Warn: '" << classname << "' from '" << filepath
				     << "' "
				        "overwrites device from '"
				     << available_devices.at(classname) << "'" << endl;
			}
		}
		available_devices.emplace(classname, filepath);
	}
}

void LuaFactory::printAvailableDevices() {
	cout << "Available devices: " << endl;
	for (const auto& [name, file] : available_devices) {
		cout << "\t" << name << " from " << file << endl;
	}
}

bool LuaFactory::deviceExists(DeviceClass classname) {
	return available_devices.find(classname) != available_devices.end();
}

unique_ptr<LuaDevice> LuaFactory::instantiateDevice(DeviceID id, DeviceClass classname) {
	if (!deviceExists(classname)) {
		throw(device_not_found_error(classname));
	}
	QFile script_file(available_devices[classname].c_str());
	if (!script_file.open(QIODevice::ReadOnly)) {
		throw(runtime_error("Could not open file " + available_devices[classname]));
	}
	QByteArray script = script_file.readAll();

	return std::make_unique<LuaDevice>(id, loadScriptFromString(L, script.toStdString(), classname), L);
}
