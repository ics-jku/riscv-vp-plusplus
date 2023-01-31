#include "breadboard.h"

#include <QPainter>
#include <QTimer>

#include "raster.h"

using namespace std;

Breadboard::Breadboard() : QWidget() {
	setFocusPolicy(Qt::StrongFocus);

	QTimer* timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, [this] { update(); });
	timer->start(1000 / 30);
}

Breadboard::~Breadboard() {}

/* UPDATE */

void Breadboard::timerUpdate(gpio::State state) {
	lua_access.lock();
	for (PinMapping& c : reading_connections) {
		// TODO: Only if pin changed?
		c.dev->pin->setPin(c.device_pin, state.pins[c.global_pin] == gpio::Pinstate::HIGH ? gpio::Tristate::HIGH
		                                                                                  : gpio::Tristate::LOW);
	}

	for (PinMapping& c : writing_connections) {
		emit(setBit(c.global_pin, c.port, c.dev->pin->getPin(c.device_pin)));
	}
	lua_access.unlock();
}

void Breadboard::connectionUpdate(bool active, gpio::Port port) {
	if (active) {
		for (const auto& [id, req] : spi_channels) {
			if (port == req.port) {
				emit(registerIOF_SPI(req.global_pin, req.port, req.fun, req.noresponse));
			}
		}
		for (const auto& [id, req] : pin_channels) {
			if (port == req.port) {
				emit(registerIOF_PIN(req.global_pin, req.port, req.fun));
			}
		}
		for (const auto& [id, req] : exmc_channels) {
			if (port == req.port) {
				emit(registerIOF_EXMC(req.global_pin, req.port, req.fun));
			}
		}
	}
	// else connection lost
}

void Breadboard::writeDevice(DeviceID device) {
	lua_access.lock();
	for (PinMapping w : writing_connections) {
		if (w.dev->getID() == device) {
			emit(setBit(w.global_pin, w.port, w.dev->pin->getPin(w.device_pin)));
		}
	}
	lua_access.unlock();
}

/* CONNECTIONS */

void Breadboard::addPin(bool synchronous, gpio::PinNumber device_pin, gpio::PinNumber global, gpio::Port port,
                        std::string name, Device* device) {
	if (!device->pin) {
		cerr << "[Breadboard] Attempting to add pin connection for device '" << device->getClass()
		     << "', but device does not implement PIN interface." << endl;
		return;
	}
	const PinLayout layout = device->pin->getPinLayout();
	if (layout.find(device_pin) == layout.end()) {
		cerr << "[Breadboard] Attempting to add pin '" << (int)device_pin << "' for device " << device->getClass()
		     << " that is not offered by device" << endl;
		return;
	}
	const PinDesc& desc = layout.at(device_pin);
	if (synchronous) {
		if (desc.dir != PinDesc::Dir::input) {
			cerr << "[Breadboard] Attempting to add pin '" << (int)device_pin << "' as synchronous for device "
			     << device->getClass()
			     << ", but device labels pin not as input."
			        " This is not supported for inout-pins and unnecessary for output pins."
			     << endl;
			return;
		}
		pin_channels.emplace(device->getID(), PIN_IOF_Request{.global_pin = global,
		                                                      .device_pin = device_pin,
		                                                      .port = port,
		                                                      .fun = [this, device, device_pin](gpio::Tristate pin) {
			                                                      lua_access.lock();
			                                                      device->pin->setPin(device_pin, pin);
			                                                      lua_access.unlock();
		                                                      }});
	} else {
		PinMapping mapping =
		    PinMapping{.global_pin = global, .device_pin = device_pin, .port = port, .name = name, .dev = device};
		if (desc.dir == PinDesc::Dir::input || desc.dir == PinDesc::Dir::inout) {
			reading_connections.push_back(mapping);
		}
		if (desc.dir == PinDesc::Dir::output || desc.dir == PinDesc::Dir::inout) {
			writing_connections.push_back(mapping);
		}
	}
}

void Breadboard::addSPI(gpio::PinNumber global, gpio::Port port, bool noresponse, Device* device) {
	if (!device->spi) {
		cerr << "[Breadboard] Attempting to add SPI connection for device '" << device->getClass()
		     << "', but device does not implement SPI interface." << endl;
		return;
	}
	spi_channels.emplace(
	    device->getID(),
	    SPI_IOF_Request{
	        .global_pin = global, .port = port, .noresponse = noresponse, .fun = [this, device](gpio::SPI_Command cmd) {
		        lua_access.lock();
		        const gpio::SPI_Response ret = device->spi->send(cmd);
		        lua_access.unlock();
		        return ret;
	        }});
}

void Breadboard::addEXMC(gpio::PinNumber global, gpio::Port port, Device* device) {
	if (!device->exmc) {
		cerr << "[Breadboard] Attempting to add EXMC connection for device '" << device->getClass()
		     << "', but device does not implement EXMC interface." << endl;
		return;
	}
	exmc_channels.emplace(
	    device->getID(),
	    EXMC_IOF_Request{.global_pin = global, .port = port, .fun = [this, device](gpio::EXMC_Data data) {
		                     lua_access.lock();
		                     const gpio::EXMC_Data ret = device->exmc->send(data);
		                     lua_access.unlock();
		                     return ret;
	                     }});
}

