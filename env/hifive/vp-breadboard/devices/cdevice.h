#pragma once

#include "device.hpp"

class CDevice : public Device {

public:
	SetBuf_fn set_buf;
	GetBuf_fn get_buf;
	Layout layout_graph;
	PinLayout layout_pin;
	Config config;

	class PIN_Interface_C : public Device::PIN_Interface {
	protected:
		CDevice* device;
	public:
		PIN_Interface_C(CDevice* device);
		~PIN_Interface_C();
		PinLayout getPinLayout();
		bool getPin(PinNumber num); // implement this
		void setPin(PinNumber num, bool val);	// implement this
	};

	class SPI_Interface_C : public Device::SPI_Interface {
	protected:
		CDevice* device;
	public:
		SPI_Interface_C(CDevice* device);
		~SPI_Interface_C();
		uint8_t send(uint8_t byte); // implement this
	};

	class Config_Interface_C : public Device::Config_Interface {
	protected:
		CDevice* device;
	public:
		Config_Interface_C(CDevice *device);
		~Config_Interface_C();
		Config getConfig();
		bool setConfig(const Config conf);
	};

	class Graphbuf_Interface_C : public Device::Graphbuf_Interface {
	protected:
		CDevice* device;
	public:
		Graphbuf_Interface_C(CDevice* device);
		~Graphbuf_Interface_C();
		Layout getLayout();
		void initializeBufferMaybe(); // implement this
		void registerSetBuf(const SetBuf_fn setBuf);
		void registerGetBuf(const GetBuf_fn getBuf);
	};

	class Input_Interface_C : public Device::Input_Interface {
	protected:
		CDevice* device;
	public:
		Input_Interface_C(CDevice* device);
		~Input_Interface_C();
		gpio::Tristate pressed(bool active);
	};

	CDevice(DeviceID id);
	~CDevice();
};
