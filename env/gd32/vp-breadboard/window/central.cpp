#include "central.h"

#include <QTimer>
#include <QVBoxLayout>

/* Constructor */

Central::Central(const std::string host, QWidget *parent) : QWidget(parent) {
	breadboard = new Breadboard();
	embedded = new Embedded(host);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addWidget(embedded);
	layout->addWidget(breadboard);
	layout->setSizeConstraint(QLayout::SetFixedSize);

	QTimer *timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, &Central::timerUpdate);
	timer->start(250);

	connect(breadboard, &Breadboard::registerIOF_PIN, embedded, &Embedded::registerIOF_PIN);
	connect(breadboard, &Breadboard::registerIOF_SPI, embedded, &Embedded::registerIOF_SPI);
	connect(breadboard, &Breadboard::registerIOF_EXMC, embedded, &Embedded::registerIOF_EXMC);
	connect(breadboard, &Breadboard::closeIOF, embedded, &Embedded::closeIOF);
	connect(breadboard, &Breadboard::closeIOFs, this, &Central::closeIOFs);
	connect(breadboard, &Breadboard::setBit, embedded, &Embedded::setBit);
	connect(embedded, &Embedded::connectionLost, this, &Central::connectionLost);
	connect(this, &Central::connectionUpdate, breadboard, &Breadboard::connectionUpdate);
}

Central::~Central() {}

void Central::connectionLost() {
	for (auto const &[_, port] : gpio::PORT_MAP) {
		if (port == gpio::Port::UNDEF) {
			continue;
		}
		emit(connectionUpdate(false, port));
	}
}

void Central::destroyConnection(gpio::Port port) {
	embedded->destroyConnection(port);
}

bool Central::toggleDebug() {
	return breadboard->toggleDebug();
}

void Central::closeIOFs(std::unordered_map<gpio::PinNumber, gpio::Port> iofs) {
	for (auto const &[pin, port] : iofs) {
		embedded->closeIOF(pin, port);
	}
	breadboard->clearConnections();
}

/* LOAD */

void Central::loadJSON(QString file) {
	emit(sendStatus("Loading config file " + file, 10000));
	breadboard->clear();
	if (!breadboard->loadConfigFile(file)) {
		emit(sendStatus("Config file " + file + " invalid.", 10000));
	}
	if (breadboard->isBreadboard()) {
		embedded->show();
	} else {
		embedded->hide();
	}
	for (auto const &[_, port] : gpio::PORT_MAP) {
		if (port == gpio::Port::UNDEF) {
			continue;
		}
		if (embedded->gpioConnected(port)) {
			breadboard->connectionUpdate(true, port);
		}
	}
}

void Central::saveJSON(QString file) {
	breadboard->saveConfigFile(file);
}

void Central::clearBreadboard() {
	emit(sendStatus("Clearing breadboard", 10000));
	breadboard->clear();
	embedded->show();
}

void Central::loadLUA(std::string dir, bool overwrite_integrated_devices) {
	breadboard->additionalLuaDir(dir, overwrite_integrated_devices);
}

/* Timer */

void Central::timerUpdate() {
	for (auto const &[_, port] : gpio::PORT_MAP) {
		if (port == gpio::Port::UNDEF) {
			continue;
		}
		bool reconnect = embedded->timerUpdate(port);
		if (reconnect) {
			emit(connectionUpdate(true, port));
		}
		if (embedded->gpioConnected(port)) {
			breadboard->timerUpdate(embedded->getState(port));
		}
	}
}
