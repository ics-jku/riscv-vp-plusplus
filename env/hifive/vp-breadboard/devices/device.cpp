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


Device::Device(const string id, LuaRef env, lua_State* l) : m_id(id), m_env(env){
	if(PIN_Interface::implementsInterface(m_env)) {
		pin = std::make_unique<PIN_Interface>(m_env);
	}
	if(SPI_Interface::implementsInterface(m_env)) {
		spi = std::make_unique<SPI_Interface>(m_env);
	}
	if(Config_Interface::implementsInterface(m_env)) {
		conf = std::make_unique<Config_Interface>(m_env);
	}
	if(Graphbuf_Interface::implementsInterface(m_env)) {
		graph = std::make_unique<Graphbuf_Interface>(env, m_id, l);
	}
};

const string& Device::getID() const {
	return m_id;
}

const string Device::getClass() const {
	return m_env["classname"].cast<string>();
}

Device::PIN_Interface::PIN_Interface(LuaRef& ref) :
		m_getPinLayout(ref["getPinLayout"]),
		m_getPin(ref["getPin"]), m_setPin(ref["setPin"]) {
	if(!implementsInterface(ref))
		cerr << "[Device] [PIN_Interface] WARN: Device " << ref << " not implementing interface" << endl;
}



bool Device::PIN_Interface::implementsInterface(const luabridge::LuaRef& ref) {
	// TODO: Better checks
	return ref["getPinLayout"].isFunction() &&
	       (ref["getPin"].isFunction() ||
	        ref["setPin"].isFunction());
}

Device::PIN_Interface::PinLayout Device::PIN_Interface::getPinLayout() {
	PinLayout ret;
	LuaResult r = m_getPinLayout();
	//cout << r.size() << " elements in pinlayout" << endl;
	ret.reserve(r.size());
	for(unsigned i = 0; i < r.size(); i++) {
		if(!r[i].isTable()){
			cerr << "pinlayout return value malformed:" << endl;
			cerr << i << "\t" << r[i] << endl;
			continue;
		}
		//cout << "\tElement " << i << ": " << r[i] << " with length " << r[i].length() << endl;
		if(r[i].length() < 2 || r[i].length() > 3) {
			cerr << "Pinlayout element " << i << " (" << r[i] << ") is malformed" << endl;
			continue;
		}
		PinDesc desc;
		auto number = r[i][1].cast<PinNumber>();
		desc.name = "undef";
		if(r[i].length() == 3)
			desc.name = r[i][3].tostring();

		const string direction_raw = r[i][2];
		if(direction_raw == "input") {
			desc.dir = PinDesc::Dir::input;
		} else if(direction_raw == "output") {
			desc.dir = PinDesc::Dir::output;
		} else if(direction_raw == "inout") {
			desc.dir = PinDesc::Dir::inout;
		} else {
			cerr << "Pinlayout element " << i << " (" << r[i] << "), direction " << direction_raw << " is malformed" << endl;
			continue;
		}
		//cout << "Mapping device's pin " << number << " (" << desc.name << ")" << endl;
		ret.emplace(number, desc);
	}
	return ret;
}


bool Device::PIN_Interface::getPin(PinNumber num) {
	const LuaResult r = m_getPin(num);
	if(!r || !r[0].isBool()) {
		cerr << "[lua] Device getPin returned malformed output" << endl;
		return false;
	}
	return r[0].cast<bool>();
}

void Device::PIN_Interface::setPin(PinNumber num, bool val) {
	m_setPin(num, val);
}

Device::SPI_Interface::SPI_Interface(LuaRef& ref) :
		m_send(ref["receiveSPI"]) {
	if(!implementsInterface(ref))
		cerr << ref << "not implementing SPI interface" << endl;
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

Device::Config_Interface::Config_Interface(luabridge::LuaRef& ref) :
	m_getConf(ref["getConfig"]), m_setConf(ref["setConfig"]), m_env(ref){

};

bool Device::Config_Interface::implementsInterface(const luabridge::LuaRef& ref) {
	return !ref["getConfig"].isNil() && !ref["setConfig"].isNil();
}

Device::Config_Interface::Config Device::Config_Interface::getConfig(){
	Config ret;
	LuaResult r = m_getConf();
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
					name, ConfigElem{value.cast<typeof(ConfigElem::Value::integer)>()}
			);
			break;
		case LUA_TBOOLEAN:
			ret.emplace(
					name, ConfigElem{value.cast<bool>()}
			);
			break;
		default:
			cerr << "Config value of unknown type: " << value << endl;
		}
	}
	return ret;
}

