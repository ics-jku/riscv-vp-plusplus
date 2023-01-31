#pragma once

#include "devices/device.hpp"

class CDevice : public Device {
   public:
	QImage* image;
	Layout layout_graph;
	PinLayout layout_pin;
	Config* config;

	class PIN_Interface_C : public Device::PIN_Interface {
	   protected:
		CDevice* device;

	   public:
		PIN_Interface_C(CDevice* device);
		~PIN_Interface_C();
		PinLayout getPinLayout();
		gpio::Tristate getPin(PinNumber num);            // implement this
		void setPin(PinNumber num, gpio::Tristate val);  // implement this
	};

	class SPI_Interface_C : public Device::SPI_Interface {
	   protected:
		CDevice* device;

	   public:
		SPI_Interface_C(CDevice* device);
		~SPI_Interface_C();
		gpio::SPI_Response send(gpio::SPI_Command byte);  // implement this
	};

	class EXMC_Interface_C : public Device::EXMC_Interface {
	   protected:
		CDevice* device;

	   public:
		EXMC_Interface_C(CDevice* device);
		~EXMC_Interface_C();
		gpio::EXMC_Data send(gpio::EXMC_Data data);  // implement this
		                                             // ToDo Methods
	};

	class Config_Interface_C : public Device::Config_Interface {
	   protected:
		CDevice* device;

	   public:
		Config_Interface_C(CDevice* device);
		~Config_Interface_C();
		Config* getConfig();
		bool setConfig(Config* conf);
	};

	class Graphbuf_Interface_C : public Device::Graphbuf_Interface {
	   protected:
		CDevice* device;

	   public:
		Graphbuf_Interface_C(CDevice* device);
		~Graphbuf_Interface_C();
		Layout getLayout();
		void initializeBufferMaybe();  // implement this
		void registerBuffer(QImage& image);
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

	class TFT_Input_Interface_C : public Device::TFT_Input_Interface {
	   protected:
		CDevice* device;

	   public:
		TFT_Input_Interface_C(CDevice* device);
		~TFT_Input_Interface_C();
		void onClick(bool active, QMouseEvent* e);
	};

	CDevice(DeviceID id);
	~CDevice();
	void setBuffer(const Xoffset, const Yoffset, Pixel);
	Pixel getBuffer(const Xoffset, const Yoffset);
};
