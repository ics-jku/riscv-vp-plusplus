#include "breadboard.h"
#include "raster.h"

using namespace std;

Breadboard::Breadboard(QWidget* parent) : QWidget(parent) {
	setFocusPolicy(Qt::StrongFocus);

	connect(this, &Breadboard::repaintSignal, this, [this]{repaint();});
}

Breadboard::~Breadboard() {
}

/* UPDATE */

void Breadboard::timerUpdate(gpio::State state) {
	lua_access.lock();
	for (auto& c : reading_connections) {
		// TODO: Only if pin changed?
		c.dev->pin->setPin(c.device_pin, state.pins[c.gpio_offs] == gpio::Pinstate::HIGH ? gpio::Tristate::HIGH : gpio::Tristate::LOW);
	}

	for (auto& c : writing_connections) {
		emit(setBit(c.gpio_offs, c.dev->pin->getPin(c.device_pin)));
	}
	lua_access.unlock();
	this->update();
}

void Breadboard::reconnected() { // new gpio connection
	for(const auto& [id, req] : spi_channels) {
		emit(registerIOF_SPI(req.gpio_offs, req.fun, req.noresponse));
	}
	for(const auto& [id, req] : pin_channels) {
		emit(registerIOF_PIN(req.gpio_offs, req.fun));
	}
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
	this->update();
	// scout << "Yee, keypress" << endl;

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
				for(pair<DeviceID,Device*> dev_it : devices) {
					if(dev_it.second->input) {
						lua_access.lock();
						dev_it.second->input->key(e->key(), true);
						lua_access.unlock();
					}
				}
				break;
		}
	}
	this->update();
}

void Breadboard::keyReleaseEvent(QKeyEvent* e)
{
	for(pair<DeviceID,Device*> dev_it : devices) {
		if(dev_it.second->input) {
			lua_access.lock();
			dev_it.second->input->key(e->key(), false);
			lua_access.unlock();
		}
	}
	this->update();
}

void Breadboard::mousePressEvent(QMouseEvent* e) {
	for(pair<DeviceID,DeviceGraphic> graph_it : device_graphics) {
		if(graphicContainsPoint(graph_it.second, e->pos())) {
			Device* dev = devices.at(graph_it.first);
			if(dev->input) {
				lua_access.lock();
				dev->input->mouse(true);
				lua_access.unlock();
			}
		}
	}
	this->update();
}

void Breadboard::mouseReleaseEvent(QMouseEvent* e) {
	for(pair<DeviceID,DeviceGraphic> graph_it : device_graphics) {
		if(graphicContainsPoint(graph_it.second, e->pos())) {
			Device* dev = devices.at(graph_it.first);
			if(dev->input) {
				lua_access.lock();
				dev->input->mouse(false);
				lua_access.unlock();
			}
		}
	}
	this->update();
}

bool Breadboard::isBreadboard() { return breadboard; }

