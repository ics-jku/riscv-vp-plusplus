#include "rgb.h"

#include <math.h>

RGB::RGB(DeviceID id) : CDevice(id) {
	if(!pin) {
		layout_pin = PinLayout();
		layout_pin.emplace(0, PinDesc{PinDesc::Dir::input, "r"});
		layout_pin.emplace(1, PinDesc{PinDesc::Dir::input, "g"});
		layout_pin.emplace(2, PinDesc{PinDesc::Dir::input, "b"});
		pin = std::make_unique<RGB_Pin>(this);
	}
	if(!graph) {
		layout_graph = Layout{1, 1, "rgba"};
		graph = std::make_unique<RGB_Graph>(this);
	}
}

RGB::~RGB() {}

const DeviceClass RGB::getClass() const { return classname; }

/* PIN */

RGB::RGB_Pin::RGB_Pin(CDevice* device) : CDevice::PIN_Interface_C(device) {}

void RGB::RGB_Pin::setPin(PinNumber num, gpio::Tristate val) {
	if(num <= 2) {
		RGB* rgb_device = static_cast<RGB*>(device);
		rgb_device->draw_rgb(num, val == gpio::Tristate::HIGH);
	}
}

/* Graph */

RGB::RGB_Graph::RGB_Graph(CDevice* device) : CDevice::Graphbuf_Interface_C(device) {}

void RGB::RGB_Graph::initializeBufferMaybe() {
	for(PinNumber num=0; num<=2; num++) {
		RGB* rgb_device = static_cast<RGB*>(device);
		rgb_device->draw_rgb(0, false);
	}
}

void RGB::draw_rgb(PinNumber num, bool val) {
	if(num > 2) { return; }
	int extent_center = ceil(image->width()/(float)2);
	Pixel cur = getBuffer(extent_center, extent_center);
	if(num == 0) cur.r = val?255:0;
	else if(num == 1) cur.g = val?255:0;
	else cur.b = val?255:0;

	auto *img = image->bits();
	for(int x=1; x<image->width(); x++) {
		for(int y=1; y<image->height(); y++) {
			float dist = sqrt(pow(extent_center - x, 2) + pow(extent_center - y, 2));
			int norm_lumen = floor((1-dist/extent_center)*255);
			if(norm_lumen < 0) norm_lumen = 0;
			if(norm_lumen > 255) norm_lumen = 255;

			const auto offs = ((y-1) * image->width() + (x-1)) * 4; // heavily depends on rgba8888
			img[offs+0] = cur.r;
			img[offs+1] = cur.g;
			img[offs+2] = cur.b;
			img[offs+3] = (uint8_t)norm_lumen;
		}
	}
}
