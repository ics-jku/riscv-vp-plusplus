#include "mainwindow.h"
#include <qpainter.h>
#include <QKeyEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cassert>
#include <iostream>
#include "ui_mainwindow.h"

#include <unistd.h>  //sleep

using namespace std;
using namespace gpio;

constexpr bool debug_logging = true;

VPBreadboard::VPBreadboard(std::string configfile,
		const char* host, const char* port,
		std::string additional_device_dir, bool overwrite_integrated_devices,
		QWidget* mparent)
    : QWidget(mparent),
      host(host),
      port(port),
      sevensegment(nullptr),
      rgbLed(nullptr),
      oled_mmap(nullptr),
      oled_iof(nullptr)
	{

	memset(buttons, 0, max_num_buttons * sizeof(Button*));

	if(additional_device_dir.size() != 0){
		lua_factory.scanAdditionalDir(additional_device_dir, overwrite_integrated_devices);
	}

	if(!loadConfigFile(configfile)) {
		cerr << "Could not load config file '" << configfile << "'" << endl;
		exit(-4);
	}

	if(debug_logging)
		lua_factory.printAvailableDevices();
}

bool VPBreadboard::loadConfigFile(std::string file) {
	// load configuration

	QFile confFile(file.c_str());
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

	QPixmap bkgnd(config_root["background"].toString(""));
	if(bkgnd.isNull())
	{
		cerr << "invalid background " << config_root["background"].toString().toStdString() << endl;
		cerr << "Available backgrounds:" << endl;
		QDirIterator it(":/img");
		while (it.hasNext()) {
			std::cout << "\t\t " << it.next().toStdString() << std::endl;
		}
		return false;
	}

	unsigned windowsize_x = bkgnd.width();
	unsigned windowsize_y = bkgnd.height();

	if(config_root.contains("windowsize")) {
		windowsize_x = config_root["windowsize"].toArray().at(0).toInt();
		windowsize_y = config_root["windowsize"].toArray().at(1).toInt();
	}

	QSize size(windowsize_x, windowsize_y);
	bkgnd = bkgnd.scaled(size, Qt::IgnoreAspectRatio);
	QPalette palette;
	palette.setBrush(QPalette::Window, bkgnd);
	this->setPalette(palette);
	setFixedSize(size);

	// TODO: Let every registered "config object" decide its own default values

	if(config_root.contains("sevensegment"))
	{
		QJsonObject obj = config_root["sevensegment"].toObject();
		sevensegment = new Sevensegment(
			QPoint(obj["offs"].toArray().at(0).toInt(312),
			       obj["offs"].toArray().at(1).toInt(353)),
			QPoint(obj["extent"].toArray().at(0).toInt(36),
			       obj["extent"].toArray().at(1).toInt(50)),
			obj["linewidth"].toInt(7)
			);
	}
	if(config_root.contains("rgb"))
	{
		QJsonObject obj = config_root["rgb"].toObject();
		rgbLed = new RGBLed(
			QPoint(obj["offs"].toArray().at(0).toInt(89),
			       obj["offs"].toArray().at(1).toInt(161)),
			obj["linewidth"].toInt(15)
			);
	}
	if(config_root.contains("oled-mmap"))
	{
		QJsonObject obj = config_root["oled-mmap"].toObject();
		oled_mmap = new OLED_mmap(
			QPoint(obj["offs"].toArray().at(0).toInt(450),
			       obj["offs"].toArray().at(1).toInt(343)),
			obj["margin"].toInt(15),
			obj["scale"].toDouble(1.));
	}
	if(config_root.contains("oled-iof"))
	{
		QJsonObject obj = config_root["oled-iof"].toObject();
		const gpio::PinNumber cs = obj["cs_pin"].toInt(9);
		const gpio::PinNumber dc = obj["dc_pin"].toInt(16);
		const bool noresponse = obj["noresponse"].toBool(true);
		oled_iof = new OLED_iof(
			QPoint(obj["offs"].toArray().at(0).toInt(450),
			       obj["offs"].toArray().at(1).toInt(343)),
			obj["margin"].toInt(15),
			obj["scale"].toDouble(1.)
			);
		spi_channels.emplace("oled-iof", SPI_IOF_Request{
				.gpio_offs = translatePinToGpioOffs(cs),
				.global_pin = cs,
				.noresponse = noresponse,
				.fun = [this](gpio::SPI_Command cmd){ return oled_iof->write(cmd);}
			 }
		);
		pin_channels.emplace("oled-iof", PIN_IOF_Request{
			.gpio_offs = translatePinToGpioOffs(dc),
			.global_pin = dc,
			.fun = [this](gpio::Tristate pin) { oled_iof->data_command_pin = pin == Tristate::HIGH ? 1 : 0;}
		});
	}
	if(config_root.contains("buttons"))
	{
		const auto butts = config_root["buttons"].toArray();
		for(unsigned i = 0; i < butts.size() && i < max_num_buttons; i++)
		{
			QJsonObject butt = butts[i].toObject();
			buttons[i] = new Button{
				QRect{
					QPoint{butt["pos"].toArray().at(0).toInt(), butt["pos"].toArray().at(1).toInt()},
					QSize{butt["dim"].toArray().at(0).toInt(), butt["dim"].toArray().at(1).toInt()}
				},
				static_cast<uint8_t>(butt["pin"].toInt()),
				butt["key"].toString(QString("")),
				butt["name"].toString(QString("undef"))
			};
		}
	}
	if(config_root.contains("devices") && config_root["devices"].isArray()) {
		auto device_descriptions = config_root["devices"].toArray();
		devices.reserve(device_descriptions.count());
		if(debug_logging)
			cout << "[config loader] reserving space for " << device_descriptions.count() << " devices." << endl;
		for(const auto& device_description : device_descriptions) {
			const auto device_desc = device_description.toObject();
			const auto& classname = device_desc["class"].toString("undefined").toStdString();
			const auto& id = device_desc["id"].toString("undefined").toStdString();

			if(!lua_factory.deviceExists(classname)) {
				cerr << "[config loader] device '" << classname << "' does not exist" << endl;

				continue;
			}
			if(devices.find(id) != devices.end()) {
				cerr << "[config loader] Another device with the ID '" << id << "' is already instatiated!" << endl;
				continue;
			}
			devices.emplace(id, lua_factory.instantiateDevice(id, classname));
			Device& device = devices.at(id);

			if(device_desc.contains("spi") && device_desc["spi"].isObject()) {
				if(!device.spi) {
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
						.fun = [this, &device](gpio::SPI_Command cmd){
							lua_access.lock();
							const auto ret = device.spi->send(cmd);
							lua_access.unlock();
							return ret;
						}
					}
				);
			}

			if(device_desc.contains("pins") && device_desc["pins"].isArray()) {
				if(!device.pin) {
					cerr << "[config loader] config for device '" << classname << "' sets"
							" an PIN interface, but device does not implement it" << endl;
					continue;
				}
				const auto pinLayout = device.pin->getPinLayout();
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
						if(pin_l.dir != Device::PIN_Interface::PinDesc::Dir::input) {
							cerr << "[config loader] config for device '" << classname << "' maps pin " <<
									(int)device_pin << " as syncronous, but device labels pin not as input."
									" This is not supported for inout-pins and unnecessary for output pins." << endl;
							continue;
						}
						pin_channels.emplace(id, PIN_IOF_Request{
							.gpio_offs = translatePinToGpioOffs(global_pin),
							.global_pin = global_pin,
							.fun = [this, &device, device_pin](gpio::Tristate pin) {
								lua_access.lock();
								device.pin->setPin(device_pin, pin == Tristate::HIGH ? 1 : 0);
								lua_access.unlock();
							}
						});
					} else {
						PinMapping mapping = PinMapping{
							.gpio_offs = translatePinToGpioOffs(global_pin),
							.global_pin = global_pin,
							.device_pin = device_pin,
							.name = pin_name,
							.dev = &device
						};
						if(pin_l.dir == Device::PIN_Interface::PinDesc::Dir::input
								|| pin_l.dir == Device::PIN_Interface::PinDesc::Dir::inout) {
							reading_connections.push_back(mapping);
						}
						if(pin_l.dir == Device::PIN_Interface::PinDesc::Dir::output
								|| pin_l.dir == Device::PIN_Interface::PinDesc::Dir::inout) {
							writing_connections.push_back(mapping);
						}
					}
				}
			}

			if(device_desc.contains("graphics") && device_desc["graphics"].isObject()) {
				if(!device.graph) {
					cerr << "[config loader] config for device '" << classname << "' sets"
							" a graph buffer interface, but device does not implement it" << endl;
					continue;
				}
				const auto layout = device.graph->getLayout();
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

				typedef Device::Graphbuf_Interface::Xoffset Xoffset;
				typedef Device::Graphbuf_Interface::Yoffset Yoffset;
				typedef Device::Graphbuf_Interface::Pixel Pixel;
				device_graphics.emplace(id, DeviceGraphic{
					.image = QImage(layout.width, layout.height, QImage::Format_RGBA8888),
					.offset = offs,
					.scale = scale
				});

				// setting up the image buffer and its functions
				auto& new_buffer = device_graphics.at(id).image;
				memset(new_buffer.bits(), 0x8F, new_buffer.sizeInBytes());

				device.graph->registerSetBuf([&new_buffer, layout, id](const Xoffset x, const Yoffset y, Pixel p){
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
				device.graph->registerGetBuf([&new_buffer, layout, id](const Xoffset x, const Yoffset y){
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
			}
		}

		if(debug_logging) {
			cout << "Instatiated devices:" << endl;
			for (auto& [id, device] : devices) {
				cout << "\t" << id << " of class " << device.getClass() << endl;

				if(device.pin)
					cout << "\t\timplements PIN" << endl;
				if(device.spi)
					cout << "\t\timplements SPI" << endl;
				if(device.conf)
					cout << "\t\timplements conf" << endl;
				if(device.graph)
					cout << "\t\timplements graphbuf" << endl;
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
	return true;
}

VPBreadboard::~VPBreadboard()
{
	if(sevensegment != nullptr)
		delete sevensegment;
	if(rgbLed != nullptr)
		delete rgbLed;
	if(oled_mmap != nullptr)
		delete oled_mmap;
	for(unsigned i = 0; i < max_num_buttons; i++)
	{
		if(buttons[i] != nullptr)
		delete buttons[i];
	}
}

void VPBreadboard::showConnectionErrorOverlay(QPainter& p) {
	p.save();
	p.setBrush(QBrush(QColor("black")));
	if(debugmode)
		p.setBrush(QBrush(QColor(0,0,0,100)));

	QRect sign;
	if(this->size().width() > this->size().height()) {
		sign = QRect (QPoint(this->size().width()/4, this->size().height()/4), this->size()/2);
	}
	else {
		sign = QRect (QPoint(this->size().width()/10, this->size().height()/4),
				QSize(4*this->size().width()/5, this->size().height()/4));
	}
	p.drawRect(sign);
	p.setFont(QFont("Arial", 25, QFont::Bold));
	QPen penHText(QColor("red"));
	if (debugmode)
		penHText.color().setAlphaF(.5);
	p.setPen(penHText);
	p.drawText(sign, QString("No connection"), Qt::AlignHCenter | Qt::AlignVCenter);
	p.restore();
}

uint64_t VPBreadboard::translateGpioToExtPin(gpio::State state) {
	uint64_t ext = 0;
	for (PinNumber i = 0; i < 24; i++)  // Max Pin is 32,  but used are only first 24
	                                   // see SiFive HiFive1 Getting Started Guide 1.0.2 p. 20
	{
		// cout << i << " to ";;
		if (i >= 16) {
			ext |= (state.pins[i] == Pinstate::HIGH ? 1 : 0) << (i - 16);
			// cout << i - 16 << endl;
		} else if (i <= 5) {
			ext |= (state.pins[i] == Pinstate::HIGH ? 1 : 0) << (i + 8);
			// cout << i + 8 << endl;
		} else if (i >= 9 && i <= 13) {
			ext |= (state.pins[i] == Pinstate::HIGH ? 1 : 0) << (i + 6);;
		}
		// rest is not connected.
	}
	return ext;
}

uint8_t VPBreadboard::translatePinNumberToSevensegment(uint64_t pinmap) {
	uint8_t ret = 0;
	static uint8_t pinMapping[8] =
	{
	  15, 16, 17, 18, 19, 7, 6, 5
	};
	for(unsigned i = 0; i < 8; i++)
	{
		ret |= pinmap & (1 << pinMapping[i]) ? (1 << i) : 0;
	}
	return ret;
}

uint8_t VPBreadboard::translatePinNumberToRGBLed(uint64_t pinmap) {
	uint8_t ret = 0;
	ret |= (~pinmap & (1 << 6)) >> 6;  // R
	ret |= (~pinmap & (1 << 3)) >> 2;  // G
	ret |= (~pinmap & (1 << 5)) >> 3;  // B
	return ret;
}

uint8_t VPBreadboard::translatePinToGpioOffs(uint8_t pin) {
	if (pin < 8) {
		return pin + 16;  // PIN_0_OFFSET
	}
	if(pin >= 8 && pin < 14) {
		return pin - 8;
	}
	//ignoring non-wired pin 14 <==> 8
	if(pin > 14 && pin < 20){
		return pin - 6;
	}

	return 0;
}

void printBin(char* buf, uint8_t len) {
	for (uint16_t byte = 0; byte < len; byte++) {
		for (int8_t bit = 7; bit >= 0; bit--) {
			printf("%c", buf[byte] & (1 << bit) ? '1' : '0');
		}
		printf(" ");
	}
	printf("\n");
}

void VPBreadboard::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	if (connected && !gpio.update()) {
		// Just lost connection
		connected = false;
	}

	if (!connected) {
		connected = gpio.setupConnection(host, port);
		showConnectionErrorOverlay(painter);
		if (!connected) {
			if(!debugmode) {
				usleep(500000);	// Ugly, sorry
				this->update();
				return;
			}
		} else {
			// just got connection.
			// TODO: New-connection callback for all devices

			for(const auto& [id, req] : spi_channels) {
				if(!gpio.isIOFactive(req.gpio_offs)) {
					const bool success =
							gpio.registerSPIOnChange(req.gpio_offs, req.fun, req.noresponse);
					if(!success) {
						cerr << "Registering spi channel for " << id << " "
								"on CS pin " << (int)req.global_pin << " (" << (int)req.gpio_offs << ")";
						if(req.noresponse)
								cerr << " in noresponse mode";
						cerr << " was not successful!" << endl;
					}
				}
			}
			for(const auto& [id, req] : pin_channels) {
				if(!gpio.isIOFactive(req.gpio_offs)) {
					const bool success =
							gpio.registerPINOnChange(req.gpio_offs, req.fun);
					if(!success) {
						cerr << "Registering pin channel for " << id << " "
								"on pin " << (int)req.global_pin << " (" << (int)req.gpio_offs << ")";
						cerr << " was not successful!" << endl;
					}
				}
			}
		}
	}

	lua_access.lock();
	for (auto& c : reading_connections) {
		// TODO: Only if pin changed?
		c.dev->pin->setPin(c.device_pin, gpio.state.pins[c.gpio_offs] == Pinstate::HIGH ? true : false);
	}
	lua_access.unlock();

	for (auto& [id, graphic] : device_graphics) {
		const auto& image = graphic.image;
		painter.drawImage(graphic.offset,
				image.scaled(image.size().width()*graphic.scale,
				image.size().height()*graphic.scale));
	}

	// TODO: Check loaded, drawable plugins instead of hardcoded objects

	if(oled_mmap)
		oled_mmap->draw(painter);

	if(oled_iof)
		oled_iof->draw(painter);

	if(sevensegment)
	{
		sevensegment->map = translatePinNumberToSevensegment(translateGpioToExtPin(gpio.state));
		sevensegment->draw(painter);
	}

	if(rgbLed)
	{
		rgbLed->map = translatePinNumberToRGBLed(translateGpioToExtPin(gpio.state));
		rgbLed->draw(painter);
	}

	//buttons
	painter.save();
	QColor dark("#101010");
	dark.setAlphaF(0.5);
	painter.setBrush(QBrush(dark));
	if(debugmode)
	{
		painter.setPen(QPen(QColor("red")));
		painter.setFont(QFont("Arial", 12));
	}
	for(unsigned i = 0; i < max_num_buttons; i++)
	{
		if(!buttons[i])
			break;
		if(buttons[i]->pressed || debugmode)
			painter.drawRect(buttons[i]->area);
		if(debugmode)
			painter.drawText(buttons[i]->area, buttons[i]->name, Qt::AlignHCenter | Qt::AlignVCenter);
	}
	painter.restore();


	if (debugmode) {
		if(sevensegment)
			painter.drawRect(QRect(sevensegment->offs, QSize(sevensegment->extent.x(), sevensegment->extent.y())));
	}
	painter.end();

	lua_access.lock();
	for (auto& c : writing_connections) {
		gpio.setBit(c.gpio_offs, c.dev->pin->getPin(c.device_pin) ? Tristate::HIGH : Tristate::LOW);
	}
	lua_access.unlock();

	// intentional slow down
	// TODO: update at fixed rate, async between redraw and gpioserver
	usleep(10000);
	this->update();
}

void VPBreadboard::notifyChange(bool success) {
	assert(success);
	update();
}

void VPBreadboard::keyPressEvent(QKeyEvent* e) {
	this->update();
	// scout << "Yee, keypress" << endl;

	if(debugmode)
	{
		switch(e->key())
		{
		case Qt::Key_Right:
			if(buttons[++moving_button] == nullptr || moving_button >= max_num_buttons)
				moving_button = 0;
			if(buttons[moving_button] == nullptr)
			{
				cout << "No Buttons available" << endl;
			}
			else
			{
				cout << "Moving button " << buttons[moving_button]->name.toStdString() << endl;
			}
			break;

		case Qt::Key_W:
			if(buttons[moving_button] == nullptr)
				break;
			buttons[moving_button]->area.moveTopLeft(buttons[moving_button]->area.topLeft() - QPoint(0, 1));
			cout << buttons[moving_button]->name.toStdString() << " ";
			cout << "X: " << buttons[moving_button]->area.topLeft().x() << " Y: " << buttons[moving_button]->area.topLeft().y() << endl;
			break;
		case Qt::Key_A:
			if(buttons[moving_button] == nullptr)
				break;
			buttons[moving_button]->area.moveTopLeft(buttons[moving_button]->area.topLeft() - QPoint(1, 0));
			cout << buttons[moving_button]->name.toStdString() << " ";
			cout << "X: " << buttons[moving_button]->area.topLeft().x() << " Y: " << buttons[moving_button]->area.topLeft().y() << endl;
			break;
		case Qt::Key_S:
			if(buttons[moving_button] == nullptr)
				break;
			buttons[moving_button]->area.moveTopLeft(buttons[moving_button]->area.topLeft() + QPoint(0, 1));
			cout << buttons[moving_button]->name.toStdString() << " ";
			cout << "X: " << buttons[moving_button]->area.topLeft().x() << " Y: " << buttons[moving_button]->area.topLeft().y() << endl;
			break;
		case Qt::Key_D:
			if(buttons[moving_button] == nullptr)
				break;
			buttons[moving_button]->area.moveTopLeft(buttons[moving_button]->area.topLeft() + QPoint(1, 0));
			cout << buttons[moving_button]->name.toStdString() << " ";
			cout << "X: " << buttons[moving_button]->area.topLeft().x() << " Y: " << buttons[moving_button]->area.topLeft().y() << endl;
			break;
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
				gpio.destroyConnection();
				QApplication::quit();
				break;
			case Qt::Key_0: {
				uint8_t until = 6;
				for (uint8_t i = 0; i < 8; i++) {
					gpio.setBit(i, i < until ? Tristate::HIGH : Tristate::LOW);
				}
				break;
			}
			case Qt::Key_1: {
				for (uint8_t i = 0; i < 8; i++) {
					gpio.setBit(i, Tristate::LOW);
				}
				break;
			}
			case Qt::Key_Space:
				cout << "Set Debug mode" << endl;
				debugmode = true;
				break;
			default:
				for(unsigned i = 0; i < max_num_buttons; i++)
				{
					if(buttons[i] == nullptr)
						break;	//this is sorted somewhat

					if (buttons[i]->keybinding == e->key()) {
						gpio.setBit(translatePinToGpioOffs(buttons[i]->pin), Tristate::LOW);  // Active low
						buttons[i]->pressed = true;
					}
				}
				break;
		}
	}
}

void VPBreadboard::keyReleaseEvent(QKeyEvent* e)
{
	for(unsigned i = 0; i < max_num_buttons; i++)
	{
		if(buttons[i] == nullptr)
			break;	//this is sorted somewhat

		if (buttons[i]->keybinding == e->key()) {
			gpio.setBit(translatePinToGpioOffs(buttons[i]->pin), Tristate::UNSET); // simple switch, disconnected
			buttons[i]->pressed = false;
		}
	}
}

void VPBreadboard::mousePressEvent(QMouseEvent* e) {
	if (e->button() == Qt::LeftButton) {
		for(unsigned i = 0; i < max_num_buttons; i++)
		{
			if(buttons[i] == nullptr)
				break;	//this is sorted somewhat

			if (buttons[i]->area.contains(e->pos())) {
				//cout << "button " << i << " click!" << endl;
				gpio.setBit(translatePinToGpioOffs(buttons[i]->pin), Tristate::LOW);  // Active low
				buttons[i]->pressed = true;
			}
		}
		// cout << "clicked summin elz" << endl;
	} else {
		cout << "Whatcha doin' there?" << endl;
	}
	this->update();
	e->accept();
}

void VPBreadboard::mouseReleaseEvent(QMouseEvent* e) {
	if (e->button() == Qt::LeftButton) {
		for(unsigned i = 0; i < max_num_buttons; i++)
		{
			if(buttons[i] == nullptr)
				break;	//this is sorted somewhat
			if (buttons[i]->area.contains(e->pos())) {
				//cout << "button " << i << " release!" << endl;
				gpio.setBit(translatePinToGpioOffs(buttons[i]->pin), Tristate::UNSET);
				buttons[i]->pressed = false;
			}
		}
		// cout << "released summin elz" << endl;
	} else {
		cout << "Whatcha doin' there?" << endl;
	}
	this->update();
	e->accept();
}
