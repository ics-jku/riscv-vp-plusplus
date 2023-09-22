#pragma once

#include <functional>
#include <set>
#include <string>
#include <unordered_map>

// Input Interface

typedef int Key;
typedef std::set<Key> Keys;

// PIN_Interface

typedef std::string DeviceID;
typedef std::string DeviceClass;

typedef unsigned PinNumber;

struct PinDesc {
	enum class Dir { input, output, inout } dir;
	// TODO: In future, add 'type' for analog values/pwm?
	std::string name;
};

typedef std::unordered_map<PinNumber, PinDesc> PinLayout;  // device pin

// ConfigInterface

struct ConfigElem {
	enum class Type {
		invalid = 0,
		integer,
		boolean,
		string,
	} type;
	union Value {
		int64_t integer;
		bool boolean;
		char* string;
	} value;

	ConfigElem() : type(Type::invalid){};
	ConfigElem(const ConfigElem& e) {
		type = e.type;
		if (type == Type::string) {
			value.string = new char[strlen(e.value.string) + 1];
			strcpy(value.string, e.value.string);
		} else {
			value = e.value;
		}
	}
	~ConfigElem() {
		if (type == Type::string) {
			delete[] value.string;
		}
	}

	ConfigElem(int64_t val) {
		type = Type::integer;
		value.integer = val;
	};
	ConfigElem(bool val) {
		type = Type::boolean;
		value.boolean = val;
	};
	ConfigElem(char* val) {
		type = Type::string;
		value.string = new char[strlen(val) + 1];
		strcpy(value.string, val);
	};
};
typedef std::string ConfigDescription;
typedef std::unordered_map<ConfigDescription, ConfigElem> Config;

// GraphBufInterface

struct Layout {
	unsigned width;
	unsigned height;
	std::string data_type;  // Currently ignored and always RGBA8888
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

struct State {
	unsigned column = 0;
	unsigned page = 0;
	uint8_t contrast = 255;
	bool display_on = true;
};
