#include "oled.h"

OLED::OLED(DeviceID id) : CDevice(id) {
	// Pin Layout
	if(!pin) {
		layout_pin = PinLayout();
		layout_pin.emplace(1, PinDesc{PinDesc::Dir::input, "data_command"});
		pin = std::make_unique<OLED_PIN>(this);
	}
	// SPI
	if(!spi) {
		spi = std::make_unique<OLED_SPI>(this);
	}
	// Graph
	if(!graph) {
		layout_graph = Layout{132, 64, "rgba"};
		graph = std::make_unique<OLED_Graph>(this);
	}
}
OLED::~OLED() {}

const DeviceClass OLED::getClass() const { return "oled"; }

/* PIN Interface */

OLED::OLED_PIN::OLED_PIN(CDevice* device) : CDevice::PIN_Interface_C(device) {}

void OLED::OLED_PIN::setPin(PinNumber num, bool val) {
	if(num == 1) {
		OLED* oled_device = static_cast<OLED*>(device);
		oled_device->is_data = val;
	}
}

/* SPI Interface */

OLED::OLED_SPI::OLED_SPI(CDevice* device) : CDevice::SPI_Interface_C(device) {}

uint8_t getMask(uint8_t op) {
	if(op == DISPLAY_START_LINE) {
		return 0xC0;
	}
	else if(op == COL_LOW || op == COL_HIGH || op == PAGE_ADDR) {
		return 0xF0;
	}
	else if(op == DISPLAY_ON) {
		return 0xFE;
	}
	return 0xFF;
}

std::pair<uint8_t, uint8_t> match(uint8_t cmd) {
	for(uint8_t op : COMMANDS) {
		if(((cmd^op)&getMask(op)) == 0) {
			return {op, cmd&(~getMask(op))};
		}
	}
	return {NOP,0};
}

uint8_t OLED::OLED_SPI::send(uint8_t byte) {
	OLED* oled_device = static_cast<OLED*>(device);
	if(oled_device->is_data) {
		if (oled_device->state.column >= device->layout_graph.width) {
			return 0;
		}
		if (oled_device->state.page >= device->layout_graph.height/8) {
			return 0;
		}
		for(unsigned y=0; y<8; y++) {
			uint8_t pix=0;
			if(byte & 1<<y) {
				pix = 255;
			}
			device->set_buf(oled_device->state.column, (oled_device->state.page*8)+y, Pixel{pix, pix, pix, oled_device->state.contrast});
		}
		oled_device->state.column += 1;
	}
	else {
		std::pair<uint8_t, uint8_t> op_payload = match(byte);
		if(op_payload.first == DISPLAY_START_LINE) {
			return 0;
		}
		else if(op_payload.first == COL_LOW) {
			oled_device->state.column = (oled_device->state.column & 0xf0) | op_payload.second;
		}
		else if(op_payload.first == COL_HIGH) {
			oled_device->state.column = (oled_device->state.column & 0x0f) | (op_payload.second << 4);
		}
		else if(op_payload.first == PAGE_ADDR) {
			oled_device->state.page = op_payload.second;
		}
		else if(op_payload.first == DISPLAY_ON) {
			oled_device->state.display_on = op_payload.second;
		}
	}
	return 0;
}

/* Graphbuf Interface */

OLED::OLED_Graph::OLED_Graph(CDevice* device) : CDevice::Graphbuf_Interface_C(device) {}

void OLED::OLED_Graph::initializeBufferMaybe() {
	for(unsigned x=0; x<device->layout_graph.width; x++) {
		for(unsigned y=0; y<device->layout_graph.height; y++) {
			device->set_buf(x, y, Pixel{0,0,0,255});
		}
	}
}
