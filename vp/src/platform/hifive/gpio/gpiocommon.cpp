/*
 * gpio.cpp
 *
 *  Created on: 7 Nov 2018
 *      Author: dwd
 */

#include "gpiocommon.hpp"

#include <stdio.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

using namespace std;
using namespace gpio;

void hexPrint(unsigned char* buf, size_t size) {
	for (uint16_t i = 0; i < size; i++) {
		printf("%2X ", buf[i]);
	}
	cout << endl;
}

void bitPrint(unsigned char* buf, size_t size) {
	for (uint16_t byte = 0; byte < size; byte++) {
		for (int8_t bit = 7; bit >= 0; bit--) {
			printf("%c", buf[byte] & (1 << bit) ? '1' : '0');
		}
		printf(" ");
	}
	printf("\n");
}

bool gpio::isIOF(const Tristate s){
	return static_cast<uint8_t>(s) > 3;
}

void GpioCommon::printRequest(const Request& req) {
	switch (req.op) {
		case Request::Type::GET_BANK:
			cout << "GET BANK";
			break;
		case Request::Type::SET_BIT:
			cout << "SET BIT ";
			cout << to_string(req.setBit.pin) << " to ";
			switch (req.setBit.val) {
				case Tristate::LOW:
					cout << "LOW";
					break;
				case Tristate::HIGH:
					cout << "HIGH";
					break;
				case Tristate::UNSET:
					cout << "unset (FLOATING)";
					break;
				default:
					cout << "IO-Function driven (see other)";
					break;
			}
			break;
		case Request::Type::REQ_IOF:
			cout << "Request io-function (or logstate)";
			break;
		case Request::Type::END_IOF:
			cout << "End io-function (or logstate)";
			break;
		default:
			cout << "INVALID";
	}
	cout << endl;
};

void GpioCommon::printState(const State& state) {
	for(PinNumber pin = 0; pin < max_num_pins; pin++) {
		if(pin > 0 && pin % 8 == 0)
			cout << " ";
		switch(state.pins[pin]) {
		case Tristate::LOW:
			cout << "0";
			break;
		case Tristate::HIGH:
			cout << "1";
			break;
		case Tristate::UNSET:
			cout << "X";
			break;
		case Tristate::IOF_SPI:
			cout << "s";
			break;
		case Tristate::IOF_I2C:
			cout << "i";
			break;
		case Tristate::IOF_PWM:
			cout << "p";
			break;
		case Tristate::IOF_UART:
			cout << "u";
			break;
		default:
			cout << "?";
			break;
		}
	}
	cout << endl;
}

GpioCommon::GpioCommon() {
	memset(&state, 0, sizeof(State));
}
