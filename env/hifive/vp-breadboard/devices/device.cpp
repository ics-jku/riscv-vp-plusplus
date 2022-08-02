#include "device.hpp"

Device::Device(const DeviceID id) : m_id(id) {}

Device::~Device() {}

const DeviceID& Device::getID() const {
	return m_id;
}

Device::PIN_Interface::~PIN_Interface() {}
Device::SPI_Interface::~SPI_Interface() {}
Device::Config_Interface::~Config_Interface() {}
Device::Graphbuf_Interface::~Graphbuf_Interface() {}
Device::Input_Interface::~Input_Interface() {}

void Device::Graphbuf_Interface::setBuffer(QImage& image, const Xoffset x, const Yoffset y, Pixel p) {
	Layout layout = getLayout();
	auto* img = image.bits();
	if(x >= layout.width || y >= layout.height) {
		std::cerr << "[Graphbuf] WARN: device write accessing graphbuffer out of bounds!" << std::endl;
		return;
	}
	const auto offs = (y * layout.width + x) * 4; // heavily depends on rgba8888
	img[offs+0] = p.r;
	img[offs+1] = p.g;
	img[offs+2] = p.b;
	img[offs+3] = p.a;
}

Pixel Device::Graphbuf_Interface::getBuffer(QImage& image, const Xoffset x, const Yoffset y) {
	Layout layout = getLayout();
	auto* img = image.bits();
	if(x >= layout.width || y >= layout.height) {
		std::cerr << "[Graphbuf] WARN: device read accessing graphbuffer out of bounds!" << std::endl;
		return Pixel{0,0,0,0};
	}
	const auto& offs = (y * layout.width + x) * 4; // heavily depends on rgba8888
	return Pixel{
		static_cast<uint8_t>(img[offs+0]),
				static_cast<uint8_t>(img[offs+1]),
				static_cast<uint8_t>(img[offs+2]),
				static_cast<uint8_t>(img[offs+3])
	};
}
