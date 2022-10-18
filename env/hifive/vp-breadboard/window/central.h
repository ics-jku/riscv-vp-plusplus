#pragma once

#include <QWidget>

#include "embedded/embedded.h"
#include "breadboard/breadboard.h"

class Central : public QWidget {
	Q_OBJECT

	Breadboard *breadboard;
	Embedded *embedded;

public:
	Central(const std::string host, const std::string port, QWidget *parent);
	~Central();
	void destroyConnection();
	bool toggleDebug();
	void saveJSON(QString file);
	std::list<DeviceClass> getAvailableDevices();

public slots:
	void loadJSON(QString file);
	void clearBreadboard();
	void loadLUA(std::string dir, bool overwrite_integrated_devices);

private slots:
	void timerUpdate();
	void connectionLost();
	void closeAllIOFs(std::vector<gpio::PinNumber> gpio_offs);
	void closeDeviceIOFs(std::vector<gpio::PinNumber> gpio_offs, DeviceID device);

signals:
	void connectionUpdate(bool active);
	void sendStatus(QString message, int ms);
};
