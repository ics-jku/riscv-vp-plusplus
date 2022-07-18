#include "breadboard.h"

using namespace std;

constexpr bool debug_logging = false;

Breadboard::Breadboard(QWidget* parent) : QWidget(parent) {
	QString bkgnd_path = ":/img/virtual_breadboard.png";
	QSize bkgnd_size = QSize(486, 233);

	setStyleSheet("background-image: url("+bkgnd_path+");");
	setFixedSize(bkgnd_size);

	setFocusPolicy(Qt::StrongFocus);
}

Breadboard::~Breadboard() {
}

/* UPDATE */

void Breadboard::timerUpdate(gpio::State state) {
	lua_access.lock();
	for (auto& c : reading_connections) {
		// TODO: Only if pin changed?
		c.dev->pin->setPin(c.device_pin, state.pins[c.gpio_offs] == gpio::Pinstate::HIGH ? true : false);
	}
	lua_access.unlock();

	lua_access.lock();
	for (auto& c : writing_connections) {
		emit(setBit(c.gpio_offs, c.dev->pin->getPin(c.device_pin) ? gpio::Tristate::HIGH : gpio::Tristate::LOW));
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

/* JSON */

bool Breadboard::loadConfigFile(QString file, string additional_device_dir, bool overwrite_integrated_devices) {

	if(additional_device_dir.size() != 0){
		factory.scanAdditionalDir(additional_device_dir, overwrite_integrated_devices);
	}

	QFile confFile(file);
	if (!confFile.open(QIODevice::ReadOnly)) {
		cerr << "Could not open config file " << endl;
		return false;
	}

	QByteArray  raw_file = confFile.readAll();
	/*
	    for(unsigned i = 0; i < raw_file.size(); i++)
	    {
	    	cout << raw_file.data()[i];
	    }
	    cout << endl;
	 */
	QJsonParseError error;
	QJsonDocument json_doc = QJsonDocument::fromJson(raw_file, &error);
	if(json_doc.isNull())
	{
		cerr << "Config seems to be invalid: ";
		cerr << error.errorString().toStdString() << endl;
		return false;
	}
	QJsonObject config_root = json_doc.object();

	// TODO: Let every registered "config object" decide its own default values

	if(config_root.contains("devices") && config_root["devices"].isArray()) {
		auto device_descriptions = config_root["devices"].toArray();
		devices.reserve(device_descriptions.count());
		if(debug_logging)
			cout << "[config loader] reserving space for " << device_descriptions.count() << " devices." << endl;
		for(const auto& device_description : device_descriptions) {
			const auto device_desc = device_description.toObject();
			const auto& classname = device_desc["class"].toString("undefined").toStdString();
			const auto& id = device_desc["id"].toString("undefined").toStdString();

			if(!factory.deviceExists(classname)) {
				cerr << "[config loader] device '" << classname << "' does not exist" << endl;

				continue;
			}
			if(devices.find(id) != devices.end()) {
				cerr << "[config loader] Another device with the ID '" << id << "' is already instatiated!" << endl;
				continue;
			}
			devices.emplace(id, factory.instantiateDevice(id, classname));
			Device* device = devices.at(id);

			if(device_desc.contains("spi") && device_desc["spi"].isObject()) {
				if(!device->spi) {
					cerr << "[config loader] config for device '" << classname << "' sets"
							" an SPI interface, but device does not implement it" << endl;
					continue;
				}
				const auto spi = device_desc["spi"].toObject();
				if(!spi.contains("cs_pin")) {
					cerr << "[config loader] config for device '" << classname << "' sets"
							" an SPI interface, but does not set a cs_pin." << endl;
					continue;
				}
				const gpio::PinNumber cs = spi["cs_pin"].toInt();
				const bool noresponse = spi["noresponse"].toBool(true);
				spi_channels.emplace(id, SPI_IOF_Request{
					.gpio_offs = translatePinToGpioOffs(cs),
							.global_pin = cs,
							.noresponse = noresponse,
							.fun = [this, device](gpio::SPI_Command cmd){
						lua_access.lock();
						const auto ret = device->spi->send(cmd);
						lua_access.unlock();
						return ret;
					}
				}
				);
			}

			if(device_desc.contains("pins") && device_desc["pins"].isArray()) {
				if(!device->pin) {
					cerr << "[config loader] config for device '" << classname << "' sets"
							" an PIN interface, but device does not implement it" << endl;
					continue;
				}
				const auto pinLayout = device->pin->getPinLayout();
				auto pin_descriptions = device_desc["pins"].toArray();
				for (const auto& pin_desc : pin_descriptions) {
					const auto pin = pin_desc.toObject();
					if(!pin.contains("device_pin") ||
							!pin.contains("global_pin")) {
						cerr << "[config loader] config for device '" << classname << "' is"
								" missing device_pin or global_pin mappings" << endl;
						continue;
					}
					const gpio::PinNumber device_pin = pin["device_pin"].toInt();
					const gpio::PinNumber global_pin = pin["global_pin"].toInt();
					const bool synchronous = pin["synchronous"].toBool(false);
					const string pin_name = pin["name"].toString("undef").toStdString();

					if(pinLayout.find(device_pin) == pinLayout.end()) {
						cerr << "[config loader] config for device '" << classname << "' names a pin " <<
								(int)device_pin << " that is not offered by device" << endl;
						continue;
					}

					//cout << "Mapping " << device.getID() << "'s pin " << (int)device_pin <<
					//		" to global pin " << (int)global_pin << endl;

					const auto& pin_l = pinLayout.at(device_pin);
					if(synchronous) {
						// TODO FIXME : Currently, only one synchronous pin seems to work!
						if(pin_l.dir != PinDesc::Dir::input) {
							cerr << "[config loader] config for device '" << classname << "' maps pin " <<
									(int)device_pin << " as syncronous, but device labels pin not as input."
									" This is not supported for inout-pins and unnecessary for output pins." << endl;
							continue;
						}
						pin_channels.emplace(id, PIN_IOF_Request{
							.gpio_offs = translatePinToGpioOffs(global_pin),
									.global_pin = global_pin,
									.fun = [this, device, device_pin](gpio::Tristate pin) {
								lua_access.lock();
								device->pin->setPin(device_pin, pin == gpio::Tristate::HIGH ? 1 : 0);
								lua_access.unlock();
							}
						});
					} else {
						PinMapping mapping = PinMapping{
							.gpio_offs = translatePinToGpioOffs(global_pin),
									.global_pin = global_pin,
									.device_pin = device_pin,
									.name = pin_name,
									.dev = device
						};
						if(pin_l.dir == PinDesc::Dir::input
								|| pin_l.dir == PinDesc::Dir::inout) {
							reading_connections.push_back(mapping);
						}
						if(pin_l.dir == PinDesc::Dir::output
								|| pin_l.dir == PinDesc::Dir::inout) {
							writing_connections.push_back(mapping);
						}
					}
				}
			}

			if(device_desc.contains("graphics") && device_desc["graphics"].isObject()) {
				if(!device->graph) {
					cerr << "[config loader] config for device '" << classname << "' sets"
							" a graph buffer interface, but device does not implement it" << endl;
					continue;
				}
				const auto layout = device->graph->getLayout();
				//cout << "\t\t\tBuffer Layout: " << layout.width << "x" << layout.height << " pixel with type " << layout.data_type << endl;
				if(layout.width == 0 || layout.height == 0) {
					cerr << "Device " << id << " of class " << classname << " "
							"requests an invalid graphbuffer size '" << layout.width << "x" << layout.height << "'" << endl;
					continue;
				}
				if(layout.data_type != "rgba") {
					cerr << "Device " << id << " of class " << classname << " "
							"requested a currently unsupported graph buffer data type '" << layout.data_type << "'" << endl;
					continue;
				}

				const auto graphics_desc = device_desc["graphics"].toObject();
				if(!(graphics_desc.contains("offs") && graphics_desc["offs"].isArray())) {
					cerr << "[config loader] config for device '" << classname << "' sets"
							" a graph buffer interface, but no valid offset given" << endl;
					continue;
				}
				const auto offs_desc = graphics_desc["offs"].toArray();
				if(offs_desc.size() != 2) {
					cerr << "[config loader] config for device '" << classname << "' sets"
							" a graph buffer interface, but offset is malformed (needs x,y, is size " << offs_desc.size() << ")" << endl;
					continue;
				}

				QPoint offs(offs_desc[0].toInt(), offs_desc[1].toInt());
				const unsigned scale = graphics_desc["scale"].toInt(1);

				device_graphics.emplace(id, DeviceGraphic{
					.image = QImage(layout.width, layout.height, QImage::Format_RGBA8888),
							.offset = offs,
							.scale = scale
				});

				// setting up the image buffer and its functions
				auto& new_buffer = device_graphics.at(id).image;
				memset(new_buffer.bits(), 0x8F, new_buffer.sizeInBytes());

				device->graph->registerSetBuf([&new_buffer, layout, id](const Xoffset x, const Yoffset y, Pixel p){
					//cout << "setBuf at " << (int) x << "x" << (int) y <<
					//		": (" << (int)p.r << "," << (int)p.g << "," << (int)p.b << "," << (int)p.a << ")" << endl;
					auto* img = new_buffer.bits();
					if(x >= layout.width || y >= layout.height) {
						cerr << "[Graphbuf] WARN: device " << id << " write accessing graphbuffer out of bounds!" << endl;
						return;
					}
					const auto offs = (y * layout.width + x) * 4; // heavily depends on rgba8888
					img[offs+0] = p.r;
					img[offs+1] = p.g;
					img[offs+2] = p.b;
					img[offs+3] = p.a;
				}
				);
				device->graph->registerGetBuf([&new_buffer, layout, id](const Xoffset x, const Yoffset y){
					auto* img = new_buffer.bits();
					if(x >= layout.width || y >= layout.height) {
						cerr << "[Graphbuf] WARN: device " << id << " read accessing graphbuffer out of bounds!" << endl;
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
				);
				// only called if lua implements the function
				device->graph->initializeBufferMaybe();
			}
		}

		if(debug_logging) {
			cout << "Instatiated devices:" << endl;
			for (auto& [id, device] : devices) {
				cout << "\t" << id << " of class " << device->getClass() << endl;

				if(device->pin)
					cout << "\t\timplements PIN" << endl;
				if(device->spi)
					cout << "\t\timplements SPI" << endl;
				if(device->conf)
					cout << "\t\timplements conf" << endl;
				if(device->graph)
					cout << "\t\timplements graphbuf (" << device->graph->getLayout().width << "x" <<
					device->graph->getLayout().height << " pixel)"<< endl;
			}

			cout << "Active pin connections:" << endl;
			cout << "\tReading (async): " << reading_connections.size() << endl;
			for(auto& conn : reading_connections){
				cout << "\t\t" << conn.dev->getID() << " (" << conn.name << "): global pin " << (int)conn.global_pin <<
						" to device pin " << (int)conn.device_pin << endl;
			}
			cout << "\tReading (synchronous): " << pin_channels.size() << endl;
			for(auto& conn : pin_channels){
				cout << "\t\t" << conn.first << ": global pin " << (int)conn.second.global_pin << endl;
			}
			cout << "\tWriting: " << writing_connections.size() << endl;
			for(auto& conn : writing_connections){
				cout << "\t\t" << conn.dev->getID() << " (" << conn.name << "): device pin " << (int)conn.device_pin <<
						" to global pin " << (int)conn.global_pin << endl;
			}
		}
	}

	if(debug_logging)
		factory.printAvailableDevices();

	return true;
}

/* QT */

void Breadboard::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	// background
	QStyleOption opt;
	opt.init(this);

	style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);

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
//			default:
//				for(unsigned i = 0; i < max_num_buttons; i++)
//				{
//					if(buttons[i] == nullptr)
//						break;	//this is sorted somewhat
//
//					if (buttons[i]->keybinding == e->key()) {
//						emit(setBit(translatePinToGpioOffs(buttons[i]->pin), gpio::Tristate::LOW));  // Active low
//						buttons[i]->pressed = true;
//					}
//				}
//				break;
		}
	}
}

void Breadboard::keyReleaseEvent(QKeyEvent* e)
{
//	for(unsigned i = 0; i < max_num_buttons; i++)
//	{
//		if(buttons[i] == nullptr)
//			break;	//this is sorted somewhat
//
//		if (buttons[i]->keybinding == e->key()) {
//			emit(setBit(translatePinToGpioOffs(buttons[i]->pin), gpio::Tristate::UNSET)); // simple switch, disconnected
//			buttons[i]->pressed = false;
//		}
//	}
}

void Breadboard::mousePressEvent(QMouseEvent* e) {
	for(pair<DeviceID,DeviceGraphic> graph_it : device_graphics) {
		// test if device graphic contains e->pos()
		// take into account graph_it.second->image.rect()
		// as well as scale and offs (as they are not actually applied to image)
		// if yes, get device from devices
		// then check for input-Interface
		// if existing, call pressed with true and set bit according to return
	}
}

void Breadboard::mouseReleaseEvent(QMouseEvent* e) {
	// basically the same as mousePressEvent
	// maybe one helper method for everything?
}

