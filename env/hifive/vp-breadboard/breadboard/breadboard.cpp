#include "breadboard.h"
#include "raster.h"
#include "devices/raster.h"

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

/* DEVICE */

void Breadboard::removeDevice(DeviceID device) {
	vector<gpio::PinNumber> iofs;
	auto pin = pin_channels.find(device);
	if(pin!=pin_channels.end()) {
		iofs.push_back(pin->second.gpio_offs);
	}
	auto spi = spi_channels.find(device);
	if(spi!=spi_channels.end()) {
		iofs.push_back(spi->second.gpio_offs);
	}
	emit(closeDeviceIOFs(iofs, device));
}

void Breadboard::removeDeviceObjects(DeviceID device) {
	pin_channels.erase(device);
	spi_channels.erase(device);
	devices.erase(device);
	writing_connections.remove_if([device](PinMapping map){return map.dev->getID() == device;});
	reading_connections.remove_if([device](PinMapping map){return map.dev->getID() == device;});
}

bool Breadboard::addDevice(DeviceClass classname) {
	//	DeviceID id = "ID"; // TODO
	//	if(!addDevice(classname, id)) {
	//		return false;
	//	}
	//	Device* device = devices.at(id).get();
	//	addGraphics(QPoint(0,0), 1, device);
	//	DeviceGraphic graphic = device_graphics.at(id);
	//	if(startDrag(id, graphic, QPoint(0,0), Qt::CopyAction) == Qt::CopyAction) {
	//		return true;
	//	}
	return false;
}

bool Breadboard::addDevice(DeviceClass classname, DeviceID id) {
	if(!factory.deviceExists(classname)) {
		cerr << "[Breadboard] Add device: class name invalid." << endl;
		return false;
	}
	if(devices.find(id) != devices.end()) {
		cerr << "[Breadboard] Another device with the ID '" << id << "' is already instatiated!" << endl;
		return false;
	}
	devices.emplace(id, factory.instantiateDevice(id, classname));
	return true;
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

/* QT */

void Breadboard::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	if(isBreadboard()) {
		painter.save();
		QColor dark("#101010");
		dark.setAlphaF(0.5);
		painter.setBrush(QBrush(dark));
		for(Row row=0; row<BB_ROWS; row++) {
			for(Index index=0; index<BB_INDEXES; index++) {
				QPoint top_left = bb_getAbsolutePosition(row, index);
				painter.drawRect(top_left.x(), top_left.y(), BB_ICON_SIZE, BB_ICON_SIZE);
			}
		}
		painter.restore();
	}

	if(debugmode) {
		QColor red("red");
		red.setAlphaF(0.5);
		painter.setBrush(QBrush(red));
	}

	// Graph Buffers
	for (auto& [id, device] : devices) {
		if(device->graph) {
			QImage* buffer = device->graph->getBuffer();
			painter.drawImage(buffer->offset(), *buffer);
			if(debugmode) {
				painter.drawRect(buffer->offset().x(), buffer->offset().y(), buffer->width(), buffer->height());
			}
		}
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
	for(auto const& [id, device] : devices) {
		if(device->graph && getGraphicBounds(device->graph->getBuffer()).contains(e->pos())) {
			switch(e->button()) {
			case Qt::LeftButton:  {
				if(debugmode) { // Move
					//					QPoint hotspot = e->pos() - graph.offset;
					//					startDrag(id, graph, hotspot, Qt::MoveAction);
				}
				else { // Input
					if(device->input) {
						lua_access.lock();
						device->input->onClick(true);
						lua_access.unlock();
						writeDevice(id);
					}
				}
				break;
			}
			case Qt::RightButton: {
				if(debugmode) {
					removeDevice(id);
				}
				break;
			}
			}
		}
	}
	update();
}

void Breadboard::mouseReleaseEvent(QMouseEvent* e) {
	for(auto const& [id, device] : devices) {
		if(device->graph && getGraphicBounds(device->graph->getBuffer()).contains(e->pos())) {
			switch(e->button()) {
			case Qt::LeftButton: {
				if(!debugmode) {
					if(device->input) {
						lua_access.lock();
						device->input->onClick(false);
						lua_access.unlock();
						writeDevice(id);
					}
				}
			}
			}
		}
		update();
	}
}

/* Drag and Drop */

//Qt::DropAction Breadboard::startDrag(DeviceID device, DeviceGraphic graphic, QPoint hotspot, Qt::DropAction action) {
	//	QByteArray itemData;
	//	QDataStream dataStream(&itemData, QIODevice::WriteOnly);
	//	dataStream << QString::fromStdString(device) << hotspot;
	//
	//	QMimeData *mimeData = new QMimeData;
	//	mimeData->setData(DEVICE_DRAG_TYPE, itemData);
	//	QDrag *drag = new QDrag(this);
	//	drag->setMimeData(mimeData);
	//	drag->setPixmap(pixmap);
	////	drag->setPixmap(QPixmap::fromImage(graphic.image).scaled(graphic.scale*graphic.image.width(),
	////			graphic.scale*graphic.image.height()));
	//	drag->setHotSpot(hotspot);
	//
	//	return drag->exec(action);
//	return Qt::MoveAction;
//}

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
	//	if(e->mimeData()->hasFormat(DEVICE_DRAG_TYPE)) {
	//		QByteArray itemData = e->mimeData()->data(DEVICE_DRAG_TYPE);
	//		QDataStream dataStream(&itemData, QIODevice::ReadOnly);
	//
	//		QString q_id;
	//		QPoint hotspot;
	//		dataStream >> q_id >> hotspot;
	//
	//		DeviceID device_id = q_id.toStdString();
	//		DeviceGraphic& device_graphic = device_graphics.at(device_id);
	//
	//		QPoint upper_left = e->pos() - hotspot;
	//
	//		if(!isInsideWindow(device_graphic, upper_left, size())) {
	//			cerr << "[Breadboard] New device position invalid: Device may not leave window view." << endl;
	//			e->ignore();
	//			return;
	//		}
	//
	//		for(const auto& [id, graphic] : device_graphics) {
	//			if(id == device_id) continue;
	//			if(isInsideGraphic(graphic, device_graphic, upper_left)) {
	//				cerr << "[Breadboard] New device position invalid: Overlaps with other device." << endl;
	//				e->ignore();
	//				return;
	//			}
	//		}
	//
	//		if(isBreadboard()) {
	//			if(bb_isWithinRaster(device_graphic, upper_left)) {
	//				QPoint dropPositionRaster = bb_getAbsolutePosition(bb_getRow(e->pos()), bb_getIndex(e->pos()));
	//				upper_left = dropPositionRaster - device_getAbsolutePosition(device_getRow(hotspot), device_getIndex(hotspot));
	//			} else {
	//				cerr << "[Breadboard] New device position invalid: Device should be at least partially on raster." << endl;
	//				e->ignore();
	//				return;
	//			}
	//		}
	//
	//		device_graphic.offset = upper_left;
	//
	//		e->acceptProposedAction();
	//	} else {
	//		cerr << "[Breadboard] New device position invalid: Invalid Mime data type." << endl;
	//		e->ignore();
	//	}
}

bool Breadboard::isBreadboard() { return bkgnd_path == DEFAULT_PATH; }
bool Breadboard::toggleDebug() {
	debugmode = !debugmode;
	return debugmode;
}

