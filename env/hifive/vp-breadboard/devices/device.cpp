/*
 * device.cpp
 *
 *  Created on: Sep 30, 2021
 *      Author: dwd
 */

#include "device.hpp"

using std::string;
using std::cout;
using std::cerr;
using std::endl;
using luabridge::LuaRef;
using luabridge::LuaResult;


Device::Device(string id, LuaRef env) : id(id), env(env){
	if(SPI_Interface::implementsInterface(env)) {
		//cout << name << " implements spi interface" << endl;
		spi = std::make_unique<SPI_Interface>(env);
	}
};

const string& Device::getID() const {
	return id;
}

Device::Config Device::getConfig(){
	Device::Config ret;
	if(env["getConfig"].isNil()){
		cerr << id << " does not implement getConfig" << endl;
		return ret;
	}
	LuaResult r = env["getConfig"]();
	//cout << r.size() << " elements in config" << endl;

	for(unsigned i = 0; i < r.size(); i++) {
		if(!r[i].isTable()){
			cerr << "config return value malformed:" << endl;
			cerr << i << "\t" << r[i] << endl;
			continue;
		}
		//cout << "\tElement " << i << ": " << r[i] << " with length " << r[i].length() << endl;
		if(r[i].length() != 2) {
			cerr << "Config element " << i << " (" << r[i] << ") is not a pair" << endl;
			continue;
		}

		LuaRef name = r[i][1];
		LuaRef value = r[i][2];

		if(!name.isString()) {
			cerr << "Config name " << name << " is not a string" << endl;
			continue;
		}

		switch(value.type()) {
		case LUA_TNUMBER:
			ret.emplace(
					name, Device::ConfigElem{value.cast<typeof(Device::ConfigElem::Value::integer)>()}
			);
			break;
		case LUA_TBOOLEAN:
			ret.emplace(
					name, Device::ConfigElem{value.cast<bool>()}
			);
			break;
		default:
			cerr << "Config value of unknown type: " << value << endl;
		}
	}
	return ret;
}

bool Device::setConfig(const Device::Config conf) {
	if(env["setConfig"].isNil()){
		cerr << "Device does not implement setConfig" << endl;
		return false;
	}
	LuaRef c = luabridge::newTable(env.state());
	for(auto& [name, elem] : conf) {
		c[name] = elem.value.integer;
	}
	return env["setConfig"](c).wasOk();
}

Device::SPI_Interface::SPI_Interface(LuaRef& ref) :
		m_setDC(ref.state()), m_send(ref.state()){
	if(!implementsInterface(ref)) {
		cerr << ref << "not implementing SPI interface" << endl;
		return;
	}

	m_setDC = ref["setDC"];
	m_send = ref["receiveSPI"];
}

bool Device::SPI_Interface::hasDC() {
	return m_setDC.isFunction();
}

void Device::SPI_Interface::setDC(bool val) {
	if(m_setDC.isFunction())
		m_setDC(val);
}

uint8_t Device::SPI_Interface::send(uint8_t byte) {
	LuaResult r = m_send(byte);
	if(r.size() != 1) {
		cerr << " send SPI function failed!" << endl;
		return 0;
	}
	if(!r[0].isNumber()) {
		cerr << " send SPI function returned invalid type " << r[0] << endl;
		return 0;
	}
	return r[0];
}

bool Device::SPI_Interface::implementsInterface(const LuaRef& ref) {
	return ref["receiveSPI"].isFunction();
}

