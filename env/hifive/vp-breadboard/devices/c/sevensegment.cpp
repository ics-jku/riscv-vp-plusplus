#include "sevensegment.h"

Sevensegment::Sevensegment(DeviceID id) : CDevice(id) {
	pin = std::make_unique<Segment_PIN>(this);
	graph = std::make_unique<Segment_Graph>(this);
}

Sevensegment::~Sevensegment() {}

const DeviceClass Sevensegment::getClass() const { return classname; }

/* PIN Interface */

Sevensegment::Segment_PIN::Segment_PIN(CDevice* device) : CDevice::PIN_Interface_C(device) {
	layout = PinLayout();
	layout.emplace(0, PinDesc{PinDesc::Dir::input, "top"});
	layout.emplace(1, PinDesc{PinDesc::Dir::input, "top_right"});
	layout.emplace(2, PinDesc{PinDesc::Dir::input, "bottom_right"});
	layout.emplace(3, PinDesc{PinDesc::Dir::input, "bottom"});
	layout.emplace(4, PinDesc{PinDesc::Dir::input, "bottom_left"});
	layout.emplace(5, PinDesc{PinDesc::Dir::input, "top_left"});
	layout.emplace(6, PinDesc{PinDesc::Dir::input, "center"});
	layout.emplace(7, PinDesc{PinDesc::Dir::input, "dot"});
}

void Sevensegment::Segment_PIN::setPin(PinNumber num, gpio::Tristate val) {
	if(num <= 7) {
		Segment_Graph* segment_device = static_cast<Segment_Graph*>(device->graph.get());
		segment_device->draw(num, val == gpio::Tristate::HIGH);
	}
}

/* Graphbuf Interface */

Sevensegment::Segment_Graph::Segment_Graph(CDevice* device) : CDevice::Graphbuf_Interface_C(device) {
	layout = Layout{4, 5, "rgba"};
}

void Sevensegment::Segment_Graph::initializeBuffer() {
	auto *img = buffer.bits();
	for(unsigned x=0; x<buffer.width(); x++) {
		for(unsigned y=0; y<buffer.height(); y++) {
			const auto offs = (y * buffer.width() + x) * 4; // heavily depends on rgba8888
			img[offs+0] = 0;
			img[offs+1] = 0;
			img[offs+2] = 0;
			img[offs+3] = 128;
		}
	}
	for(PinNumber num=0; num<=7; num++) {
		draw(num, false);
	}
}

void Sevensegment::Segment_Graph::draw(PinNumber num, bool val) {
	if(num > 7) { return; }
	// general display stuff
	unsigned xcol1 = 0;
	unsigned xcol2 = 3 * (buffer.width() / 4);
	unsigned yrow1 = 0;
	unsigned yrow2 = (buffer.height() - 2) / 2;
	unsigned yrow3 = buffer.height() - 2;
	unsigned x1=0, y1=0;
	unsigned x2=0, y2=0;
	// assign start and end point of line
	if(num == 0) {
		x1 = xcol1, y1 = yrow1;
		x2 = xcol2, y2 = yrow1;
	} else if(num == 1) {
		x1 = xcol2, y1 = yrow1;
		x2 = xcol2, y2 = yrow2;
	} else if(num == 2) {
		x1 = xcol2, y1 = yrow2;
		x2 = xcol2, y2 = yrow3;
	} else if(num == 3) {
		x1 = xcol1, y1 = yrow3;
		x2 = xcol2, y2 = yrow3;
	} else if(num == 4) {
		x1 = xcol1, y1 = yrow2;
		x1 = xcol1, y2 = yrow3;
	} else if(num == 5) {
		x1 = xcol1, y1 = yrow1;
		x2 = xcol1, y2 = yrow2;
	} else if(num == 6) {
		x1 = xcol1, y1 = yrow2;
		x2 = xcol2, y2 = yrow2;
	} else if(num == 7) {
		x1 = buffer.width() - 4, y1 = yrow3 - 2;
		x2 = buffer.width() - 2, y2 = yrow3;
	}

	auto *img = buffer.bits();
	for(unsigned x=x1; x<=x2+1; x++) {
		for(unsigned y=y1; y<=y2+1; y++) {
			const auto offs = (y * buffer.width() + x) * 4; // heavily depends on rgba8888
			img[offs+0] = val?(uint8_t)255:(uint8_t)0;
			img[offs+1] = 0;
			img[offs+2] = 0;
			img[offs+3] = 255;
		}
	}
}
