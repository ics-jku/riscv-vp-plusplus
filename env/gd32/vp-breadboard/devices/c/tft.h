#pragma once

#include <queue>
#include <vector>

#include "devices/factory/cFactory.h"

#define ENABLE_SCREENSHOT
#ifdef ENABLE_SCREENSHOT
const uint8_t TFT_SCREENSHOT = 0xFF;
#endif

const uint8_t TFT_CASET = 0x2A;
const uint8_t TFT_MADCTL = 0x36;
const uint8_t TFT_PASET = 0x2B;
const uint8_t TFT_RAMWR = 0x2C;

const uint8_t XPT_X = 0x90;
const uint8_t XPT_Y = 0xD0;

const uint16_t touchCalibration_x0 = 300, touchCalibration_x1 = 3360, touchCalibration_y0 = 300,
               touchCalibration_y1 = 3400, width = 240, height = 320;

template <class T, unsigned max>
class Parameters {
	std::vector<T> parameters;
	int count = 0;

   public:
	uint8_t cmd;

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

class TFTStateTranslator {
	struct TFTState {
		struct Range {
			uint16_t start;
			uint16_t end;
		};
		Range column_range;
		Range page_range;
		uint16_t column;
		uint16_t page;
	};

	TFTState virtual_state;

	const uint16_t max_page = 319;
	const uint16_t max_column = 239;

	uint8_t ctl = 0;

   public:
	void setCtl(uint8_t bit5, uint8_t bit6, uint8_t bit7) {
		ctl = ((bit5 & 1) << 2) | ((bit6 & 1) << 1) | (bit7 & 1);
	}

	void setRangeColumn(uint16_t start, uint16_t end) {
		virtual_state.column_range.start = start;
		virtual_state.column_range.end = end;
	}

	void setRangePage(uint16_t start, uint16_t end) {
		virtual_state.page_range.start = start;
		virtual_state.page_range.end = end;
	}

	void setToStart() {
		virtual_state.column = virtual_state.column_range.start;
		virtual_state.page = virtual_state.page_range.start;
	}

	void advance() {
		if (virtual_state.column < virtual_state.column_range.end) {
			virtual_state.column++;
		} else {
			virtual_state.column = virtual_state.column_range.start;
			if (virtual_state.page < virtual_state.page_range.end) {
				virtual_state.page++;
			} else {
				virtual_state.page = virtual_state.page_range.start;
			}
		}
	}

	uint16_t getPhysicalColumn() {
		switch (ctl) {
			case 0:
			case 1:
				return virtual_state.column;
			case 2:
			case 3:
				return max_column - virtual_state.column;
			case 4:
			case 5:
				return virtual_state.page;
			case 6:
			case 7:
				return max_column - virtual_state.page;
			default:
				return 0;
		}
	}

	uint16_t getPhysicalPage() {
		switch (ctl) {
			case 0:
			case 2:
				return virtual_state.page;
			case 1:
			case 3:
				return max_page - virtual_state.page;
			case 4:
			case 6:
				return virtual_state.column;
			case 5:
			case 7:
				return max_page - virtual_state.column;
			default:
				return 0;
		}
	}
};

class TFT : public CDevice {
	bool is_data = false;
	TFTStateTranslator state;
	uint8_t current_cmd;
	Parameters<uint8_t, 4> parameters;
	bool penirq;
	std::queue<uint8_t> txbuffer;
	uint16_t current_x;
	uint16_t current_y;

   public:
	TFT(DeviceID id);
	~TFT();

	inline static DeviceClass classname = "tft";
	const DeviceClass getClass() const;

	class TFT_PIN : public CDevice::PIN_Interface_C {
	   public:
		TFT_PIN(CDevice* device);
		void setPin(PinNumber num, gpio::Tristate val);
		gpio::Tristate getPin(PinNumber num);
	};

	class TFT_EXMC : public CDevice::EXMC_Interface_C {
	   public:
		TFT_EXMC(CDevice* device);
		gpio::EXMC_Data send(gpio::EXMC_Data data);
	};

	class TFT_SPI : public CDevice::SPI_Interface_C {
	   public:
		TFT_SPI(CDevice* device);
		gpio::SPI_Response send(gpio::SPI_Command byte);
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
