#include "button.h"

#include <QKeySequence>

Button::Button(DeviceID id) : CDevice(id) {
	if (!graph) {
		layout_graph = Layout{25, 25, "rgba"};
		graph = std::make_unique<Button_Graph>(this);
	}
	if (!input) {
		input = std::make_unique<Button_Input>(this);
	}
	if (!pin) {
		layout_pin = PinLayout();
		layout_pin.emplace(1, PinDesc{PinDesc::Dir::output, "output"});
		pin = std::make_unique<Button_PIN>(this);
	}
}

Button::~Button() {}

const DeviceClass Button::getClass() const {
	return classname;
}

/* PIN Interface */

Button::Button_PIN::Button_PIN(CDevice* device) : CDevice::PIN_Interface_C(device) {}

gpio::Tristate Button::Button_PIN::getPin(PinNumber num) {
	if (num == 1) {
		Button* button_device = static_cast<Button*>(device);
		return button_device->active ? gpio::Tristate::LOW : gpio::Tristate::UNSET;
	}
	return gpio::Tristate::UNSET;
}

/* Graph Interface */

Button::Button_Graph::Button_Graph(CDevice* device) : CDevice::Graphbuf_Interface_C(device) {}

void Button::Button_Graph::initializeBufferMaybe() {
	Button* button_device = static_cast<Button*>(device);
	button_device->draw_area();
}

void Button::draw_area() {
	auto* img = image->bits();
	for (unsigned x = 0; x < layout_graph.width; x++) {
		for (unsigned y = 0; y < layout_graph.height; y++) {
			const auto offs = (y * layout_graph.width + x) * 4;  // heavily depends on rgba8888
			img[offs + 0] = active ? (uint8_t)255 : (uint8_t)0;
			img[offs + 1] = 0;
			img[offs + 2] = 0;
			img[offs + 3] = 128;
		}
	}
}

/* Input Interface */

Button::Button_Input::Button_Input(CDevice* device) : CDevice::Input_Interface_C(device) {}

void Button::Button_Input::onClick(bool active) {
	Button* button_device = static_cast<Button*>(device);
	button_device->active = active;
	button_device->draw_area();
}

void Button::Button_Input::onKeypress(int key, bool active) {
	onClick(active);
}
