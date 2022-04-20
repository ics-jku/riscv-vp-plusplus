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

bool gpio::isIOF(const Pinstate s){
	return static_cast<uint8_t>(s) >= static_cast<uint8_t>(Pinstate::START_OF_IOFs);
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
					cout << "-INVALID-";
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

void GpioCommon::printPinstate(const gpio::Pinstate& state) {
	switch(state) {
	case Pinstate::LOW:
		cout << "0";
		break;
	case Pinstate::HIGH:
		cout << "1";
		break;
	case Pinstate::UNSET:
		cout << "X";
		break;
	case Pinstate::IOF_SPI:
		cout << "s";
		break;
	case Pinstate::IOF_I2C:
		cout << "i";
		break;
	case Pinstate::IOF_PWM:
		cout << "p";
		break;
	case Pinstate::IOF_UART:
		cout << "u";
		break;
	default:
		cout << "?";
		break;
	}
}

void GpioCommon::printState(const State& state) {
	for(PinNumber pin = 0; pin < max_num_pins; pin++) {
		if(pin > 0 && pin % 8 == 0)
			cout << " ";
		printPinstate(state.pins[pin]);
	}
	cout << endl;
}

GpioCommon::GpioCommon() {
	memset(&state, 0, sizeof(State));
}
