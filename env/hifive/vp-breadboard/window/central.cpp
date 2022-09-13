#include "central.h"

/* Constructor */

Central::Central(const std::string host, const std::string port, QWidget *parent) : QWidget(parent) {
	breadboard = new Breadboard(this);
	embedded = new Embedded(host, port, this);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addWidget(embedded);
	layout->addWidget(breadboard);

	QTimer *timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, &Central::timerUpdate);
	timer->start(250);

	connect(breadboard, &Breadboard::registerIOF_PIN, embedded, &Embedded::registerIOF_PIN);
	connect(breadboard, &Breadboard::registerIOF_SPI, embedded, &Embedded::registerIOF_SPI);
	connect(breadboard, &Breadboard::closeIOF, embedded, &Embedded::closeIOF);
	connect(breadboard, &Breadboard::destroyConnection, embedded, &Embedded::destroyConnection);
	connect(breadboard, &Breadboard::setBit, embedded, &Embedded::setBit);
}

Central::~Central() {
	delete breadboard;
	delete embedded;
}

/* LOAD */

void Central::loadJSON(QString file) {
	breadboard->clear();
	breadboard->loadConfigFile(file);
	if(breadboard->isBreadboard()) {
		embedded->show();
	}
	else {
		embedded->hide();
	}
	// TODO resize layout, central, window
}

void Central::loadLUA(std::string dir, bool overwrite_integrated_devices) {
	breadboard->additionalLuaDir(dir, overwrite_integrated_devices);
}

/* Timer */

void Central::timerUpdate() {
 	bool reconnect = embedded->timerUpdate();
	if(reconnect) {
		breadboard->reconnected();
	}
	if(embedded->gpioConnected()) {
		breadboard->timerUpdate(embedded->getState());
	}
}
