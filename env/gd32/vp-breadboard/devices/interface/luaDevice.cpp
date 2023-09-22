/*
 * device.cpp
 *
 *  Created on: Sep 30, 2021
 *      Author: dwd
 */

#include "luaDevice.hpp"

#include <QKeySequence>

using luabridge::LuaRef;
using luabridge::LuaResult;
using std::cerr;
using std::cout;
using std::endl;
using std::string;

LuaDevice::LuaDevice(const DeviceID id, LuaRef env, lua_State* l) : Device(id), m_env(env) {
	if (PIN_Interface_Lua::implementsInterface(m_env)) {
		pin = std::make_unique<PIN_Interface_Lua>(m_env);
	}
	if (SPI_Interface_Lua::implementsInterface(m_env)) {
		spi = std::make_unique<SPI_Interface_Lua>(m_env);
	}
	if (Config_Interface_Lua::implementsInterface(m_env)) {
		conf = std::make_unique<Config_Interface_Lua>(m_env);
	}
	if (Graphbuf_Interface_Lua::implementsInterface(m_env)) {
		graph = std::make_unique<Graphbuf_Interface_Lua>(env, m_id, l);
	}
	if (Input_Interface_Lua::implementsInterface(m_env)) {
		input = std::make_unique<Input_Interface_Lua>(m_env);
	}
};

LuaDevice::~LuaDevice() {}

const DeviceClass LuaDevice::getClass() const {
	return m_env["classname"].cast<string>();
}

LuaDevice::PIN_Interface_Lua::PIN_Interface_Lua(LuaRef& ref)
    : m_getPinLayout(ref["getPinLayout"]), m_getPin(ref["getPin"]), m_setPin(ref["setPin"]) {
	if (!implementsInterface(ref))
		cerr << "[LuaDevice] [PIN_Interface] WARN: Device " << ref << " not implementing interface" << endl;
}

LuaDevice::PIN_Interface_Lua::~PIN_Interface_Lua() {}

bool LuaDevice::PIN_Interface_Lua::implementsInterface(const luabridge::LuaRef& ref) {
	// TODO: Better checks
	return ref["getPinLayout"].isFunction() && (ref["getPin"].isFunction() || ref["setPin"].isFunction());
}

PinLayout LuaDevice::PIN_Interface_Lua::getPinLayout() {
	PinLayout ret;
	LuaResult r = m_getPinLayout();
	// cout << r.size() << " elements in pinlayout" << endl;
	ret.reserve(r.size());
	for (unsigned i = 0; i < r.size(); i++) {
		if (!r[i].isTable()) {
			cerr << "pinlayout return value malformed:" << endl;
			cerr << i << "\t" << r[i] << endl;
			continue;
		}
		// cout << "\tElement " << i << ": " << r[i] << " with length " << r[i].length() << endl;
		if (r[i].length() < 2 || r[i].length() > 3) {
			cerr << "Pinlayout element " << i << " (" << r[i] << ") is malformed" << endl;
			continue;
		}
		PinDesc desc;
		auto number = r[i][1].cast<PinNumber>();
		desc.name = "undef";
		if (r[i].length() == 3)
			desc.name = r[i][3].tostring();

		const string direction_raw = r[i][2];
		if (direction_raw == "input") {
			desc.dir = PinDesc::Dir::input;
		} else if (direction_raw == "output") {
			desc.dir = PinDesc::Dir::output;
		} else if (direction_raw == "inout") {
			desc.dir = PinDesc::Dir::inout;
		} else {
			// TODO: Add PWM input here? Or better, lua script has to cope with ratios
			cerr << "Pinlayout element " << i << " (" << r[i] << "), direction " << direction_raw << " is malformed"
			     << endl;
			continue;
		}
		// cout << "Mapping Device's pin " << number << " (" << desc.name << ")" << endl;
		ret.emplace(number, desc);
	}
	return ret;
}

gpio::Tristate LuaDevice::PIN_Interface_Lua::getPin(PinNumber num) {
	const LuaResult r = m_getPin(num);
	if (!r || !r[0].isBool()) {
		cerr << "[lua] Device getPin returned malformed output" << endl;
		return gpio::Tristate::LOW;
	}
	return r[0].cast<bool>() ? gpio::Tristate::HIGH : gpio::Tristate::LOW;
}

void LuaDevice::PIN_Interface_Lua::setPin(PinNumber num, gpio::Tristate val) {
	m_setPin(num, val == gpio::Tristate::HIGH ? true : false);
}

LuaDevice::SPI_Interface_Lua::SPI_Interface_Lua(LuaRef& ref) : m_send(ref["receiveSPI"]) {
	if (!implementsInterface(ref))
		cerr << ref << "not implementing SPI interface" << endl;
}

