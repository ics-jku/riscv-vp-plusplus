#include "breadboard.h"
#include "raster.h"

using namespace std;

Breadboard::Breadboard(QWidget* parent) : QWidget(parent) {
	setFocusPolicy(Qt::StrongFocus);

	QTimer *timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, [this]{update();});
	timer->start(1000/30);
}

Breadboard::~Breadboard() {
}

/* UPDATE */

void Breadboard::timerUpdate(gpio::State state) {
	lua_access.lock();
	for (PinMapping& c : reading_connections) {
		// TODO: Only if pin changed?
		c.dev->pin->setPin(c.device_pin, state.pins[c.gpio_offs] == gpio::Pinstate::HIGH ? gpio::Tristate::HIGH : gpio::Tristate::LOW);
	}

	for (PinMapping& c : writing_connections) {
		emit(setBit(c.gpio_offs, c.dev->pin->getPin(c.device_pin)));
	}
	lua_access.unlock();
}

void Breadboard::reconnected() { // new gpio connection
	for(const auto& [id, req] : spi_channels) {
		emit(registerIOF_SPI(req.gpio_offs, req.fun, req.noresponse));
	}
	for(const auto& [id, req] : pin_channels) {
		emit(registerIOF_PIN(req.gpio_offs, req.fun));
	}
}

void Breadboard::writeDevice(DeviceID device) {
	lua_access.lock();
	for(PinMapping w : writing_connections) {
		if(w.dev->getID() == device) {
			emit(setBit(w.gpio_offs, w.dev->pin->getPin(w.device_pin)));
		}
	}
	lua_access.unlock();
}

/* QT */

void Breadboard::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	// Graph Buffers
	for (auto& [id, graphic] : device_graphics) {
		const auto& image = graphic.image;
		painter.drawImage(graphic.offset,
				image.scaled(image.size().width()*graphic.scale,
				image.size().height()*graphic.scale));
	}

	painter.end();
}

/* User input */

void Breadboard::keyPressEvent(QKeyEvent* e) {
	if(debugmode)
	{
		switch(e->key())
		{
		case Qt::Key_Space:
			cout << "Debug mode off" << endl;
			debugmode = 0;
			break;
		default:
			break;
		}
	}
	else
	{
		//normal mode
		switch (e->key()) {
			case Qt::Key_Escape:
			case Qt::Key_Q:
				emit(destroyConnection());
				QApplication::quit();
				break;
			case Qt::Key_0: {
				uint8_t until = 6;
				for (uint8_t i = 0; i < 8; i++) {
					emit(setBit(i, i < until ? gpio::Tristate::HIGH : gpio::Tristate::LOW));
				}
				break;
			}
			case Qt::Key_1: {
				for (uint8_t i = 0; i < 8; i++) {
					emit(setBit(i, gpio::Tristate::LOW));
				}
				break;
			}
			case Qt::Key_Space:
				cout << "Set Debug mode" << endl;
				debugmode = true;
				break;
			default:
				for(auto const& [id, device] : devices) {
					if(device->input) {
						lua_access.lock();
						device->input->onKeypress(e->key(), true);
						lua_access.unlock();
						writeDevice(id);
					}
				}
				break;
		}
	}
	update();
}

void Breadboard::keyReleaseEvent(QKeyEvent* e)
{
	for(auto const& [id, device] : devices) {
		if(device->input) {
			lua_access.lock();
			device->input->onKeypress(e->key(), false);
			lua_access.unlock();
			writeDevice(id);
		}
	}
	update();
}

void Breadboard::mousePressEvent(QMouseEvent* e) {
	for(auto const& [id, graph] : device_graphics) {
		if(isInsideGraphic(graph, e->pos())) {
			Device* dev = devices.at(id).get();
			if(dev->input) {
				lua_access.lock();
				dev->input->onClick(true);
				lua_access.unlock();
				writeDevice(id);
			}
		}
	}
	update();
}

void Breadboard::mouseReleaseEvent(QMouseEvent* e) {
	for(auto const& [id, graph] : device_graphics) {
		if(isInsideGraphic(graph, e->pos())) {
			Device* dev = devices.at(id).get();
			if(dev->input) {
				lua_access.lock();
				dev->input->onClick(false);
				lua_access.unlock();
				writeDevice(id);
			}
		}
	}
	update();
}

bool Breadboard::isBreadboard() { return breadboard; }

