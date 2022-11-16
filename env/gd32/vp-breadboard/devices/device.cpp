#include "device.hpp"

#include <QJsonArray>
#include <QKeySequence>

Device::Device(const DeviceID id) : m_id(id) {}

Device::~Device() {}

const DeviceID& Device::getID() const {
	return m_id;
}

void Device::fromJSON(QJsonObject json) {
	if (json.contains("conf") && json["conf"].isObject()) {
		if (!conf) {
			std::cerr << "[Factory] config for device '" << getClass()
			          << "' sets"
			             " a Config Interface, but device does no implement it"
			          << std::endl;
		} else {
			QJsonObject conf_obj = json["conf"].toObject();
			auto config = new Config();
			for (QJsonObject::iterator conf_it = conf_obj.begin(); conf_it != conf_obj.end(); conf_it++) {
				if (conf_it.value().isBool()) {
					config->emplace(conf_it.key().toStdString(), ConfigElem{conf_it.value().toBool()});
				} else if (conf_it.value().isDouble()) {
					config->emplace(conf_it.key().toStdString(), ConfigElem{(int64_t)conf_it.value().toInt()});
				} else if (conf_it.value().isString()) {
					QByteArray value_bytes = conf_it.value().toString().toLocal8Bit();
					config->emplace(conf_it.key().toStdString(), ConfigElem{value_bytes.data()});
				} else {
					std::cerr << "Invalid conf element type" << std::endl;
				}
			}
			conf->setConfig(config);
		}
	}

	if (json.contains("keybindings") && json["keybindings"].isArray()) {
		if (!input) {
			std::cerr << "[Factory] config for device '" << getClass()
			          << "' sets"
			             " keybindings, but device does not implement input interface"
			          << std::endl;
		} else {
			QJsonArray bindings = json["keybindings"].toArray();
			Keys keys;
			for (const QJsonValue& binding : bindings) {
				QKeySequence binding_sequence = QKeySequence(binding.toString());
				if (binding_sequence.count()) {
					keys.emplace(binding_sequence[0]);
				}
			}
			input->setKeys(keys);
		}
	}
}

QJsonObject Device::toJSON() {
	QJsonObject json;
	json["class"] = QString::fromStdString(getClass());
	json["id"] = QString::fromStdString(getID());
	if (conf) {
		QJsonObject conf_json;
		for (auto const& [desc, elem] : *conf->getConfig()) {
			if (elem.type == ConfigElem::Type::integer) {
				conf_json[QString::fromStdString(desc)] = (int)elem.value.integer;
			} else if (elem.type == ConfigElem::Type::boolean) {
				conf_json[QString::fromStdString(desc)] = elem.value.boolean;
			} else if (elem.type == ConfigElem::Type::string) {
				conf_json[QString::fromStdString(desc)] = QString::fromLocal8Bit(elem.value.string);
			}
		}
		json["conf"] = conf_json;
	}
	if (input) {
		Keys keys = input->getKeys();
		if (keys.size()) {
			QJsonArray keybindings_json;
			for (const Key& key : keys) {
				keybindings_json.append(QJsonValue(QKeySequence(key).toString()));
			}
			json["keybindings"] = keybindings_json;
		}
	}
	return json;
}

Device::PIN_Interface::~PIN_Interface() {}
Device::SPI_Interface::~SPI_Interface() {}
Device::EXMC_Interface::~EXMC_Interface() {}
Device::Config_Interface::~Config_Interface() {}
Device::Graphbuf_Interface::~Graphbuf_Interface() {}
Device::Input_Interface::~Input_Interface() {}
Device::TFT_Input_Interface::~TFT_Input_Interface() {}

void Device::Graphbuf_Interface::setBuffer(QImage& image, Layout layout, const Xoffset x, const Yoffset y, Pixel p) {
	auto* img = image.bits();
	if (x >= layout.width || y >= layout.height) {
		std::cerr << "[Graphbuf] WARN: device write accessing graphbuffer out of bounds!" << std::endl;
		return;
	}
	const auto offs = (y * layout.width + x) * 4;  // heavily depends on rgba8888
	img[offs + 0] = p.r;
	img[offs + 1] = p.g;
	img[offs + 2] = p.b;
	img[offs + 3] = p.a;
}

Pixel Device::Graphbuf_Interface::getBuffer(QImage& image, Layout layout, const Xoffset x, const Yoffset y) {
	auto* img = image.bits();
	if (x >= layout.width || y >= layout.height) {
		std::cerr << "[Graphbuf] WARN: device read accessing graphbuffer out of bounds!" << std::endl;
		return Pixel{0, 0, 0, 0};
	}
	const auto& offs = (y * layout.width + x) * 4;  // heavily depends on rgba8888
	return Pixel{static_cast<uint8_t>(img[offs + 0]), static_cast<uint8_t>(img[offs + 1]),
	             static_cast<uint8_t>(img[offs + 2]), static_cast<uint8_t>(img[offs + 3])};
}

void Device::Input_Interface::setKeys(Keys bindings) {
	keybindings = bindings;
}

Keys Device::Input_Interface::getKeys() {
	return keybindings;
}
