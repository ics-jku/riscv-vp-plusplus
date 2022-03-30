/*
 * cli-client.cpp
 *
 *  Created on: 7 Nov 2018
 *      Author: dwd
 */

#include <unistd.h>
#include <iostream>
#include <functional>

#include "gpio-client.hpp"

using namespace std;
using namespace gpio;


int justPrint(GpioClient& gpio) {
	while (true) { //just update the view
		if (!gpio.update()) {
			cerr << "Error updating" << endl;
			return -1;
		}
		GpioCommon::printState(gpio.state);
		usleep(125000);
	}
	return 0;
}

int setPins(GpioClient& gpio) {
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
	return 0;
}

int registerForSPI(GpioClient& gpio) {

	if (!gpio.update()) {
		cerr << "Error updating" << endl;
		return -1;
	}

	PinNumber spi_pin;
	//looking for all available SPI pins
	for(spi_pin = 0; spi_pin < max_num_pins; spi_pin++){
		if(gpio.state.pins[spi_pin] == Pinstate::IOF_SPI) {
			if(gpio.registerSPIOnChange(spi_pin,
					[spi_pin](SPI_Command c){
						cout << "Pin " << (int)spi_pin << " got SPI command " << (int)c << endl; return c%4;
					}
				)){
				cout << "Registered SPI on Pin " << (int)spi_pin << endl;
			} else {
				cerr << "Could not register SPI onchange" << endl;
				return -1;
			}
		}
	}

	while(gpio.update()){
		usleep(1000000);
		GpioCommon::printState(gpio.state);
	}
	return 0;
}

int main(int argc, char* argv[]) {
	if (argc < 3) {
		cout << "usage: " << argv[0] << " host port [testnr] (e.g. localhost 1339)" << endl;
		exit(-1);
	}

	GpioClient gpio;

	while(!gpio.setupConnection(argv[1], argv[2])) {
		cout << "connecting..." << endl;
		usleep(1000000);
	}
	cout << "connected." << endl;

	int test = 0;
	if (argc > 3)
		test = atoi(argv[3]);

	cout << "Running test nr " << test << endl;

	switch(test){
	case 0:
		return justPrint(gpio);
	case 1:
		return setPins(gpio);
	case 2:
		return registerForSPI(gpio);
	default:
		cerr << "Invalid test number given." << endl;
		return -1;
	}
}
