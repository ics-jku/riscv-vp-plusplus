#pragma once

#include "devices/cdevice.h"

class OLED : public CDevice {
	bool is_data = false;
	State state;

public:
	OLED(DeviceID id);
	~OLED();
	const DeviceClass getClass() const;

	class OLED_PIN : public CDevice::PIN_Interface_C {
	public:
		OLED_PIN(CDevice* device);
		void setPin(PinNumber num, bool val);
	};

	class OLED_SPI : public CDevice::SPI_Interface_C {
	public:
		OLED_SPI(CDevice* device);
		uint8_t send(uint8_t byte);
	};

	class OLED_Graph : public CDevice::Graphbuf_Interface_C {
	public:
		OLED_Graph(CDevice* device);
		void initializeBufferMaybe();
	};
};