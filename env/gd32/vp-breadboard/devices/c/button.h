#pragma once

#include "devices/factory/cFactory.h"

class Button : public CDevice {
	bool active = false;

   public:
	Button(DeviceID id);
	~Button();

	inline static DeviceClass classname = "button";
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
		void onClick(bool active);
		void onKeypress(int key, bool active);
	};
};

static const bool registeredButton = getCFactory().registerDeviceType<Button>();
