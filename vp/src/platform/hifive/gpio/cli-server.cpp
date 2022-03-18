/*
 * gpio-server.cpp
 *
 *  Created on: 5 Nov 2018
 *      Author: dwd
 */

#include <unistd.h>
#include <csignal>
#include <functional>
#include <iostream>
#include <thread>

#include "gpio-server.hpp"

using namespace std;
using namespace gpio;

bool stop = false;

void signalHandler(int signum) {
	cout << "Interrupt signal (" << signum << ") received.\n";

	if (stop)
		exit(signum);
	stop = true;
	raise(SIGUSR1);  // this breaks wait in thread
}

void onChangeCallback(GpioServer* gpio, PinNumber pin, Tristate val) {
	gpio->state.pins[pin] = val;
	printf("Bit %d changed to %d\n", pin, gpio->state.pins[pin] == Tristate::HIGH ? 1 : 0);
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		cout << "usage: " << argv[0] << " port (e.g. 1339)" << endl;
		exit(-1);
	}

	GpioServer gpio;

	if (!gpio.setupConnection(argv[1])) {
		cerr << "cant set up server" << endl;
		exit(-1);
	}

	signal(SIGINT, signalHandler);

	gpio.registerOnChange(bind(onChangeCallback, &gpio, placeholders::_1, placeholders::_2));
	thread server(bind(&GpioServer::startListening, &gpio));

	gpio.state.pins[0] = Tristate::IOF_SPI;
	gpio.state.pins[1] = Tristate::IOF_PWM;

	while (!stop && !gpio.isStopped()) {
		// some example actions
		usleep(100000);
		// here was a bitshift, implement this for lulz?

		auto pin = reinterpret_cast<uint8_t*>(&gpio.state.pins[11]);
		(*pin)++;
		if(*pin > 6)
			*pin = 0;
	}
	gpio.quit();
	server.join();
}
