#include "button.h"

Button::Button(DeviceID id) : CDevice(id) {
	if(!graph) {
		layout_graph = Layout{35, 35, "rgba"};
		graph = std::make_unique<Button_Graph>(this);
	}
	if(!input) {
		input = std::make_unique<Button_Input>(this);
	}
	if(!pin) {
		layout_pin = PinLayout();
		layout_pin.emplace(1, PinDesc{PinDesc::Dir::output, "output"});
		pin = std::make_unique<Button_PIN>(this);
	}
	if(!conf) {
		config = Config();
		conf = std::make_unique<Config_Interface_C>(this);
	}
}

Button::~Button() {}

const DeviceClass Button::getClass() const { return classname; }

/* PIN Interface */

Button::Button_PIN::Button_PIN(CDevice* device) : CDevice::PIN_Interface_C(device) {}

gpio::Tristate Button::Button_PIN::getPin(PinNumber num) {
	if(num == 1) {
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
	for(unsigned x=0; x<layout_graph.width; x++) {
		for(unsigned y=0; y<layout_graph.height; y++) {
			setBuffer(x, y, Pixel{(active?(uint8_t)255:(uint8_t)0), 0, 0, 128});
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
	if(device->conf) {
		Config::iterator keybinding_it = device->conf->getConfig().find("keybinding");
		if(keybinding_it != device->conf->getConfig().end() &&
				keybinding_it->second.type == ConfigElem::Type::integer &&
				keybinding_it->second.value.integer == key) {
			onClick(active);
		}
	}
}
