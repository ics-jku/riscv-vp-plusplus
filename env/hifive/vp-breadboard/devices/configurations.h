#include <string>
#include <unordered_map>
#include <functional>

// PIN_Interface

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
