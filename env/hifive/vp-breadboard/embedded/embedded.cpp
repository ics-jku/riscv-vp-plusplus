#include "embedded.h"

#include "gpio-helpers.h"

using namespace gpio;
using namespace std;

Embedded::Embedded(const std::string host, const std::string port, QWidget *parent) :
		QWidget(parent), host(host), port(port) {
	QSize bkgnd_size = QSize(417, 231);
	QString bkgnd_path = ":/img/virtual_hifive.png";
	QPixmap bkgnd(bkgnd_path);
	bkgnd = bkgnd.scaled(bkgnd_size, Qt::IgnoreAspectRatio);
	QPalette palette;
	palette.setBrush(QPalette::Window, bkgnd);
	this->setPalette(palette);
	this->setAutoFillBackground(true);
	setFixedSize(bkgnd_size);
	setAttribute(Qt::WA_TransparentForMouseEvents);
}

Embedded::~Embedded() {}

/* GPIO */

bool Embedded::timerUpdate() { // return: new connection?
	if(connected && !gpio.update()) {
		connected = false;
	}
	if(!connected) {
		connected = gpio.setupConnection(host.c_str(), port.c_str());
		if(connected) {
			return true;
		}
	}
	return false;
}

State Embedded::getState() {
	return gpio.state;
}

bool Embedded::gpioConnected() {
	return connected;
}

void Embedded::registerIOF_PIN(PinNumber gpio_offs, GpioClient::OnChange_PIN fun) {
	if(!gpio.isIOFactive(gpio_offs)) {
		const bool success= gpio.registerPINOnChange(gpio_offs, fun);
	}
}

void Embedded::registerIOF_SPI(PinNumber gpio_offs, GpioClient::OnChange_SPI fun, bool no_response) {
	if(!gpio.isIOFactive(gpio_offs)) {
		const bool success = gpio.registerSPIOnChange(gpio_offs, fun, no_response);
	}
}

void Embedded::closeIOF(PinNumber gpio_offs) {
	gpio.closeIOFunction(gpio_offs);
}

void Embedded::destroyConnection() {
	gpio.destroyConnection();
}

void Embedded::setBit(gpio::PinNumber gpio_offs, gpio::Tristate state) {
	if(connected) {
		gpio.setBit(gpio_offs, state);
	}
}

/* PAINT */

void Embedded::paintEvent(QPaintEvent*) {
	if(!connected) {
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setBrush(QBrush(QColor("black")));
		QRect sign;
		if(this->size().width() > this->size().height())
		{
			sign = QRect (QPoint(this->size().width()/4, this->size().height()/4), this->size()/2);
		}
		else
		{
			sign = QRect (QPoint(this->size().width()/10, this->size().height()/4),
					QSize(4*this->size().width()/5, this->size().height()/4));
		}
		painter.drawRect(sign);
		painter.setFont(QFont("Arial", 25, QFont::Bold));
		QPen penHText(QColor("red"));
		painter.setPen(penHText);
		painter.drawText(sign, QString("No connection"), Qt::AlignHCenter | Qt::AlignVCenter);

		painter.end();
	}
}
