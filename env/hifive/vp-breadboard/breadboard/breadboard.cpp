#include "breadboard.h"
#include "raster.h"

#include <QTimer>
#include <QPainter>
#include <QMimeData>
#include <QDrag>

using namespace std;

Breadboard::Breadboard() : QWidget() {
	setFocusPolicy(Qt::StrongFocus);
	setAcceptDrops(true);

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

void Breadboard::connectionUpdate(bool active) {
	if(active) {
		for(const auto& [id, req] : spi_channels) {
			emit(registerIOF_SPI(req.gpio_offs, req.fun, req.noresponse));
		}
		for(const auto& [id, req] : pin_channels) {
			emit(registerIOF_PIN(req.gpio_offs, req.fun));
		}
	}
	// else connection lost
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

/* CONNECTIONS */

void Breadboard::addPin(bool synchronous, gpio::PinNumber device_pin, gpio::PinNumber global, std::string name, Device* device) {
	if(!device->pin) {
		cerr << "[Breadboard] Attempting to add pin connection for device '" << device->getClass() <<
				"', but device does not implement PIN interface." << endl;
		return;
	}
	const PinLayout layout = device->pin->getPinLayout();
	if(layout.find(device_pin) == layout.end()) {
		cerr << "[Breadboard] Attempting to add pin '" << (int)device_pin << "' for device " <<
				device->getClass() << " that is not offered by device" << endl;
		return;
	}
	const PinDesc& desc = layout.at(device_pin);
	if(synchronous) {
		if(desc.dir != PinDesc::Dir::input) {
			cerr << "[Breadboard] Attempting to add pin '" << (int)device_pin << "' as synchronous for device " <<
					device->getClass() << ", but device labels pin not as input."
					" This is not supported for inout-pins and unnecessary for output pins." << endl;
			return;
		}
		pin_channels.emplace(device->getID(), PIN_IOF_Request{
			.gpio_offs = translatePinToGpioOffs(global),
					.global_pin = global,
					.device_pin = device_pin,
					.fun = [this, device, device_pin](gpio::Tristate pin) {
				lua_access.lock();
				device->pin->setPin(device_pin, pin);
				lua_access.unlock();
			}
		});
	}
	else {
		PinMapping mapping = PinMapping{
			.gpio_offs = translatePinToGpioOffs(global),
					.global_pin = global,
					.device_pin = device_pin,
					.name = name,
					.dev = device
		};
		if(desc.dir == PinDesc::Dir::input
				|| desc.dir == PinDesc::Dir::inout) {
			reading_connections.push_back(mapping);
		}
		if(desc.dir == PinDesc::Dir::output
				|| desc.dir == PinDesc::Dir::inout) {
			writing_connections.push_back(mapping);
		}
	}
}

void Breadboard::addSPI(gpio::PinNumber global, bool noresponse, Device* device) {
	if(!device->spi) {
		cerr << "[Breadboard] Attempting to add SPI connection for device '" << device->getClass() <<
				"', but device does not implement SPI interface." << endl;
		return;
	}
	spi_channels.emplace(device->getID(), SPI_IOF_Request{
		.gpio_offs = translatePinToGpioOffs(global),
				.global_pin = global,
				.noresponse = noresponse,
				.fun = [this, device](gpio::SPI_Command cmd){
			lua_access.lock();
			const gpio::SPI_Response ret = device->spi->send(cmd);
			lua_access.unlock();
			return ret;
		}
	}
	);
}

void Breadboard::addGraphics(QPoint offset, unsigned scale, Device* device) {
	if(!device->graph) {
		cerr << "[Breadboard] Attempting to add graph buffer for device '" << device->getClass() <<
				"', but device does not implement Graph interface." << endl;
		return;
	}
	const Layout layout = device->graph->getLayout();
	//cout << "\t\t\tBuffer Layout: " << layout.width << "x" << layout.height << " pixel with type " << layout.data_type << endl;
	if(layout.width == 0 || layout.height == 0) {
		cerr << "Device " << device->getID() << " of class " << device->getClass() << " "
				"requests an invalid graphbuffer size '" << layout.width << "x" << layout.height << "'" << endl;
		return;
	}
	if(layout.data_type != "rgba") {
		cerr << "Device " << device->getID() << " of class " << device->getClass() << " "
				"requested a currently unsupported graph buffer data type '" << layout.data_type << "'" << endl;
		return;
	}

	device_graphics.emplace(device->getID(), DeviceGraphic{
		.image = QImage(layout.width, layout.height, QImage::Format_RGBA8888),
				.offset = offset,
				.scale = scale
	});
}


/* QT */

void Breadboard::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	if(isBreadboard()) {
		QColor dark("#101010");
		dark.setAlphaF(0.5);
		painter.setBrush(QBrush(dark));
		for(RowID row=0; row<BB_ROWS; row++) {
			for(IndexID index=0; index<BB_INDEXES; index++) {
				QPoint top_left = bb_getAbsolutePosition(row, index);
				painter.drawRect(top_left.x(), top_left.y(), BB_ICON_WIDTH, BB_ICON_WIDTH);
			}
		}
	}

	// Graph Buffers
	for (auto& [id, graphic] : device_graphics) {
		const auto& image = graphic.image;
		painter.drawImage(graphic.offset,
				image.scaled(image.width()*graphic.scale,
				image.height()*graphic.scale));
	}

	painter.end();
}

/* User input */

void Breadboard::keyPressEvent(QKeyEvent* e) {
	if(!debugmode) {
		switch (e->key()) {
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
			default:
				for(auto const& [id, device] : devices) {
					if(device->input) {
						Keys device_keys = device->input->getKeys();
						if(device_keys.find(e->key()) != device_keys.end()) {
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

void Breadboard::keyReleaseEvent(QKeyEvent* e)
{
	if(!debugmode) {
		for(auto const& [id, device] : devices) {
			if(device->input) {
				Keys device_keys = device->input->getKeys();
				if(device_keys.find(e->key()) != device_keys.end()) {
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
	if(e->button() == Qt::LeftButton) {
		for(auto const& [id, graph] : device_graphics) {
			if(isInsideGraphic(graph, e->pos())) {
				if(debugmode) { // Move
					QByteArray itemData;
					QDataStream dataStream(&itemData, QIODevice::WriteOnly);
					dataStream << QString::fromStdString(id);

					QMimeData *mimeData = new QMimeData;
					mimeData->setData(DEVICE_DRAG_TYPE, itemData);
					QDrag *drag = new QDrag(this);
					drag->setMimeData(mimeData);
					drag->setPixmap(QPixmap::fromImage(graph.image).scaled(graph.scale*graph.image.width(),
							graph.scale*graph.image.height()));
					drag->setHotSpot(QPoint(0,0));

					drag->exec(Qt::MoveAction);
				}
				else {
					Device& dev = *devices.at(id).get();
					if(dev.input) {
						lua_access.lock();
						dev.input->onClick(true);
						lua_access.unlock();
						writeDevice(id);
					}
				}
			}
		}
		update();
	}
}

void Breadboard::mouseReleaseEvent(QMouseEvent* e) {
	if(e->button() == Qt::LeftButton) {
		for(auto const& [id, graph] : device_graphics) {
			if(isInsideGraphic(graph, e->pos())) {
				if(!debugmode) {
					Device& dev = *devices.at(id).get();
					if(dev.input) {
						lua_access.lock();
						dev.input->onClick(false);
						lua_access.unlock();
						writeDevice(id);
					}
				}
			}
		}
		update();
	}
}

/* Drag and Drop */

void Breadboard::dragMoveEvent(QDragMoveEvent* e) {
	if(e->mimeData()->hasFormat(DEVICE_DRAG_TYPE) && (isBreadboard()?bb_isOnRaster(e->pos()):true)) {
		e->acceptProposedAction();
	} else {
		e->ignore();
	}
}

void Breadboard::dragEnterEvent(QDragEnterEvent* e)  {
	if(e->mimeData()->hasFormat(DEVICE_DRAG_TYPE)) {
		e->acceptProposedAction();
	} else {
		e->ignore();
	}
}

void Breadboard::dropEvent(QDropEvent* e) {
	if(e->mimeData()->hasFormat(DEVICE_DRAG_TYPE)) {
		QByteArray itemData = e->mimeData()->data(DEVICE_DRAG_TYPE);
		QDataStream dataStream(&itemData, QIODevice::ReadOnly);

		QString q_id;
		dataStream >> q_id;

		DeviceID device_id = q_id.toStdString();
		DeviceGraphic& device_graphic = device_graphics.at(device_id);

		if(!isInsideWindow(device_graphic, e->pos(), size())) {
			e->ignore();
			return;
		}

		for(const auto& [id, graphic] : device_graphics) {
			if(id == device_id) continue;
			if(isInsideGraphic(graphic, device_graphic, e->pos())) {
				e->ignore();
				return;
			}
		}

		QPoint newOffset = e->pos();

		if(isBreadboard()) {
			if(!bb_isWithinRaster(device_graphic, e->pos())) {
				e->ignore();
				return;
			} else {
				newOffset = bb_getAbsolutePosition(bb_getRow(e->pos()), bb_getIndex(e->pos()));
			}
		}

		device_graphic.offset = newOffset;

		e->acceptProposedAction();
	} else {
		e->ignore();
	}
}

bool Breadboard::isBreadboard() { return bkgnd_path == DEFAULT_PATH; }
bool Breadboard::toggleDebug() {
	debugmode = !debugmode;
	return debugmode;
}

