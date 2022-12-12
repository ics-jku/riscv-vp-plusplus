#pragma once

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QWidget>
#include <list>
#include <mutex>  // TODO: FIXME: Create one Lua state per device that uses asyncs like SPI and synchronous pins
#include <unordered_map>

#include "configurations.h"

const static QString default_bkgnd = ":/img/virtual_breadboard.png";

class Breadboard : public QWidget {
	Q_OBJECT

	std::mutex lua_access;  // TODO: Use multiple Lua states per 'async called' device
	Factory factory;
	std::unordered_map<DeviceID, std::unique_ptr<Device>> devices;
	std::unordered_map<DeviceID, SPI_IOF_Request> spi_channels;
	std::unordered_map<DeviceID, PIN_IOF_Request> pin_channels;
	std::unordered_map<DeviceID, EXMC_IOF_Request> exmc_channels;
	std::unordered_map<DeviceID, DeviceGraphic> device_graphics;

	std::list<PinMapping> reading_connections;  // Semantic subject to change
	std::list<PinMapping> writing_connections;

	bool debugmode = false;
	QString bkgnd_path;

	// Connections
	void addPin(bool synchronous, gpio::PinNumber device_pin, gpio::PinNumber global, gpio::Port port, std::string name,
	            Device* device);
	void addSPI(gpio::PinNumber global, gpio::Port port, bool noresponse, Device* device);
	void addEXMC(gpio::PinNumber global, gpio::Port port, Device* device);
	void addGraphics(QPoint offset, unsigned scale, Device* device);

	void writeDevice(DeviceID device);

	// QT
	void paintEvent(QPaintEvent*) override;
	void keyPressEvent(QKeyEvent* e) override;
	void keyReleaseEvent(QKeyEvent* e) override;
	void mousePressEvent(QMouseEvent* e) override;
	void mouseMoveEvent(QMouseEvent* e) override;
	void mouseReleaseEvent(QMouseEvent* e) override;

   public:
	Breadboard();
	~Breadboard();

	bool toggleDebug();

	// JSON
	bool loadConfigFile(QString file);
	bool saveConfigFile(QString file);
	void additionalLuaDir(std::string additional_device_dir, bool overwrite_integrated_devices);
	void clear();
	void clearConnections();

	// GPIO
	void timerUpdate(gpio::State state);
	bool isBreadboard();

   public slots:
	void connectionUpdate(bool active, gpio::Port port);

   signals:
	void registerIOF_PIN(gpio::PinNumber pin, gpio::Port port, GpioClient::OnChange_PIN fun);
	void registerIOF_SPI(gpio::PinNumber pin, gpio::Port port, GpioClient::OnChange_SPI fun, bool noresponse);
	void registerIOF_EXMC(gpio::PinNumber pin, gpio::Port port, GpioClient::OnChange_EXMC fun);
	void closeIOF(gpio::PinNumber pin, gpio::Port port);
	void closeIOFs(std::unordered_map<gpio::PinNumber, gpio::Port> pin);
	void setBit(gpio::PinNumber pin, gpio::Port port, gpio::Tristate state);
};
