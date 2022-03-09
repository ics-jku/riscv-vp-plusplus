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

bool stop = false;

void signalHandler(int signum) {
	cout << "Interrupt signal (" << signum << ") received.\n";

	if (stop)
		exit(signum);
	stop = true;
	raise(SIGUSR1);  // this breaks wait in thread
}

void onChangeCallback(GpioServer* gpio, uint8_t bit, GpioCommon::Tristate val) {
	if (val == GpioCommon::Tristate::LOW) {
		gpio->state &= ~(1l << bit);
	} else if (val == GpioCommon::Tristate::HIGH) {
		gpio->state |= 1l << bit;
	} else {
		printf("Ignoring tristate for now\n");
		return;
	}
	printf("Bit %d changed to %ld\n", bit, (gpio->state & (1l << bit)) >> bit);
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

	while (!stop && !gpio.isStopped()) {
		// some example actions
		usleep(100000);
		if (!(gpio.state & (1 << 11))) {
			gpio.state <<= 1;
			if (!(gpio.state & 0xFF)) {
				gpio.state = 1;
			}
		}
	}
	gpio.quit();
	server.join();
}
