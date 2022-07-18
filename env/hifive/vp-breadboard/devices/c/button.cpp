#include "button.h"

Button::Button(DeviceID id) : CDevice(id) {
	if(!graph) {
		layout_graph = Layout{36, 36, "rgba"};
		graph = std::make_unique<Button_Graph>(this);
	}
	if(!input) {
		input = std::make_unique<Button_Input>(this);
	}
}

Button::~Button() {}

const DeviceClass Button::getClass() const { return "button"; }

/* Graph Interface */

Button::Button_Graph::Button_Graph(CDevice* device) : CDevice::Graphbuf_Interface_C(device) {}

void Button::Button_Graph::initializeBufferMaybe() {
	Button* button_device = static_cast<Button*>(device);
	button_device->draw_area(false);
}

void Button::draw_area(bool active) {
	// background
	for(unsigned x=0; x<layout_graph.width; x++) {
		for(unsigned y=0; y<layout_graph.height; y++) {
			set_buf(x, y, Pixel{(active?(uint8_t)255:(uint8_t)0), 0, 0, 128});
		}
	}
}

/* Input Interface */

Button::Button_Input::Button_Input(CDevice* device) : CDevice::Input_Interface_C(device) {}

gpio::Tristate Button::Button_Input::pressed(bool active) {
	Button* button_device = static_cast<Button*>(device);
	button_device->draw_area(active);
	return active ? gpio::Tristate::LOW : gpio::Tristate::UNSET;
}
