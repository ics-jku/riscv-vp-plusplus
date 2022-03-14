/*
 * cli-client.cpp
 *
 *  Created on: 7 Nov 2018
 *      Author: dwd
 */

#include <unistd.h>
#include <iostream>

#include "gpio-client.hpp"

using namespace std;
using namespace gpio;

int main(int argc, char* argv[]) {
	if (argc < 3) {
		cout << "usage: " << argv[0] << " host port (e.g. localhost 1339)" << endl;
		exit(-1);
	}

	GpioClient gpio;

	if (!gpio.setupConnection(argv[1], argv[2])) {
		cout << "cant setup connection" << endl;
		return -1;
	}

	while (true) { //just update the view
		if (!gpio.update()) {
			cerr << "Error updating" << endl;
			return -1;
		}
		GpioCommon::printState(gpio.state);
		usleep(125000);
	}

	// example actions
	for (uint8_t i = 0; i < 64; i++) {
		if (!gpio.setBit(i, Tristate::HIGH)) {
			cerr << "Error setting Bit " << i << endl;
			return -1;
		}
		if (!gpio.update()) {
			cerr << "Error updating" << endl;
			return -1;
		}
		GpioCommon::printState(gpio.state);
		usleep(750);
	}

	for (uint8_t i = 0; i < 64; i++) {
		if (!gpio.setBit(i, Tristate::LOW)) {
			cerr << "Error resetting Bit " << i << endl;
			return -1;
		}
		if (!gpio.update()) {
			cerr << "Error updating" << endl;
			return -1;
		}
		GpioCommon::printState(gpio.state);
		usleep(750);
	}
}