bool Device::Config_Interface::setConfig(const Device::Config_Interface::Config conf) {
	LuaRef c = luabridge::newTable(m_env.state());
	for(auto& [name, elem] : conf) {
		c[name] = elem.value.integer;
	}
	return m_setConf(c).wasOk();
}

Device::Graphbuf_Interface::Graphbuf_Interface(luabridge::LuaRef& ref,
	                                           std::string device_id,
	                                           lua_State* l) :
		m_getGraphBufferLayout(ref["getGraphBufferLayout"]), m_env(ref),
		m_deviceId(device_id), L(l) {
	if(!implementsInterface(ref))
		cerr << "[Device] [Graphbuf_Interface] WARN: Device " << ref << " not implementing interface" << endl;

	declarePixelFormat(L);
}

Device::Graphbuf_Interface::Layout Device::Graphbuf_Interface::getLayout() {
	Layout ret = {0,0,"invalid"};
	LuaResult r = m_getGraphBufferLayout();
	if(r.size() != 1 || !r[0].isTable() || r[0].length() != 3) {
		cerr << "[Device] [Graphbuf_Interface] Layout malformed" << endl;
		return ret;
	}
	ret.width = r[0][1].cast<unsigned>();
	ret.height = r[0][2].cast<unsigned>();
	const auto& type = r[0][3];
	if(!type.isString() || type != string("rgba")) {
		cerr << "[Device] [Graphbuf_Interface] Layout type may only be 'rgba' at the moment." << endl;
		return ret;
	}
	ret.data_type = type.cast<string>();
	return ret;
}

void Device::Graphbuf_Interface::declarePixelFormat(lua_State* L) {
	if(luaL_dostring (L, "graphbuf.Pixel(0,0,0,0)") != 0) {
		//cout << "Testpixel could not be created, probably was not yet registered" << endl;
		luabridge::getGlobalNamespace(L)
			.beginNamespace("graphbuf")
			  .beginClass <Pixel> ("Pixel")
			    .addConstructor <void (*) (const uint8_t, const uint8_t, const uint8_t, const uint8_t)> ()
			    .addProperty ("r", &Pixel::r)
			    .addProperty ("g", &Pixel::g)
			    .addProperty ("b", &Pixel::b)
			    .addProperty ("a", &Pixel::a)
			  .endClass ()
			.endNamespace()
		;
		//cout << "Graphbuf: Declared Pixel class to lua." << endl;
	} else {
		//cout << "Pixel class already registered." << endl;
	}
}

template<typename FunctionFootprint>
void Device::Graphbuf_Interface::registerGlobalFunctionAndInsertLocalAlias(
		const std::string name, FunctionFootprint fun) {
	if(m_deviceId.length() == 0 || name.length() == 0) {
		cerr << "[Graphbuf] Error: Name '" << name << "' or prefix '"
				<< m_deviceId << "' invalid!" << endl;
		return;
	}

	const auto globalFunctionName = m_deviceId + "_" + name;
	luabridge::getGlobalNamespace(L)
		.addFunction(globalFunctionName.c_str(), fun)
	;
	//cout << "Inserted function " << globalFunctionName << " into global namespace" << endl;

	const auto global_lua_fun = luabridge::getGlobal(L, globalFunctionName.c_str());
	if(!global_lua_fun.isFunction()) {
		cerr << "[Graphbuf] Error: " << globalFunctionName  << " is not valid!" << endl;
		return;
	}
	m_env[name.c_str()] = global_lua_fun;

	//cout << "Registered function " << globalFunctionName << " to " << name << endl;
};


void Device::Graphbuf_Interface::registerSetBuf(const SetBuf_fn setBuf) {
	registerGlobalFunctionAndInsertLocalAlias<>("setGraphbuffer", setBuf);
}

void Device::Graphbuf_Interface::registerGetBuf(const GetBuf_fn getBuf) {
	registerGlobalFunctionAndInsertLocalAlias<>("getGraphbuffer", getBuf);
}

bool Device::Graphbuf_Interface::implementsInterface(const luabridge::LuaRef& ref) {
	if(!ref["getGraphBufferLayout"].isFunction()) {
		//cout << "getGraphBufferLayout not a Function" << endl;
		return false;
	}
	LuaResult r = ref["getGraphBufferLayout"]();
	if(r.size() != 1 || !r[0].isTable() || r[0].length() != 3) {
		//cout << "return val is " << r.size() << " " << !r[0].isTable() << r[0].length() << endl;
		return false;
	}
	const auto& type = r[0][3];
	if(!type.isString()) {
		//cout << "Type not a string" << endl;
		return false;
	}
	return true;
}

