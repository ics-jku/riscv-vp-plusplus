#include "tft.h"

TFT::TFT(DeviceID id) : CDevice(id) {
	// EXMC
	if (!exmc) {
		exmc = std::make_unique<TFT_EXMC>(this);
	}
	// TFT Input
	if (!tft_input) {
		tft_input = std::make_unique<TFT_Input>(this);
	}
	// Graph
	if (!graph) {
		layout_graph = Layout{320, 240, "rgba"};
		graph = std::make_unique<TFT_Graph>(this);
	}
}
TFT::~TFT() {}

const DeviceClass TFT::getClass() const {
	return classname;
}

/* EXMC Interface */

TFT::TFT_EXMC::TFT_EXMC(CDevice* device) : CDevice::EXMC_Interface_C(device) {}

/* Graphbuf Interface */

TFT::TFT_Graph::TFT_Graph(CDevice* device) : CDevice::Graphbuf_Interface_C(device) {}

void TFT::TFT_Graph::initializeBufferMaybe() {
	auto* img = device->image->bits();
	for (unsigned x = 0; x < device->layout_graph.width; x++) {
		for (unsigned y = 0; y < device->layout_graph.height; y++) {
			const auto offs = (y * device->layout_graph.width + x) * 4;  // heavily depends on rgba8888
			img[offs + 0] = 0;
			img[offs + 1] = 0;
			img[offs + 2] = 0;
			img[offs + 3] = 255;
		}
	}
}

/* Input Interface */

TFT::TFT_Input::TFT_Input(CDevice* device) : CDevice::TFT_Input_Interface_C(device) {}

void TFT::TFT_Input::onClick(bool active, QMouseEvent* e) {
	// TODO
	TFT* tft_device = static_cast<TFT*>(device);
	tft_device->draw(active, e);
}

void TFT::draw(bool active, QMouseEvent* e) {
	auto* img = image->bits();
	const auto offs = (e->pos().y() * layout_graph.width + e->pos().x()) * 4;  // heavily depends on rgba8888
	img[offs + 0] = 255;
	img[offs + 1] = 255;
	img[offs + 2] = 255;
	img[offs + 3] = 255;
}
