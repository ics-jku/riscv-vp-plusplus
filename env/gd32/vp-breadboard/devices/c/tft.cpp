#include "tft.h"

#include <stdio.h>

TFT::TFT(DeviceID id) : CDevice(id) {
	std::cout << "hi tft" << std::endl;
	// EXMC
	if (!exmc) {
		exmc = std::make_unique<TFT_EXMC>(this);
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
