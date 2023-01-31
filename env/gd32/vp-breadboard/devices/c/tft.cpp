#include "tft.h"

TFT::TFT(DeviceID id) : CDevice(id) {
	// Pin Layout
	if (!pin) {
		layout_pin = PinLayout();
		layout_pin.emplace(1, PinDesc{PinDesc::Dir::input, "data_command"});
		layout_pin.emplace(2, PinDesc{PinDesc::Dir::output, "penirq"});
		pin = std::make_unique<TFT_PIN>(this);
	}
	// EXMC
	if (!exmc) {
		exmc = std::make_unique<TFT_EXMC>(this);
	}
	// SPI
	if (!spi) {
		spi = std::make_unique<TFT_SPI>(this);
	}
	// TFT Input
	if (!tft_input) {
		tft_input = std::make_unique<TFT_Input>(this);
	}
	// Graph
	if (!graph) {
		layout_graph = Layout{height, width, "rgba"};
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

gpio::Tristate TFT::TFT_PIN::getPin(PinNumber num) {
	if (num == 2) {
		TFT* tft_device = static_cast<TFT*>(device);
		return tft_device->penirq ? gpio::Tristate::LOW : gpio::Tristate::HIGH;
	}
	return gpio::Tristate::UNSET;
}

/* EXMC Interface */

TFT::TFT_EXMC::TFT_EXMC(CDevice* device) : CDevice::EXMC_Interface_C(device) {}

gpio::EXMC_Data TFT::TFT_EXMC::send(gpio::EXMC_Data data) {
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

				tft_device->state.advance();
				break;
			}
			case TFT_MADCTL: {
				uint8_t b5 = (data & 0x20) >> 5;
				uint8_t b6 = (data & 0x40) >> 6;
				uint8_t b7 = (data & 0x80) >> 7;

				tft_device->state.setCtl(b5, b6, b7);
				break;
			}
#ifdef ENABLE_SCREENSHOT
			case TFT_SCREENSHOT: {
				tft_device->image->save(QString::fromStdString("screenshot_" + std::to_string(data) + ".png"));
				break;
			}
#endif
			default:
				break;
		}
	} else {
		tft_device->current_cmd = data;
		if (data == TFT_RAMWR) {
			tft_device->state.setToStart();
		}
	}
	return 0;
}

/* SPI Interface */

TFT::TFT_SPI::TFT_SPI(CDevice* device) : CDevice::SPI_Interface_C(device) {}

gpio::SPI_Response TFT::TFT_SPI::send(gpio::SPI_Command byte) {
	TFT* tft_device = static_cast<TFT*>(device);
	uint8_t response = 0;

	if (!tft_device->txbuffer.empty()) {
		response = tft_device->txbuffer.front();
		tft_device->txbuffer.pop();
	}

	switch (byte) {
		case XPT_X: {
			tft_device->txbuffer.push(tft_device->current_x >> 5);
			tft_device->txbuffer.push((tft_device->current_x & 0x1F) << 3);
			break;
		}
		case XPT_Y: {
			tft_device->txbuffer.push(tft_device->current_y >> 5);
			tft_device->txbuffer.push((tft_device->current_y & 0x1F) << 3);
			break;
		}
		default:
			break;
	}

	return response;
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
			img[offs + 3] = 0;
		}
	}
}

void convertXY(uint16_t* x, uint16_t* y) {
	uint16_t x_tmp = *x, y_tmp = *y, xx, yy;

	xx = ((x_tmp * touchCalibration_x1) / height) + touchCalibration_x0;
	yy = ((y_tmp * touchCalibration_y1) / width) + touchCalibration_y0;

	*x = xx;
	*y = yy;
}

/* Input Interface */

TFT::TFT_Input::TFT_Input(CDevice* device) : CDevice::TFT_Input_Interface_C(device) {}

void TFT::TFT_Input::onClick(bool active, QMouseEvent* e) {
	TFT* tft_device = static_cast<TFT*>(device);
	tft_device->penirq = active;
	uint16_t tmp_x = e->pos().x() - 116;
	uint16_t tmp_y = e->pos().y() - 93;
	convertXY(&tmp_x, &tmp_y);
	tft_device->current_x = tmp_x;
	tft_device->current_y = tmp_y;
}
