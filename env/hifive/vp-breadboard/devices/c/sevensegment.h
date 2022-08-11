#pragma once

#include "devices/interface/cdevice.h"

class Sevensegment : public CDevice {

public:
	Sevensegment(DeviceID id);
	~Sevensegment();
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
		void initializeBufferMaybe();
	};
};