void Breadboard::addGraphics(QPoint offset, unsigned scale, Device* device) {
	if (!device->graph) {
		cerr << "[Breadboard] Attempting to add graph buffer for device '" << device->getClass()
		     << "', but device does not implement Graph interface." << endl;
		return;
	}
	const Layout layout = device->graph->getLayout();
	// cout << "\t\t\tBuffer Layout: " << layout.width << "x" << layout.height << " pixel with type " <<
	// layout.data_type << endl;
	if (layout.width == 0 || layout.height == 0) {
		cerr << "Device " << device->getID() << " of class " << device->getClass()
		     << " "
		        "requests an invalid graphbuffer size '"
		     << layout.width << "x" << layout.height << "'" << endl;
		return;
	}
	if (layout.data_type != "rgba") {
		cerr << "Device " << device->getID() << " of class " << device->getClass()
		     << " "
		        "requested a currently unsupported graph buffer data type '"
		     << layout.data_type << "'" << endl;
		return;
	}

	device_graphics.emplace(
	    device->getID(),
	    DeviceGraphic{
	        .image = QImage(layout.width, layout.height, QImage::Format_RGBA8888), .offset = offset, .scale = scale});
}

/* QT */

void Breadboard::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	// Graph Buffers
	for (auto& [id, graphic] : device_graphics) {
		const auto& image = graphic.image;
		painter.drawImage(graphic.offset,
		                  image.scaled(image.size().width() * graphic.scale, image.size().height() * graphic.scale));
	}

	painter.end();
}

/* User input */

void Breadboard::keyPressEvent(QKeyEvent* e) {
	if (!debugmode) {
		switch (e->key()) {
			case Qt::Key_0: {
				uint8_t until = 6;
				for (uint8_t i = 0; i < 8; i++) {
					for (auto const& [_, port] : gpio::PORT_MAP) {
						if (port == gpio::Port::UNDEF) {
							continue;
						}
						emit(setBit(i, port, i < until ? gpio::Tristate::HIGH : gpio::Tristate::LOW));
					}
				}
				break;
			}
			case Qt::Key_1: {
				for (uint8_t i = 0; i < 8; i++) {
					for (auto const& [_, port] : gpio::PORT_MAP) {
						if (port == gpio::Port::UNDEF) {
							continue;
						}
						emit(setBit(i, port, gpio::Tristate::LOW));
					}
				}
				break;
			}
			default:
				for (auto const& [id, device] : devices) {
					if (device->input) {
						Keys device_keys = device->input->getKeys();
						if (device_keys.find(e->key()) != device_keys.end()) {
							lua_access.lock();
							device->input->onKeypress(e->key(), true);
							lua_access.unlock();
							writeDevice(id);
						}
					}
				}
				break;
		}
		update();
	}
}

void Breadboard::keyReleaseEvent(QKeyEvent* e) {
	if (!debugmode) {
		for (auto const& [id, device] : devices) {
			if (device->input) {
				Keys device_keys = device->input->getKeys();
				if (device_keys.find(e->key()) != device_keys.end()) {
					lua_access.lock();
					device->input->onKeypress(e->key(), false);
					lua_access.unlock();
					writeDevice(id);
				}
			}
		}
		update();
	}
}

void Breadboard::mousePressEvent(QMouseEvent* e) {
	if (!debugmode) {
		for (auto const& [id, graph] : device_graphics) {
			if (isInsideGraphic(graph, e->pos())) {
				Device& dev = *devices.at(id).get();
				if (dev.input) {
					lua_access.lock();
					dev.input->onClick(true);
					lua_access.unlock();
					writeDevice(id);
				}
				if (dev.tft_input) {
					lua_access.lock();
					dev.tft_input->onClick(true, e);
					lua_access.unlock();
					writeDevice(id);
				}
			}
		}
		update();
	}
}

void Breadboard::mouseMoveEvent(QMouseEvent* e) {
	mousePressEvent(e);
}

void Breadboard::mouseReleaseEvent(QMouseEvent* e) {
	if (!debugmode) {
		for (auto const& [id, graph] : device_graphics) {
			if (isInsideGraphic(graph, e->pos())) {
				Device& dev = *devices.at(id).get();
				if (dev.input) {
					lua_access.lock();
					dev.input->onClick(false);
					lua_access.unlock();
					writeDevice(id);
				}
				if (dev.tft_input) {
					lua_access.lock();
					dev.tft_input->onClick(false, e);
					lua_access.unlock();
					writeDevice(id);
				}
			}
		}
		update();
	}
}

bool Breadboard::isBreadboard() {
	return bkgnd_path == default_bkgnd;
}
bool Breadboard::toggleDebug() {
	debugmode = !debugmode;
	return debugmode;
}
