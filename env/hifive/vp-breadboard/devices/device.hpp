#pragma once

#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#include <gpio/gpio-client.hpp>

#include "configurations.h"

class Device {
protected:
	DeviceID m_id;

public:

	const DeviceID& getID() const;
	virtual const DeviceClass getClass() const = 0;

	class PIN_Interface {
	public:
		virtual ~PIN_Interface();
		virtual PinLayout getPinLayout() = 0;
		virtual bool getPin(PinNumber num) = 0;
		virtual void setPin(PinNumber num, bool val) = 0;	// TODO Tristate?
	};

	class SPI_Interface {
	public:
		virtual ~SPI_Interface();
		virtual uint8_t send(uint8_t byte) = 0;
	};

	class Config_Interface {
	public:
		virtual ~Config_Interface();
		virtual Config getConfig() = 0;
		virtual bool setConfig(const Config conf) = 0;
	};

	class Graphbuf_Interface {
	public:
		virtual ~Graphbuf_Interface();
		virtual Layout getLayout() = 0;
		virtual void initializeBufferMaybe() = 0;
		virtual void registerSetBuf(const SetBuf_fn setBuf) = 0;
		virtual void registerGetBuf(const GetBuf_fn getBuf) = 0;
	};

	class Input_Interface {
	public:
		virtual ~Input_Interface();
		virtual gpio::Tristate pressed(bool active) = 0;
	};


	std::unique_ptr<PIN_Interface> pin;
	std::unique_ptr<SPI_Interface> spi;
	std::unique_ptr<Config_Interface> conf;
	std::unique_ptr<Graphbuf_Interface> graph;
	std::unique_ptr<Input_Interface> input;

	Device(DeviceID id);
	virtual ~Device();
};
