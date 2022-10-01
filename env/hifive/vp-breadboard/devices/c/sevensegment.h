#pragma once

#include "devices/factory/cFactory.h"

class Sevensegment : public CDevice {

public:
	Sevensegment(DeviceID id);
	~Sevensegment();

	inline static DeviceClass classname = "sevensegment";
	const DeviceClass getClass() const;
	void draw_segment(PinNumber num, bool val);

	class Segment_PIN : public CDevice::PIN_Interface_C {
	public:
		Segment_PIN(CDevice* device);
		void setPin(PinNumber num, gpio::Tristate val);
	};

	class Segment_Graph : public CDevice::Graphbuf_Interface_C {
	public:
		Segment_Graph(CDevice* device);
		void initializeBuffer();
	};
};

static const bool registeredSevensegment = getCFactory().registerDeviceType<Sevensegment>();
