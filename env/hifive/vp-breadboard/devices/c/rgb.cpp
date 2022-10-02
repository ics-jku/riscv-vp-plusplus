#include "rgb.h"

#include <math.h>

RGB::RGB(DeviceID id) : CDevice(id) {
	pin = std::make_unique<RGB_Pin>(this);
	graph = std::make_unique<RGB_Graph>(this);
}

RGB::~RGB() {}

const DeviceClass RGB::getClass() const { return classname; }

/* PIN */

RGB::RGB_Pin::RGB_Pin(CDevice* device) : CDevice::PIN_Interface_C(device) {
	layout = PinLayout();
	layout.emplace(0, PinDesc{PinDesc::Dir::input, "r"});
	layout.emplace(1, PinDesc{PinDesc::Dir::input, "g"});
	layout.emplace(2, PinDesc{PinDesc::Dir::input, "b"});
}

void RGB::RGB_Pin::setPin(PinNumber num, gpio::Tristate val) {
	if(num <= 2) {
		RGB_Graph* rgb_graph = static_cast<RGB_Graph*>(device->graph.get());
		rgb_graph->draw(num, val == gpio::Tristate::HIGH);
	}
}

/* Graph */

RGB::RGB_Graph::RGB_Graph(CDevice* device) : CDevice::Graphbuf_Interface_C(device) {
	layout = Layout{1, 1, "rgba"};
}

void RGB::RGB_Graph::initializeBuffer() {
	for(PinNumber num=0; num<=2; num++) {
		draw(0, false);
	}
}

void RGB::RGB_Graph::draw(PinNumber num, bool val) {
	if(num > 2) { return; }
	int extent_center = ceil(buffer.width()/(float)2);
	Pixel cur = getPixel(extent_center, extent_center);
	if(num == 0) cur.r = val?255:0;
	else if(num == 1) cur.g = val?255:0;
	else cur.b = val?255:0;

	auto *img = buffer.bits();
	for(int x=1; x<buffer.width(); x++) {
		for(int y=1; y<buffer.height(); y++) {
			float dist = sqrt(pow(extent_center - x, 2) + pow(extent_center - y, 2));
			int norm_lumen = floor((1-dist/extent_center)*255);
			if(norm_lumen < 0) norm_lumen = 0;
			if(norm_lumen > 255) norm_lumen = 255;

			const auto offs = ((y-1) * buffer.width() + (x-1)) * 4; // heavily depends on rgba8888
			img[offs+0] = cur.r;
			img[offs+1] = cur.g;
			img[offs+2] = cur.b;
			img[offs+3] = (uint8_t)norm_lumen;
		}
	}
}
