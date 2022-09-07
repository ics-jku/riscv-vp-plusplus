#include "central.h"

/* Constructor */

Central::Central(QString configfile, std::string additional_device_dir, const std::string host, const std::string port, bool overwrite_integrated_devices, QWidget *parent) : QWidget(parent) {
	breadboard = new Breadboard(this);
	breadboard->additionalLuaDir(additional_device_dir, overwrite_integrated_devices);
	if(!breadboard->loadConfigFile(configfile)) {
		exit(-4);
	}

	embedded = new Embedded(host, port, breadboard->isBreadboard(), this);

	QVBoxLayout *layout = new QVBoxLayout(this);
	if(breadboard->isBreadboard()) {
		layout->addWidget(embedded);
	}
	else {
		embedded->setFixedSize(breadboard->size());
	}
	layout->addWidget(breadboard);

	QTimer *timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, &Central::timerUpdate);
	timer->start(250);

	connect(breadboard, &Breadboard::registerIOF_PIN, embedded, &Embedded::registerIOF_PIN);
	connect(breadboard, &Breadboard::registerIOF_SPI, embedded, &Embedded::registerIOF_SPI);
	connect(breadboard, &Breadboard::destroyConnection, embedded, &Embedded::destroyConnection);
	connect(breadboard, &Breadboard::setBit, embedded, &Embedded::setBit);
}

Central::~Central() {
	delete breadboard;
	delete embedded;
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
