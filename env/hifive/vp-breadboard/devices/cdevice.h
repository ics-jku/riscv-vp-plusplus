#pragma once

#include "device.hpp"

class CDevice : public Device {
public:

	class PIN_Interface_C : public Device::PIN_Interface {
		PinLayout layout;
	public:
		PIN_Interface_C(PinLayout layout);
		~PIN_Interface_C();
		PinLayout getPinLayout();
		bool getPin(PinNumber num); // implement this
		void setPin(PinNumber num, bool val);	// implement this
	};

	class SPI_Interface_C : public Device::SPI_Interface {
	public:
		SPI_Interface_C();
		~SPI_Interface_C();
		uint8_t send(uint8_t byte); // implement this
	};

	class Config_Interface_C : public Device::Config_Interface {
		Config config;
	public:
		Config_Interface_C(Config config);
		~Config_Interface_C();
		Config getConfig();
		bool setConfig(const Config conf);
	};

	class Graphbuf_Interface_C : public Device::Graphbuf_Interface {
		Layout layout;
		SetBuf_fn set_buf;
		GetBuf_fn get_buf;
	public:
		Graphbuf_Interface_C(Layout layout);
		~Graphbuf_Interface_C();
		Layout getLayout();
		void initializeBufferMaybe(); // implement this
		void registerSetBuf(const SetBuf_fn setBuf);
		void registerGetBuf(const GetBuf_fn getBuf);
	};

	CDevice(DeviceID id, PinLayout pin_layout, bool implements_SPI, Config config, Layout graph_layout);
	~CDevice();
};
