#pragma once

#include <QWidget>
#include <gpio/gpio-client.hpp>

struct GpioObj {
	GpioClient client;
	bool connected = false;
};

class Embedded : public QWidget {
	Q_OBJECT

	GpioObj gpioa;
	GpioObj gpiob;
	GpioObj gpioc;
	GpioObj gpiod;
	GpioObj gpioe;

	const std::unordered_map<gpio::Port, GpioObj*> gpio_map{
	    {gpio::Port::A, &gpioa}, {gpio::Port::B, &gpiob}, {gpio::Port::C, &gpioc},
	    {gpio::Port::D, &gpiod}, {gpio::Port::E, &gpioe},
	};

	const std::string host;

   public:
	Embedded(const std::string host);
	~Embedded();

	bool timerUpdate(gpio::Port port);
	gpio::State getState(gpio::Port port);
	bool gpioConnected(gpio::Port port);
	void destroyConnection(gpio::Port port);

   public slots:
	void registerIOF_PIN(gpio::PinNumber pin, gpio::Port port, GpioClient::OnChange_PIN fun);
	void registerIOF_SPI(gpio::PinNumber pin, gpio::Port port, GpioClient::OnChange_SPI fun, bool noresponse);
	void registerIOF_EXMC(gpio::PinNumber pin, gpio::Port port, GpioClient::OnChange_EXMC fun);
	void closeIOF(gpio::PinNumber pin, gpio::Port port);
	void setBit(gpio::PinNumber pin, gpio::Port port, gpio::Tristate state);

   signals:
	void connectionLost();
};
