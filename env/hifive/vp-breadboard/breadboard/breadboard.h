#pragma once

#include <QtWidgets>
#include <unordered_map>
#include <list>
#include <mutex> // TODO: FIXME: Create one Lua state per device that uses asyncs like SPI and synchronous pins

#include "configurations.h"
#include "devices/c/all_devices.hpp"
#include "embedded/gpio-helpers.h"

static constexpr unsigned max_num_buttons = 7;

class Breadboard : public QWidget {
	Q_OBJECT

	// TODO get C-Device factory
	Sevensegment* sevensegment;
	RGBLed* rgbLed;
	OLED_mmap* oled_mmap;
	OLED_iof* oled_iof;
	Button* buttons[max_num_buttons];
	//

	std::mutex lua_access;		//TODO: Use multiple Lua states per 'async called' device
	LuaEngine lua_factory;
	typedef std::string DeviceID;
	std::unordered_map<DeviceID,Device> devices;
	std::unordered_map<DeviceID,SPI_IOF_Request> spi_channels;
	std::unordered_map<DeviceID,PIN_IOF_Request> pin_channels;
	std::unordered_map<DeviceID,DeviceGraphic> device_graphics;

	std::list<PinMapping> reading_connections;		// Semantic subject to change
	std::list<PinMapping> writing_connections;

	bool debugmode = false;
	unsigned moving_button = 0;

	void paintEvent(QPaintEvent*) override;
	void keyPressEvent(QKeyEvent* e) override;
	void keyReleaseEvent(QKeyEvent* e) override;
	void mousePressEvent(QMouseEvent* e) override;
	void mouseReleaseEvent(QMouseEvent* e) override;

	// TODO: Phase these out and decide based on config
	static uint8_t translatePinNumberToSevensegment(uint64_t pinmap);
	static uint8_t translatePinNumberToRGBLed(uint64_t pinmap);

public:
	Breadboard(QWidget *parent);
	~Breadboard();

	bool loadConfigFile(QString file, std::string additional_device_dir, bool overwrite_integrated_devices);

	void timerUpdate(gpio::State state);
	void reconnected();

signals:
	void registerIOF_PIN(gpio::PinNumber gpio_offs, GpioClient::OnChange_PIN fun);
	void registerIOF_SPI(gpio::PinNumber gpio_offs, GpioClient::OnChange_SPI fun, bool noresponse);
	void destroyConnection();
	void setBit(gpio::PinNumber gpio_offs, gpio::Tristate state);
};
