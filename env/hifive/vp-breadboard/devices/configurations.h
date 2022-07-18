#include <string>
#include <unordered_map>
#include <functional>

// PIN_Interface

typedef std::string DeviceID;
typedef std::string DeviceClass;

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

typedef std::unordered_map<PinNumber,PinDesc> PinLayout; // device pin

//ConfigInterface

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

// GraphBufInterface

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

struct State {
	unsigned column = 0;
	unsigned page = 0;
	uint8_t contrast = 255;
	bool display_on = true;
};

const uint8_t COL_LOW= 0;
const uint8_t COL_HIGH = 0x10;
const uint8_t PUMP_VOLTAGE = 0x30;
const uint8_t DISPLAY_START_LINE = 0x40;
const uint8_t CONTRAST_MODE_SET = 0x81;
const uint8_t DISPLAY_ON = 0xAE;
const uint8_t PAGE_ADDR = 0xB0;
const uint8_t NOP = 0xE3;

const uint8_t COMMANDS [8] = {COL_LOW, COL_HIGH, PUMP_VOLTAGE, DISPLAY_START_LINE, CONTRAST_MODE_SET, DISPLAY_ON, PAGE_ADDR, NOP};
