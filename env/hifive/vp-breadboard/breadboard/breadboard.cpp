#include "breadboard.h"
#include "raster.h"
#include "devices/raster.h"

#include <QTimer>
#include <QPainter>
#include <QMimeData>
#include <QDrag>
#include <QInputDialog>

using namespace std;

Breadboard::Breadboard() : QWidget() {
	setFocusPolicy(Qt::StrongFocus);
	setAcceptDrops(true);

	QTimer *timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, [this]{update();});
	timer->start(1000/30);

	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &QWidget::customContextMenuRequested, this, &Breadboard::openDeviceMenu);
	device_menu = new QMenu(this);
	QAction* delete_device = new QAction("Delete");
	connect(delete_device, &QAction::triggered, this, &Breadboard::removeActiveDevice);
	device_menu->addAction(delete_device);
	QAction* scale_device = new QAction("Scale");
	connect(scale_device, &QAction::triggered, this, &Breadboard::scaleActiveDevice);
	device_menu->addAction(scale_device);
	QAction* keybinding_device = new QAction("Edit Keybindings");
	connect(keybinding_device, &QAction::triggered, this, &Breadboard::keybindingActiveDevice);
	device_menu->addAction(keybinding_device);
	QAction* config_device = new QAction("Edit configurations");
	connect(config_device, &QAction::triggered, this, &Breadboard::configActiveDevice);
	device_menu->addAction(config_device);

	device_keys = new KeybindingDialog(this);
	device_config = new ConfigDialog(this);
	error_dialog = new QErrorMessage(this);
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

void Breadboard::writeDevice(DeviceID id) {
	lua_access.lock();
	for(PinMapping w : writing_connections) {
		if(w.dev->getID() == id) {
			emit(setBit(w.gpio_offs, w.dev->pin->getPin(w.device_pin)));
		}
	}
	lua_access.unlock();
}

/* DEVICE */

std::list<DeviceClass> Breadboard::getAvailableDevices() { return factory.getAvailableDevices(); }

void Breadboard::removeDevice(DeviceID id) {
	vector<gpio::PinNumber> iofs;
	auto pin = pin_channels.find(id);
	if(pin!=pin_channels.end()) {
		iofs.push_back(pin->second.gpio_offs);
	}
	auto spi = spi_channels.find(id);
	if(spi!=spi_channels.end()) {
		iofs.push_back(spi->second.gpio_offs);
	}
	emit(closeDeviceIOFs(iofs, id));
}

void Breadboard::removeDeviceObjects(DeviceID id) {
	pin_channels.erase(id);
	spi_channels.erase(id);
	writing_connections.remove_if([id](PinMapping map){return map.dev->getID() == id;});
	reading_connections.remove_if([id](PinMapping map){return map.dev->getID() == id;});
	devices.erase(id);
}

bool Breadboard::addDevice(DeviceClass classname) {
	DeviceID id;
	if(devices.size() < std::numeric_limits<unsigned>::max()) {
		id = std::to_string(devices.size());
	}
	else {
		std::set<unsigned> used_ids;
		for(auto const& [id_it, device_it] : devices) {
			used_ids.insert(std::stoi(id_it));
		}
		unsigned id_int = 0;
		for(unsigned used_id : used_ids) {
			if(used_id > id_int)
				break;
			id_int++;
		}
		id = std::to_string(id_int);
	}

	if(!addDevice(classname, id)) {
		cerr << "[Breadboard] Could not add new device " << classname << endl;
		return false;
	}
	Device* device = devices.at(id).get();
	if(!device->graph) return true;
	device->graph->createBuffer(QPoint(0,0));
	device->graph->setScale(0);
	if(startDrag(id, QPoint(0,0), Qt::CopyAction) == Qt::CopyAction) {
		return true;
	}
	cerr << "[Breadboard] Could not add new device " << classname << endl;
	return false;
}

bool Breadboard::addDevice(DeviceClass classname, DeviceID id) {
	if(!id.size()) {
		cerr << "[Breadboard] Device ID cannot be empty string!" << endl;
	}
	if(!factory.deviceExists(classname)) {
		cerr << "[Breadboard] Add device: class name '" << classname << "' invalid." << endl;
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
			QImage buffer = device->graph->getBuffer();
			painter.drawImage(buffer.offset(), buffer.scaled(buffer.width()*device->graph->getScale(),
					buffer.height()*device->graph->getScale()));
			if(debugmode) {
				painter.drawRect(getGraphicBounds(buffer, device->graph->getScale()));
			}
		}
	}

	painter.end();
}

/* Context Menu */

void Breadboard::openDeviceMenu(QPoint pos) {
	for(auto const& [id, device] : devices) {
		if(device->graph && getGraphicBounds(device->graph->getBuffer(), device->graph->getScale()).contains(pos)) {
			active_device_menu = id;
			device_menu->popup(mapToGlobal(pos));
			return;
		}
	}
}

void Breadboard::removeActiveDevice() {
	if(!active_device_menu.size()) return;
	removeDevice(active_device_menu);
	active_device_menu = "";
}

