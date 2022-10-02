#pragma once

#include "devices/device.hpp"

class CDevice : public Device {
public:
	class PIN_Interface_C : public Device::PIN_Interface {
	protected:
		CDevice* device;
		PinLayout layout;
	public:
		PIN_Interface_C(CDevice* device);
		~PIN_Interface_C();
		PinLayout getPinLayout();
		gpio::Tristate getPin(PinNumber num); // implement this
		void setPin(PinNumber num, gpio::Tristate val);	// implement this
	};

	class SPI_Interface_C : public Device::SPI_Interface {
	protected:
		CDevice* device;
	public:
		SPI_Interface_C(CDevice* device);
		~SPI_Interface_C();
		gpio::SPI_Response send(gpio::SPI_Command byte); // implement this
	};

	class Config_Interface_C : public Device::Config_Interface {
	protected:
		CDevice* device;
		Config* config;
	public:
		Config_Interface_C(CDevice *device);
		~Config_Interface_C();
		Config* getConfig();
		bool setConfig(Config* conf);
	};

	class Graphbuf_Interface_C : public Device::Graphbuf_Interface {
	protected:
		CDevice* device;
		Layout layout;
	public:
		Graphbuf_Interface_C(CDevice* device);
		~Graphbuf_Interface_C();
		Layout getLayout();
		void initializeBuffer(); // implement this
	};

	class Input_Interface_C : public Device::Input_Interface {
	protected:
		CDevice* device;
	public:
		Input_Interface_C(CDevice* device);
		~Input_Interface_C();
		void onClick(bool active);
		void onKeypress(Key key, bool active);
	};

	CDevice(DeviceID id);
	~CDevice();
};
