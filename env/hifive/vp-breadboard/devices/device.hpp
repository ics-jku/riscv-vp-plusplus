#pragma once

#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#include "configurations.h"

class Device {
protected:
	std::string m_id;

public:

	const std::string& getID() const;
	virtual const std::string getClass() const = 0;

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

		template<typename FunctionFootprint>
		void registerGlobalFunctionAndInsertLocalAlias(const std::string name, FunctionFootprint fun);
	};


	std::unique_ptr<PIN_Interface> pin;
	std::unique_ptr<SPI_Interface> spi;
	std::unique_ptr<Config_Interface> conf;
	std::unique_ptr<Graphbuf_Interface> graph;

	Device(const std::string id);
	virtual ~Device();
};