void Breadboard::scaleActiveDevice() {
	auto device = devices.find(active_device_menu);
	if(device == devices.end() || !device->second->graph) {
		error_dialog->showMessage("Device does not implement graph interface.");
		return;
	}
	bool ok;
	int scale = QInputDialog::getInt(this, "Input new scale value", "Scale", 1, 1, 10, 1, &ok);
	if(ok) {
		device->second->graph->setScale(scale);
	}
	active_device_menu = "";
}

void Breadboard::keybindingActiveDevice() {
	auto device = devices.find(active_device_menu);
	if(device == devices.end() || !device->second->input) {
		error_dialog->showMessage("Device does not implement input interface.");
		return;
	}
	device_keys->setKeys(device->second->input->getKeys());
	device_keys->exec();
	active_device_menu = "";
}

void Breadboard::configActiveDevice() {
	auto device = devices.find(active_device_menu);
	if(device == devices.end() || !device->second->conf) {
		error_dialog->showMessage("Device does not implement config interface.");
		return;
	}
	device_config->setConfig(device->second->conf->getConfig());
	device_config->exec();
	active_device_menu = "";
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
		case Qt::Key_O: {
			addDevice("oled");
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
		if(device->graph && getGraphicBounds(device->graph->getBuffer(), device->graph->getScale()).contains(e->pos())) {
			if(e->button() == Qt::LeftButton)  {
				if(debugmode) { // Move
					QPoint hotspot = e->pos() - device->graph->getBuffer().offset();
					startDrag(id, hotspot, Qt::MoveAction);
				}
				else { // Input
					if(device->input) {
						lua_access.lock();
						device->input->onClick(true);
						lua_access.unlock();
						writeDevice(id);
					}
				}
				return;
			}
		}
	}
	update();
}

void Breadboard::mouseReleaseEvent(QMouseEvent* e) {
	for(auto const& [id, device] : devices) {
		if(device->graph && getGraphicBounds(device->graph->getBuffer(), device->graph->getScale()).contains(e->pos())) {
			if(e->button() == Qt::LeftButton) {
				if(!debugmode) {
					if(device->input) {
						lua_access.lock();
						device->input->onClick(false);
						lua_access.unlock();
						writeDevice(id);
					}
				}
				return;
			}
		}
	}
	update();
}

/* Drag and Drop */

Qt::DropAction Breadboard::startDrag(DeviceID id, QPoint hotspot, Qt::DropAction action) {
	Device* device = devices.at(id).get();
	if(!device || !device->graph) return Qt::IgnoreAction;
	QByteArray itemData;
	QDataStream dataStream(&itemData, QIODevice::WriteOnly);
	dataStream << QString::fromStdString(id) << hotspot;

	QImage buffer = device->graph->getBuffer();
	unsigned scale = device->graph->getScale();
	if(!scale) scale = 1;
	buffer = buffer.scaled(buffer.width()*scale, buffer.height()*scale);

	QMimeData *mimeData = new QMimeData;
	mimeData->setData(DEVICE_DRAG_TYPE, itemData);
	QDrag *drag = new QDrag(this);
	drag->setMimeData(mimeData);
	drag->setPixmap(QPixmap::fromImage(buffer));
	drag->setHotSpot(hotspot);

	return drag->exec(action);
}

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
		QPoint hotspot;
		dataStream >> q_id >> hotspot;

		DeviceID device_id = q_id.toStdString();
		Device* device = devices.at(device_id).get();
		if(!device->graph) return;

		unsigned scale = device->graph->getScale();
		if(!scale) scale = 1;

		QPoint upper_left = e->pos() - hotspot;
		QRect device_bounds = QRect(upper_left, getGraphicBounds(device->graph->getBuffer(), scale).size());

		if(!rect().contains(device_bounds)) {
			cerr << "[Breadboard] New device position invalid: Device may not leave window view." << endl;
			e->ignore();
			return;
		}

		for(const auto& [id_it, device_it] : devices) {
			if(id_it == device_id) continue;
			if(device_it->graph && getGraphicBounds(device_it->graph->getBuffer(), device_it->graph->getScale()).intersects(device_bounds)) {
				cerr << "[Breadboard] New device position invalid: Overlaps with other device." << endl;
				e->ignore();
				return;
			}
		}

		if(isBreadboard()) {
			if(bb_getRasterBounds().intersects(device_bounds)) {
				QPoint dropPositionRaster = bb_getAbsolutePosition(bb_getRow(e->pos()), bb_getIndex(e->pos()));
				upper_left = dropPositionRaster - device_getAbsolutePosition(device_getRow(hotspot), device_getIndex(hotspot));
			} else {
				cerr << "[Breadboard] New device position invalid: Device should be at least partially on raster." << endl;
				e->ignore();
				return;
			}
		}

		device->graph->getBuffer().setOffset(upper_left);
		device->graph->setScale(scale);

		e->acceptProposedAction();
	} else {
		cerr << "[Breadboard] New device position invalid: Invalid Mime data type." << endl;
		e->ignore();
	}
}

bool Breadboard::isBreadboard() { return bkgnd_path == DEFAULT_PATH; }
bool Breadboard::toggleDebug() {
	debugmode = !debugmode;
	return debugmode;
}

