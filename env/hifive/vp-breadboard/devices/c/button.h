#pragma once

#include "devices/cdevice.h"

class Button : public CDevice {
public:
	Button(DeviceID id);
	~Button();
	const DeviceClass getClass() const;
	void draw_area(bool active);

	class Button_Graph : public CDevice::Graphbuf_Interface_C {
	public:
		Button_Graph(CDevice* device);
		void initializeBufferMaybe();
	};

	class Button_Input : public CDevice::Input_Interface_C {
	public:
		Button_Input(CDevice* device);
		gpio::Tristate pressed(bool active);
	};
};
