#pragma once

#include "cdevice.h"

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
		gpio::Tristate getPin(PinNumber num);
	};

	class Button_Graph : public CDevice::Graphbuf_Interface_C {
	public:
		Button_Graph(CDevice* device);
		void initializeBufferMaybe();
	};

	class Button_Input : public CDevice::Input_Interface_C {
	public:
		Button_Input(CDevice* device);
		void mouse(bool active);
		void key(int key, bool active);
	};
};
