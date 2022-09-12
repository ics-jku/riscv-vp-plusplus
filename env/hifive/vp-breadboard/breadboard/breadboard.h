#pragma once

#include <QtWidgets>
#include <unordered_map>
#include <list>
#include <mutex> // TODO: FIXME: Create one Lua state per device that uses asyncs like SPI and synchronous pins

#include "configurations.h"
#include "embedded/gpio-helpers.h"

const static QString default_bkgnd = ":/img/virtual_breadboard.png";

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
	QString bkgnd_path = default_bkgnd;

	// Connections
	void addPin(bool synchronous, gpio::PinNumber device_pin, gpio::PinNumber global, std::string name, Device* device);
	void addSPI(gpio::PinNumber global, bool noresponse, Device* device);
	void addGraphics(QPoint offset, unsigned scale, Device* device);

	void writeDevice(DeviceID device);

	// QT
	void paintEvent(QPaintEvent*) override;
	void keyPressEvent(QKeyEvent* e) override;
	void keyReleaseEvent(QKeyEvent* e) override;
	void mousePressEvent(QMouseEvent* e) override;
	void mouseReleaseEvent(QMouseEvent* e) override;

public:
	Breadboard(QWidget *parent);
	~Breadboard();

	// JSON
	bool loadConfigFile(QString file);
	bool saveConfigFile(QString file);
	void additionalLuaDir(std::string additional_device_dir, bool overwrite_integrated_devices);
	void clear();

	// GPIO
	void timerUpdate(gpio::State state);
	void reconnected();
	bool isBreadboard();

signals:
	void registerIOF_PIN(gpio::PinNumber gpio_offs, GpioClient::OnChange_PIN fun);
	void registerIOF_SPI(gpio::PinNumber gpio_offs, GpioClient::OnChange_SPI fun, bool noresponse);
	void closeIOF(gpio::PinNumber gpio_offs);
	void destroyConnection();
	void setBit(gpio::PinNumber gpio_offs, gpio::Tristate state);
};
