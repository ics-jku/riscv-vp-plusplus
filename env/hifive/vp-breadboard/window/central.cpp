#include "central.h"

#include <QVBoxLayout>
#include <QTimer>

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
	connect(breadboard, &Breadboard::setBit, embedded, &Embedded::setBit);
	connect(embedded, &Embedded::connectionLost, this, &Central::connectionLost);
	connect(this, &Central::connectionUpdate, breadboard, &Breadboard::connectionUpdate);
}

Central::~Central() {
}

void Central::connectionLost() {
	emit(connectionUpdate(false));
}

void Central::destroyConnection() {
	embedded->destroyConnection();
}

bool Central::toggleDebug() {
	return breadboard->toggleDebug();
}

/* LOAD */

void Central::loadJSON(QString file) {
	emit(sendStatus("Loading config file " + file, 10000));
	breadboard->clear();
	if(!breadboard->loadConfigFile(file)) {
		emit(sendStatus("Config file " + file + " invalid.", 10000));
	}
	if(breadboard->isBreadboard()) {
		embedded->show();
	}
	else {
		embedded->hide();
	}
}

void Central::loadLUA(std::string dir, bool overwrite_integrated_devices) {
	breadboard->additionalLuaDir(dir, overwrite_integrated_devices);
}

/* Timer */

void Central::timerUpdate() {
 	bool reconnect = embedded->timerUpdate();
	if(reconnect) {
		emit(connectionUpdate(true));
	}
	if(embedded->gpioConnected()) {
		breadboard->timerUpdate(embedded->getState());
	}
}
