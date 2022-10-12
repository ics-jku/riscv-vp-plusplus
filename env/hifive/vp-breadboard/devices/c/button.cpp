#include "button.h"

#include <QKeySequence>

Button::Button(DeviceID id) : CDevice(id) {
	graph = std::make_unique<Button_Graph>(this);
	input = std::make_unique<Button_Input>(this);
	pin = std::make_unique<Button_PIN>(this);
}

Button::~Button() {}

const DeviceClass Button::getClass() const { return classname; }

/* PIN Interface */

Button::Button_PIN::Button_PIN(CDevice* device) : CDevice::PIN_Interface_C(device) {
	layout = PinLayout();
	layout.emplace(1, PinDesc{PinDesc::Dir::output, "output"});
}

gpio::Tristate Button::Button_PIN::getPin(PinNumber num) {
	if(num == 1) {
		Button* button_device = static_cast<Button*>(device);
		return button_device->active ? gpio::Tristate::LOW : gpio::Tristate::UNSET;
	}
	return gpio::Tristate::UNSET;
}

/* Graph Interface */

Button::Button_Graph::Button_Graph(CDevice* device) : CDevice::Graphbuf_Interface_C(device) {
	layout = Layout{2,2,"rgba"};
}

void Button::Button_Graph::initializeBuffer() {
	Button_Graph* button_graph = static_cast<Button_Graph*>(device->graph.get());
	button_graph->draw();
}

void Button::Button_Graph::draw() {
	Button* button_device = static_cast<Button*>(device);
	auto *img = buffer.bits();
	for(unsigned x=0; x<buffer.width(); x++) {
		for(unsigned y=0; y<buffer.height(); y++) {
			const auto offs = (y * buffer.width() + x) * 4; // heavily depends on rgba8888
			img[offs+0] = button_device->active?(uint8_t)255:(uint8_t)0;
			img[offs+1] = 0;
			img[offs+2] = 0;
			img[offs+3] = 128;
		}
	}
}

/* Input Interface */

Button::Button_Input::Button_Input(CDevice* device) : CDevice::Input_Interface_C(device) {}

void Button::Button_Input::onClick(bool active) {
	Button* button_device = static_cast<Button*>(device);
	button_device->active = active;
	Button_Graph* button_graph = static_cast<Button_Graph*>(device->graph.get());
	button_graph->draw();
}

void Button::Button_Input::onKeypress(int key, bool active) {
	onClick(active);
}
