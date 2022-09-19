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

public slots:
	void loadJSON(QString file);
	void clearBreadboard();
	void loadLUA(std::string dir, bool overwrite_integrated_devices);

private slots:
	void timerUpdate();
	void connectionLost();
	void closeIOFs(std::vector<gpio::PinNumber> gpio_offs);

signals:
	void connectionUpdate(bool active);
	void sendStatus(QString message, int ms);
};
