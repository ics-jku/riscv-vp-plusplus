#pragma once

#include <QWidget>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPaintEvent>

#include <unordered_map>
#include <list>
#include <mutex> // TODO: FIXME: Create one Lua state per device that uses asyncs like SPI and synchronous pins

#include "configurations.h"
#include "types.h"
#include "embedded/gpio-helpers.h"
#include "devices/factory/factory.h"

class Breadboard : public QWidget {
	Q_OBJECT

	std::mutex lua_access;		//TODO: Use multiple Lua states per 'async called' device
	Factory factory;
	std::unordered_map<DeviceID,std::unique_ptr<Device>> devices;
	std::unordered_map<DeviceID,SPI_IOF_Request> spi_channels;
	std::unordered_map<DeviceID,PIN_IOF_Request> pin_channels;

	std::list<PinMapping> reading_connections;		// Semantic subject to change
	std::list<PinMapping> writing_connections;

	bool debugmode = false;
	QString bkgnd_path;

	// Device
	bool addDevice(DeviceClass classname, DeviceID device_id);
	void removeDevice(DeviceID id);

	// Connections
	void addPin(bool synchronous, gpio::PinNumber device_pin, gpio::PinNumber global, std::string name, Device* device);
	void addSPI(gpio::PinNumber global, bool noresponse, Device* device);
	void addGraphics(QPoint offset, unsigned scale, Device* device);

	void writeDevice(DeviceID id);

	// Drag and Drop
	Qt::DropAction startDrag(DeviceID device_id, QPoint hotspot, Qt::DropAction action);
	void dropEvent(QDropEvent* e) override;
	void dragEnterEvent(QDragEnterEvent* e) override;
	void dragMoveEvent(QDragMoveEvent* e) override;

	// QT
	void paintEvent(QPaintEvent* e) override;
	void keyPressEvent(QKeyEvent* e) override;
	void keyReleaseEvent(QKeyEvent* e) override;
	void mousePressEvent(QMouseEvent* e) override;
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

	// Devices
	void removeDeviceObjects(DeviceID id);
	std::list<DeviceClass> getAvailableDevices();

public slots:
	void connectionUpdate(bool active);
	bool addDevice(DeviceClass classname);

signals:
	void registerIOF_PIN(gpio::PinNumber gpio_offs, GpioClient::OnChange_PIN fun);
	void registerIOF_SPI(gpio::PinNumber gpio_offs, GpioClient::OnChange_SPI fun, bool noresponse);
	void closeAllIOFs(std::vector<gpio::PinNumber> gpio_offs);
	void closeDeviceIOFs(std::vector<gpio::PinNumber> gpio_offs, DeviceID device_id);
	void setBit(gpio::PinNumber gpio_offs, gpio::Tristate state);
};
