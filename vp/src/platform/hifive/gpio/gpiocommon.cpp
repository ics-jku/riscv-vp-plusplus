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

void GpioCommon::printRequest(Request* req) {
	switch (req->op) {
		case Request::Type::GET_BANK:
			cout << "GET BANK";
			break;
		case Request::Type::SET_BIT:
			cout << "SET BIT ";
			cout << to_string(req->setBit.pin) << " to ";
			switch (req->setBit.val) {
				case Tristate::LOW:
					cout << "LOW";
					break;
				case Tristate::HIGH:
					cout << "HIGH";
					break;
				case Tristate::IOF:
					cout << "IO-Function driven (see other)";
					break;
				case Tristate::UNSET:
					cout << "unset (FLOATING)";
			}
			break;
		default:
			cout << "INVALID";
	}
	cout << endl;
};

GpioCommon::GpioCommon() {
	state = 0;
}
