#pragma once

#include "devices/factory/cFactory.h"

class RGB : public CDevice {

public:
	RGB(DeviceID id);
	~RGB();

	inline static DeviceClass classname = "rgb";
	const DeviceClass getClass() const;
	void draw_rgb(PinNumber num, bool val);

	class RGB_Pin : public CDevice::PIN_Interface_C {
	public:
		RGB_Pin(CDevice* device);
		void setPin(PinNumber num, gpio::Tristate val);
	};

	class RGB_Graph : public CDevice::Graphbuf_Interface_C {
	public:
		RGB_Graph(CDevice* device);
		void initializeBuffer();
	};
};

static const bool registeredRGB = getCFactory().registerDeviceType<RGB>();
