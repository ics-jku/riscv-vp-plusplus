#pragma once

#include <QtWidgets>
#include <unordered_map>
#include <list>
#include <mutex> // TODO: FIXME: Create one Lua state per device that uses asyncs like SPI and synchronous pins

#include "configurations.h"
#include "devices/c/all_devices.hpp"
#include "embedded/gpio-helpers.h"

class Breadboard : public QWidget {
	Q_OBJECT

	std::mutex lua_access;		//TODO: Use multiple Lua states per 'async called' device
	Factory factory;
	std::unordered_map<DeviceID,std::unique_ptr<Device>> devices;
	std::unordered_map<DeviceID,SPI_IOF_Request> spi_channels;
	std::unordered_map<DeviceID,PIN_IOF_Request> pin_channels;
	std::unordered_map<DeviceID,DeviceGraphic> device_graphics;

	std::list<PinMapping> reading_connections;		// Semantic subject to change
	std::list<PinMapping> writing_connections;

	bool debugmode = false;
	bool breadboard = true;

	void writeDevice(DeviceID device);

	void paintEvent(QPaintEvent*) override;
	void keyPressEvent(QKeyEvent* e) override;
	void keyReleaseEvent(QKeyEvent* e) override;
	void mousePressEvent(QMouseEvent* e) override;
	void mouseReleaseEvent(QMouseEvent* e) override;

public:
	Breadboard(QWidget *parent);
	~Breadboard();

	bool loadConfigFile(QString file, std::string additional_device_dir, bool overwrite_integrated_devices);

	void timerUpdate(gpio::State state);
	void reconnected();
	bool isBreadboard();

signals:
	void registerIOF_PIN(gpio::PinNumber gpio_offs, GpioClient::OnChange_PIN fun);
	void registerIOF_SPI(gpio::PinNumber gpio_offs, GpioClient::OnChange_SPI fun, bool noresponse);
	void destroyConnection();
	void setBit(gpio::PinNumber gpio_offs, gpio::Tristate state);
};
