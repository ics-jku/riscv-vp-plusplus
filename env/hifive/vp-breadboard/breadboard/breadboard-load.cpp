#include "breadboard.h"

using namespace std;

constexpr bool debug_logging = false;

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
	QJsonParseError error;
	QJsonDocument json_doc = QJsonDocument::fromJson(raw_file, &error);
	if(json_doc.isNull())
	{
		cerr << "Config seems to be invalid: ";
		cerr << error.errorString().toStdString() << endl;
		return false;
	}
	QJsonObject config_root = json_doc.object();

	QString bkgnd_path = ":/img/virtual_breadboard.png";
	QSize bkgnd_size = QSize(486, 233);
	if(config_root.contains("window") && config_root["window"].isObject()) {
		breadboard = false;
		QJsonObject window = config_root["window"].toObject();
		bkgnd_path = window["background"].toString(bkgnd_path);
		unsigned windowsize_x = window["windowsize"].toArray().at(0).toInt();
		unsigned windowsize_y = window["windowsize"].toArray().at(1).toInt();
		bkgnd_size = QSize(windowsize_x, windowsize_y);
	}

	QPixmap bkgnd(bkgnd_path);
	bkgnd = bkgnd.scaled(bkgnd_size, Qt::IgnoreAspectRatio);
	QPalette palette;
	palette.setBrush(QPalette::Window, bkgnd);
	this->setPalette(palette);
	this->setAutoFillBackground(true);

	setFixedSize(bkgnd_size);

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

			if(device_desc.contains("conf") && device_desc["conf"].isObject()) {
				if(!device->conf) {
					cerr << "[config loader] config for device '" << classname << "' sets"
							" a Config Interface, but device does no implement it" << endl;
					continue;
				}

				QJsonObject conf_obj = device_desc["conf"].toObject();
				Config conf = Config();
				for(QJsonObject::iterator conf_it = conf_obj.begin(); conf_it != conf_obj.end(); conf_it++) {
					if(conf_it.value().isBool()) {
						conf.emplace(conf_it.key().toStdString(), ConfigElem{conf_it.value().toBool()});
					}
					else if(conf_it.value().isDouble()) {
						conf.emplace(conf_it.key().toStdString(), ConfigElem{(int64_t) conf_it.value().toInt()});
					}
					else {
						cerr << "Invalid conf element type" << endl;
					}
				}
				device->conf->setConfig(conf);
			}

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
								device->pin->setPin(device_pin, pin);
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

				device->graph->registerBuffer(new_buffer);
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
