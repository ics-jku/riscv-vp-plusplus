#include "device.hpp"
#include "breadboard/configurations.h"

#include <QKeySequence>
#include <QJsonArray>

Device::Device(const DeviceID id) : m_id(id) {}

Device::~Device() {}

const DeviceID& Device::getID() const {
	return m_id;
}

void Device::fromJSON(QJsonObject json) {
	if(json.contains("conf") && json["conf"].isObject()) {
		if(!conf) {
			std::cerr << "[Device] config for device '" << getClass() << "' sets"
					" a Config Interface, but device does not implement it" << std::endl;
		}
		else {
			QJsonObject conf_obj = json["conf"].toObject();
			auto config = new Config();
			for(QJsonObject::iterator conf_it = conf_obj.begin(); conf_it != conf_obj.end(); conf_it++) {
				if(conf_it.value().isBool()) {
					config->emplace(conf_it.key().toStdString(), ConfigElem{conf_it.value().toBool()});
				}
				else if(conf_it.value().isDouble()) {
					config->emplace(conf_it.key().toStdString(), ConfigElem{(int64_t) conf_it.value().toInt()});
				}
				else if(conf_it.value().isString()) {
					QByteArray value_bytes = conf_it.value().toString().toLocal8Bit();
					config->emplace(conf_it.key().toStdString(), ConfigElem{value_bytes.data()});
				}
				else {
					std::cerr << "[Device] Invalid conf element type" << std::endl;
				}
			}
			conf->setConfig(config);
		}
	}

	if(json.contains("keybindings") && json["keybindings"].isArray()) {
		if(!input) {
			std::cerr << "[Device] config for device '" << getClass() << "' sets"
					" keybindings, but device does not implement input interface" << std::endl;
		}
		else {
			QJsonArray bindings = json["keybindings"].toArray();
			Keys keys;
			for(const QJsonValue& binding : bindings) {
				QKeySequence binding_sequence = QKeySequence(binding.toString());
				if(binding_sequence.count()) {
					keys.emplace(binding_sequence[0]);
				}
			}
			input->setKeys(keys);
		}
	}

	if(json.contains("graphics") && json["graphics"].isObject()) {
		if(!graph) {
			std::cerr << "[Device] Config for device '" << getClass() << "' contains graph info, "
					"but device does not implement graph interface" << std::endl;
		}
		else {
			QJsonObject graphics = json["graphics"].toObject();
			const QJsonArray offs_desc = graphics["offs"].toArray();
			QPoint offs(offs_desc[0].toInt(), offs_desc[1].toInt());
			graph->createBuffer(offs);
			unsigned scale = graphics["scale"].toInt();
			graph->setScale(scale);
		}
	}
}

QJsonObject Device::toJSON() {
	QJsonObject json;
	json["class"] = QString::fromStdString(getClass());
	json["id"] = QString::fromStdString(getID());
	if(conf) {
		QJsonObject conf_json;
		for(auto const& [desc, elem] : *conf->getConfig()) {
			if(elem.type == ConfigElem::Type::integer) {
				conf_json[QString::fromStdString(desc)] = (int) elem.value.integer;
			}
			else if(elem.type == ConfigElem::Type::boolean) {
				conf_json[QString::fromStdString(desc)] = elem.value.boolean;
			}
			else if(elem.type == ConfigElem::Type::string) {
				conf_json[QString::fromStdString(desc)] = QString::fromLocal8Bit(elem.value.string);
			}
		}
		json["conf"] = conf_json;
	}
	if(input) {
		Keys keys = input->getKeys();
		if(keys.size()) {
			QJsonArray keybindings_json;
			for(const Key& key : keys) {
				keybindings_json.append(QJsonValue(QKeySequence(key).toString()));
			}
			json["keybindings"] = keybindings_json;
		}
	}
	if(graph) {
		QJsonObject graph_json;
		QJsonArray offs_json;
		offs_json.append(graph->getBuffer().offset().x());
		offs_json.append(graph->getBuffer().offset().y());
		graph_json["offs"] = offs_json;
		graph_json["scale"] = (int) graph->getScale();
		json["graphics"] = graph_json;
	}
	return json;
}

Device::PIN_Interface::~PIN_Interface() {}
Device::SPI_Interface::~SPI_Interface() {}
Device::Config_Interface::~Config_Interface() {}
Device::Graphbuf_Interface::~Graphbuf_Interface() {}
Device::Input_Interface::~Input_Interface() {}

void Device::Graphbuf_Interface::createBuffer(QPoint offset) {
	Layout layout = getLayout();
	buffer = QImage(layout.width*BB_ICON_SIZE, layout.height*BB_ICON_SIZE, QImage::Format_RGBA8888);
	memset(buffer.bits(), 0x8F, buffer.sizeInBytes());
	buffer.setOffset(offset);
	initializeBuffer();
}

void Device::Graphbuf_Interface::setScale(unsigned scale) {
	this->scale = scale;
}

unsigned Device::Graphbuf_Interface::getScale() { return scale; }

QImage& Device::Graphbuf_Interface::getBuffer() {
	return buffer;
}

void Device::Graphbuf_Interface::setPixel(const Xoffset x, const Yoffset y, Pixel p) {
	auto* img = getBuffer().bits();
	if(x >= buffer.width() || y >= buffer.height()) {
		std::cerr << "[Device] WARN: device write accessing graphbuffer out of bounds!" << std::endl;
		return;
	}
	const auto offs = (y * buffer.width() + x) * 4; // heavily depends on rgba8888
	img[offs+0] = p.r;
	img[offs+1] = p.g;
	img[offs+2] = p.b;
	img[offs+3] = p.a;
}

Pixel Device::Graphbuf_Interface::getPixel(const Xoffset x, const Yoffset y) {
	auto* img = getBuffer().bits();
	if(x >= buffer.width() || y >= buffer.height()) {
		std::cerr << "[Device] WARN: device read accessing graphbuffer out of bounds!" << std::endl;
		return Pixel{0,0,0,0};
	}
	const auto& offs = (y * buffer.width() + x) * 4; // heavily depends on rgba8888
	return Pixel{
		static_cast<uint8_t>(img[offs+0]),
				static_cast<uint8_t>(img[offs+1]),
				static_cast<uint8_t>(img[offs+2]),
				static_cast<uint8_t>(img[offs+3])
	};
}

void Device::Input_Interface::setKeys(Keys bindings) {
	keybindings = bindings;
}

Keys Device::Input_Interface::getKeys() {
	return keybindings;
}