LuaDevice::SPI_Interface_Lua::~SPI_Interface_Lua() {}

gpio::SPI_Response LuaDevice::SPI_Interface_Lua::send(gpio::SPI_Command byte) {
	LuaResult r = m_send(byte);
	if (r.size() != 1) {
		cerr << " send SPI function failed!" << endl;
		return 0;
	}
	if (!r[0].isNumber()) {
		cerr << " send SPI function returned invalid type " << r[0] << endl;
		return 0;
	}
	return r[0];
}

bool LuaDevice::SPI_Interface_Lua::implementsInterface(const LuaRef& ref) {
	return ref["receiveSPI"].isFunction();
}

LuaDevice::Config_Interface_Lua::Config_Interface_Lua(luabridge::LuaRef& ref)
    : m_getConf(ref["getConfig"]),
      m_setConf(ref["setConfig"]),
      m_env(ref){

      };

LuaDevice::Config_Interface_Lua::~Config_Interface_Lua() {}

bool LuaDevice::Config_Interface_Lua::implementsInterface(const luabridge::LuaRef& ref) {
	return !ref["getConfig"].isNil() && !ref["setConfig"].isNil();
}

Config* LuaDevice::Config_Interface_Lua::getConfig() {
	auto ret = new Config();
	LuaResult r = m_getConf();
	// cout << r.size() << " elements in config" << endl;

	for (unsigned i = 0; i < r.size(); i++) {
		if (!r[i].isTable()) {
			cerr << "config return value malformed:" << endl;
			cerr << i << "\t" << r[i] << endl;
			continue;
		}
		// cout << "\tElement " << i << ": " << r[i] << " with length " << r[i].length() << endl;
		if (r[i].length() != 2) {
			cerr << "Config element " << i << " (" << r[i] << ") is not a pair" << endl;
			continue;
		}

		LuaRef name = r[i][1];
		LuaRef value = r[i][2];

		if (!name.isString()) {
			cerr << "Config name " << name << " is not a string" << endl;
			continue;
		}

		switch (value.type()) {
			case LUA_TNUMBER:
				ret->emplace(name, ConfigElem{value.cast<typeof(ConfigElem::Value::integer)>()});
				break;
			case LUA_TBOOLEAN:
				ret->emplace(name, ConfigElem{value.cast<bool>()});
				break;
			case LUA_TSTRING:
				ret->emplace(name, ConfigElem{(char*)value.cast<string>().c_str()});
				break;
			default:
				cerr << "Config value of unknown type: " << value << endl;
		}
	}
	return ret;
}

bool LuaDevice::Config_Interface_Lua::setConfig(Config* conf) {
	LuaRef c = luabridge::newTable(m_env.state());
	for (auto& [name, elem] : (*conf)) {
		switch (elem.type) {
			case ConfigElem::Type::boolean:
				c[name] = elem.value.boolean;
				break;
			case ConfigElem::Type::integer:
				c[name] = elem.value.integer;
				break;
			case ConfigElem::Type::string:
				c[name] = string(elem.value.string);
				break;
		}
	}
	return m_setConf(c).wasOk();
}

LuaDevice::Graphbuf_Interface_Lua::Graphbuf_Interface_Lua(luabridge::LuaRef& ref, DeviceID device_id, lua_State* l)
    : m_getGraphBufferLayout(ref["getGraphBufferLayout"]),
      m_initializeGraphBuffer(ref["initializeGraphBuffer"]),
      m_env(ref),
      m_deviceId(device_id),
      L(l) {
	if (!implementsInterface(ref))
		cerr << "[LuaDevice] [Graphbuf_Interface] WARN: Device " << ref << " not implementing interface" << endl;

	declarePixelFormat(L);
}

LuaDevice::Graphbuf_Interface_Lua::~Graphbuf_Interface_Lua() {}

Layout LuaDevice::Graphbuf_Interface_Lua::getLayout() {
	Layout ret = {0, 0, "invalid"};
	LuaResult r = m_getGraphBufferLayout();
	if (r.size() != 1 || !r[0].isTable() || r[0].length() != 3) {
		cerr << "[LuaDevice] [Graphbuf_Interface] Layout malformed" << endl;
		return ret;
	}
	ret.width = r[0][1].cast<unsigned>();
	ret.height = r[0][2].cast<unsigned>();
	const auto& type = r[0][3];
	if (!type.isString() || type != string("rgba")) {
		cerr << "[LuaDevice] [Graphbuf_Interface] Layout type may only be 'rgba' at the moment." << endl;
		return ret;
	}
	ret.data_type = type.cast<string>();
	return ret;
}

