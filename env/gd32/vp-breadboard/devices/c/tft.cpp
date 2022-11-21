#include "tft.h"

TFT::TFT(DeviceID id) : CDevice(id) {
	// Pin Layout
	if (!pin) {
		layout_pin = PinLayout();
		layout_pin.emplace(1, PinDesc{PinDesc::Dir::input, "data_command"});
		pin = std::make_unique<TFT_PIN>(this);
	}
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

/* PIN Interface */

TFT::TFT_PIN::TFT_PIN(CDevice* device) : CDevice::PIN_Interface_C(device) {}

void TFT::TFT_PIN::setPin(PinNumber num, gpio::Tristate val) {
	if (num == 1) {
		TFT* tft_device = static_cast<TFT*>(device);
		tft_device->is_data = val == gpio::Tristate::HIGH;
	}
}

/* EXMC Interface */

TFT::TFT_EXMC::TFT_EXMC(CDevice* device) : CDevice::EXMC_Interface_C(device) {}

void TFT::TFT_EXMC::send(gpio::EXMC_Data data) {
	TFT* tft_device = static_cast<TFT*>(device);
	if (tft_device->is_data) {
		switch (tft_device->current_cmd) {
			case TFT_CASET:
			case TFT_PASET: {
				if (tft_device->parameters.isEmpty())
					tft_device->parameters.cmd = tft_device->current_cmd;
				else if (!tft_device->parameters.isEmpty() && tft_device->parameters.cmd != tft_device->current_cmd) {
					std::cerr << "writing parameters for wrong command\n";
					tft_device->parameters.reset();
					break;
				}

				tft_device->parameters.add((uint8_t)data);
				if (tft_device->parameters.isComplete()) {
					auto start = (tft_device->parameters[0] << 8) | tft_device->parameters[1];
					auto end = (tft_device->parameters[2] << 8) | tft_device->parameters[3];

					if (tft_device->current_cmd == TFT_CASET)
						tft_device->state.setRangeColumn(start, end);
					else if (tft_device->current_cmd == TFT_PASET)
						tft_device->state.setRangePage(start, end);
					else
						break;

					tft_device->parameters.reset();
				}
				break;
			}
			case TFT_RAMWR: {
				uint8_t r = (data & 0xF800) >> 8;
				uint8_t g = (data & 0x07E0) >> 3;
				uint8_t b = (data & 0x1F) << 3;

				device->image->setPixel(tft_device->state.getPhysicalPage(), tft_device->state.getPhysicalColumn(),
				                        qRgb(r, g, b));

				if (!tft_device->state.isColumnFull()) {
					tft_device->state.incColumn();
				} else if (!tft_device->state.isPageFull()) {
					tft_device->state.setColumnStart();
					tft_device->state.incPage();
				}
				break;
			}
			case TFT_MADCTL: {
				uint8_t b5 = (data & 0b100000) >> 5;
				uint8_t b6 = (data & 0b1000000) >> 6;
				uint8_t b7 = (data & 0b10000000) << 7;

				tft_device->state.setCtl(b5, b6, b6);
				break;
			}
			default:
				break;
		}
	} else {
		tft_device->current_cmd = data;
		if (data == TFT_RAMWR) {
			tft_device->state.setColumnStart();
			tft_device->state.setPageStart();
		}
	}
}

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
	// here we need to send an interrupt to the VP
	// and somhow send the coordinates to the VP
	TFT* tft_device = static_cast<TFT*>(device);
	tft_device->draw(active, e);
}

void TFT::draw(bool active, QMouseEvent* e) {
	// TODO
	// this method should be called when we recive data via the EXMC client
	auto* img = image->bits();
	const auto offs = (e->pos().y() * layout_graph.width + e->pos().x()) * 4;  // heavily depends on rgba8888
	img[offs + 0] = 255;
	img[offs + 1] = 255;
	img[offs + 2] = 255;
	img[offs + 3] = 255;
}
