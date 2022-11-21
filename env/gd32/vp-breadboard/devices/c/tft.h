#pragma once

#include <vector>

#include "devices/factory/cFactory.h"

const uint8_t TFT_CASET = 0x2A;
const uint8_t TFT_PASET = 0x2B;
const uint8_t TFT_RAMWR = 0x2C;

const uint8_t COMMANDS[3] = {TFT_CASET, TFT_PASET, TFT_RAMWR};

template <class T, unsigned max>
struct Parameters {
	std::vector<T> parameters;
	uint8_t cmd;
	int count = 0;

	bool add(T val) {
		if (count >= max)
			return false;
		parameters.push_back(val);
		count++;
		return true;
	}

	bool isComplete() {
		return count >= max;
	}

	bool isEmpty() {
		return count == 0;
	}

	void reset() {
		count = 0;
		parameters.clear();
	}

	T& operator[](int index) {
		if (index >= count)
			throw std::out_of_range("index out of bound");
		return parameters[index];
	}
};

struct TFTState {
	struct Range {
		uint16_t start;
		uint16_t end;
	};
	Range column;
	Range page;
	uint16_t current_column;
	uint16_t current_page;
};

class TFT : public CDevice {
	bool is_data = false;
	TFTState state;
	uint8_t current_cmd;
	Parameters<uint8_t, 4> parameters;

   public:
	TFT(DeviceID id);
	~TFT();

	inline static DeviceClass classname = "tft";
	const DeviceClass getClass() const;
	void draw(bool active, QMouseEvent* e);

	class TFT_PIN : public CDevice::PIN_Interface_C {
	   public:
		TFT_PIN(CDevice* device);
		void setPin(PinNumber num, gpio::Tristate val);
	};

	class TFT_EXMC : public CDevice::EXMC_Interface_C {
	   public:
		TFT_EXMC(CDevice* device);
		void send(gpio::EXMC_Data data);
	};

	class TFT_Graph : public CDevice::Graphbuf_Interface_C {
	   public:
		TFT_Graph(CDevice* device);
		void initializeBufferMaybe();
	};

	class TFT_Input : public CDevice::TFT_Input_Interface_C {
	   public:
		TFT_Input(CDevice* device);
		void onClick(bool active, QMouseEvent* e);
	};
};

static const bool registeredTFT = getCFactory().registerDeviceType<TFT>();
