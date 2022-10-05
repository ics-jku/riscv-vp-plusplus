#pragma once

#include <QWidget>

#include "breadboard/breadboard.h"
#include "embedded/embedded.h"

class Central : public QWidget {
	Q_OBJECT

	Breadboard *breadboard;
	Embedded *embedded;

   public:
	Central(const std::string host, QWidget *parent);
	~Central();
	void destroyConnection(gpio::Port port);
	bool toggleDebug();
	void saveJSON(QString file);

   public slots:
	void loadJSON(QString file);
	void clearBreadboard();
	void loadLUA(std::string dir, bool overwrite_integrated_devices);

   private slots:
	void timerUpdate();
	void connectionLost();
	void closeIOFs(std::unordered_map<gpio::PinNumber, gpio::Port> iofs);

   signals:
	void connectionUpdate(bool active, gpio::Port port);
	void sendStatus(QString message, int ms);
};