void LuaDevice::Graphbuf_Interface_Lua::initializeBufferMaybe() {
	if (m_initializeGraphBuffer.isFunction()) {
		m_initializeGraphBuffer();
	} else {
		// cout << "Device " << m_deviceId << " does not implement 'initializeGraphBuffer()'" << endl;
	}
}

void LuaDevice::Graphbuf_Interface_Lua::declarePixelFormat(lua_State* L) {
	if (luaL_dostring(L, "graphbuf.Pixel(0,0,0,0)") != 0) {
		// cout << "Testpixel could not be created, probably was not yet registered" << endl;
		luabridge::getGlobalNamespace(L)
		    .beginNamespace("graphbuf")
		    .beginClass<Pixel>("Pixel")
		    .addConstructor<void (*)(const uint8_t, const uint8_t, const uint8_t, const uint8_t)>()
		    .addProperty("r", &Pixel::r)
		    .addProperty("g", &Pixel::g)
		    .addProperty("b", &Pixel::b)
		    .addProperty("a", &Pixel::a)
		    .endClass()
		    .endNamespace();
		// cout << "Graphbuf: Declared Pixel class to lua." << endl;
	} else {
		// cout << "Pixel class already registered." << endl;
	}
}

template <typename FunctionFootprint>
void LuaDevice::Graphbuf_Interface_Lua::registerGlobalFunctionAndInsertLocalAlias(const std::string name,
                                                                                  FunctionFootprint fun) {
	if (m_deviceId.length() == 0 || name.length() == 0) {
		cerr << "[Graphbuf] Error: Name '" << name << "' or prefix '" << m_deviceId << "' invalid!" << endl;
		return;
	}

	const auto globalFunctionName = m_deviceId + "_" + name;
	luabridge::getGlobalNamespace(L).addFunction(globalFunctionName.c_str(), fun);
	// cout << "Inserted function " << globalFunctionName << " into global namespace" << endl;

	const auto global_lua_fun = luabridge::getGlobal(L, globalFunctionName.c_str());
	if (!global_lua_fun.isFunction()) {
		cerr << "[Graphbuf] Error: " << globalFunctionName << " is not valid!" << endl;
		return;
	}
	m_env[name.c_str()] = global_lua_fun;

	// cout << "Registered function " << globalFunctionName << " to " << name << endl;
};

void LuaDevice::Graphbuf_Interface_Lua::registerBuffer(QImage& image) {
	Layout layout = getLayout();
	std::function<Pixel(const Xoffset, const Yoffset)> getBuf = [&image, layout](const Xoffset x, const Yoffset y) {
		return getBuffer(image, layout, x, y);
	};
	registerGlobalFunctionAndInsertLocalAlias("getGraphbuffer", getBuf);
	std::function<void(const Xoffset, const Yoffset, Pixel)> setBuf =
	    [&image, layout](const Xoffset x, const Yoffset y, Pixel p) { setBuffer(image, layout, x, y, p); };
	registerGlobalFunctionAndInsertLocalAlias<>("setGraphbuffer", setBuf);
}

bool LuaDevice::Graphbuf_Interface_Lua::implementsInterface(const luabridge::LuaRef& ref) {
	if (!ref["getGraphBufferLayout"].isFunction()) {
		// cout << "getGraphBufferLayout not a Function" << endl;
		return false;
	}
	LuaResult r = ref["getGraphBufferLayout"]();
	if (r.size() != 1 || !r[0].isTable() || r[0].length() != 3) {
		// cout << "return val is " << r.size() << " " << !r[0].isTable() << r[0].length() << endl;
		return false;
	}
	const auto& type = r[0][3];
	if (!type.isString()) {
		// cout << "Type not a string" << endl;
		return false;
	}
	return true;
}

LuaDevice::Input_Interface_Lua::Input_Interface_Lua(luabridge::LuaRef& ref)
    : m_onClick(ref["onClick"]), m_onKeypress(ref["onKeypress"]), m_env(ref) {
	if (!implementsInterface(ref))
		cerr << "[LuaDevice] [Input_Interface] WARN: Device " << ref << " not implementing interface" << endl;
}

LuaDevice::Input_Interface_Lua::~Input_Interface_Lua() {}

void LuaDevice::Input_Interface_Lua::onClick(bool active) {
	m_onClick(active);
}

void LuaDevice::Input_Interface_Lua::onKeypress(Key key, bool active) {
	m_onKeypress(QKeySequence(key).toString().toStdString(), active);
}

bool LuaDevice::Input_Interface_Lua::implementsInterface(const luabridge::LuaRef& ref) {
	return !ref["onClick"].isNil() || !ref["onKeypress"].isNil();
}
