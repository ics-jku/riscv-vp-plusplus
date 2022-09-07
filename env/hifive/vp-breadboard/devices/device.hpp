#pragma once

#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#include <QImage>
#include <QJsonObject>

#include "configurations.h"
#include <gpio/gpio-client.hpp>

class Device {
protected:
	DeviceID m_id;

public:

	const DeviceID& getID() const;
	virtual const DeviceClass getClass() const = 0;

	void fromJSON(QJsonObject json);
	QJsonObject toJSON();

	class PIN_Interface {
	public:
		virtual ~PIN_Interface();
		virtual PinLayout getPinLayout() = 0;
		virtual gpio::Tristate getPin(PinNumber num) = 0;
		virtual void setPin(PinNumber num, gpio::Tristate val) = 0;
	};

	class SPI_Interface {
	public:
		virtual ~SPI_Interface();
		virtual gpio::SPI_Response send(gpio::SPI_Command byte) = 0;
	};

	class Config_Interface {
	public:
		virtual ~Config_Interface();
		virtual Config* getConfig() = 0;
		virtual bool setConfig(Config* conf) = 0;
	};

	class Graphbuf_Interface {
	public:
		virtual ~Graphbuf_Interface();
		virtual Layout getLayout() = 0;
		virtual void initializeBufferMaybe() = 0;
		virtual void registerBuffer(QImage& image) = 0;
		static void setBuffer(QImage&, Layout, const Xoffset, const Yoffset, Pixel);
		static Pixel getBuffer(QImage&, Layout, const Xoffset, const Yoffset);
	};

	class Input_Interface {
	public:
		Keys keybindings;
		virtual ~Input_Interface();
		virtual void onClick(bool active) = 0;
		virtual void onKeypress(Key key, bool active) = 0;
		void setKeys(Keys bindings);
		Keys getKeys();
	};


	std::unique_ptr<PIN_Interface> pin;
	std::unique_ptr<SPI_Interface> spi;
	std::unique_ptr<Config_Interface> conf;
	std::unique_ptr<Graphbuf_Interface> graph;
	std::unique_ptr<Input_Interface> input;

	Device(DeviceID id);
	virtual ~Device();
};
