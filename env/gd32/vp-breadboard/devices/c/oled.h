#pragma once

#include "devices/factory/cFactory.h"

const uint8_t COL_LOW = 0;
const uint8_t COL_HIGH = 0x10;
const uint8_t PUMP_VOLTAGE = 0x30;
const uint8_t DISPLAY_START_LINE = 0x40;
const uint8_t CONTRAST_MODE_SET = 0x81;
const uint8_t DISPLAY_ON = 0xAE;
const uint8_t PAGE_ADDR = 0xB0;
const uint8_t NOP = 0xE3;

const uint8_t COMMANDS[8] = {COL_LOW,           COL_HIGH,   PUMP_VOLTAGE, DISPLAY_START_LINE,
                             CONTRAST_MODE_SET, DISPLAY_ON, PAGE_ADDR,    NOP};

class OLED : public CDevice {
	bool is_data = false;
	State state;

   public:
	OLED(DeviceID id);
	~OLED();

	inline static DeviceClass classname = "oled";
	const DeviceClass getClass() const;

	class OLED_PIN : public CDevice::PIN_Interface_C {
	   public:
		OLED_PIN(CDevice* device);
		void setPin(PinNumber num, gpio::Tristate val);
	};

	class OLED_SPI : public CDevice::SPI_Interface_C {
	   public:
		OLED_SPI(CDevice* device);
		gpio::SPI_Response send(gpio::SPI_Command byte);
	};

	class OLED_Graph : public CDevice::Graphbuf_Interface_C {
	   public:
		OLED_Graph(CDevice* device);
		void initializeBufferMaybe();
	};
};

static const bool registeredOLED = getCFactory().registerDeviceType<OLED>();
