#include "embedded.h"

using namespace gpio;
using namespace std;

Embedded::Embedded(const std::string host) : QWidget(), host(host) {
	QSize bkgnd_size = QSize(417, 231);
	QString bkgnd_path = ":/img/virtual_hifive.png";
	QPixmap bkgnd(bkgnd_path);
	bkgnd = bkgnd.scaled(bkgnd_size, Qt::IgnoreAspectRatio);
	QPalette palette;
	palette.setBrush(QPalette::Window, bkgnd);
	this->setPalette(palette);
	this->setAutoFillBackground(true);
	setFixedSize(bkgnd_size);
}

Embedded::~Embedded() {}

/* GPIO */

bool Embedded::timerUpdate(gpio::Port port) {  // return: new connection?
	GpioObj* gpio = gpio_map.find(port)->second;

	if (gpio->connected && !gpio->client.update()) {
		emit(connectionLost());
		gpio->connected = false;
	}
	if (!gpio->connected) {
		gpio->connected = gpio->client.setupConnection(host.c_str(), std::to_string(static_cast<int>(port)).c_str());
		if (gpio->connected) {
			return true;
		}
	}
	return false;
}

State Embedded::getState(gpio::Port port) {
	return gpio_map.find(port)->second->client.state;
}

bool Embedded::gpioConnected(gpio::Port port) {
	return gpio_map.find(port)->second->connected;
}

void Embedded::registerIOF_PIN(PinNumber pin, gpio::Port port, GpioClient::OnChange_PIN fun) {
	GpioObj* gpio = gpio_map.find(port)->second;
	if (!gpio->client.isIOFactive(pin)) {
		const bool success = gpio->client.registerPINOnChange(pin, fun);
	}
}

void Embedded::registerIOF_SPI(PinNumber pin, gpio::Port port, GpioClient::OnChange_SPI fun, bool no_response) {
	GpioObj* gpio = gpio_map.find(port)->second;
	if (!gpio->client.isIOFactive(pin)) {
		const bool success = gpio->client.registerSPIOnChange(pin, fun, no_response);
	}
}

void Embedded::registerIOF_EXMC(PinNumber pin, gpio::Port port, GpioClient::OnChange_EXMC fun) {
	GpioObj* gpio = gpio_map.find(port)->second;
	if (!gpio->client.isIOFactive(pin)) {
		const bool success = gpio->client.registerEXMCOnChange(pin, fun);
	}
}

void Embedded::closeIOF(PinNumber pin, gpio::Port port) {
	gpio_map.find(port)->second->client.closeIOFunction(pin);
}

void Embedded::destroyConnection(gpio::Port port) {
	gpio_map.find(port)->second->client.destroyConnection();
}

void Embedded::setBit(gpio::PinNumber pin, gpio::Port port, gpio::Tristate state) {
	GpioObj* gpio = gpio_map.find(port)->second;
	if (gpio->connected) {
		gpio->client.setBit(pin, state);
	}
}
