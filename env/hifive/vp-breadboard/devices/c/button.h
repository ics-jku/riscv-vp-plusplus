#pragma once

#include "devices/cdevice.h"

class Button : public CDevice {
	bool active = false;
public:
	Button(DeviceID id);
	~Button();
	const DeviceClass getClass() const;
	void draw_area();

	class Button_PIN : public CDevice::PIN_Interface_C {
	public:
		Button_PIN(CDevice* device);
		bool getPin(PinNumber num);
	};

	class Button_Graph : public CDevice::Graphbuf_Interface_C {
	public:
		Button_Graph(CDevice* device);
		void initializeBufferMaybe();
	};

	class Button_Input : public CDevice::Input_Interface_C {
	public:
		Button_Input(CDevice* device);
		gpio::Tristate mouse(bool active);
		gpio::Tristate key(int key, bool active);
	};
};
