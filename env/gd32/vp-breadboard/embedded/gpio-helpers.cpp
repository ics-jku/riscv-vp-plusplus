#include "gpio-helpers.h"

uint64_t translateGpioToExtPin(gpio::State state) {
	uint64_t ext = 0;
	for (gpio::PinNumber i = 0; i < 24; i++)  // Max Pin is 32,  but used are only first 24
	                                          // see SiFive HiFive1 Getting Started Guide 1.0.2 p. 20
	{
		// cout << i << " to ";;
		if (i >= 16) {
			ext |= (state.pins[i] == gpio::Pinstate::HIGH ? 1 : 0) << (i - 16);
			// cout << i - 16 << endl;
		} else if (i <= 5) {
			ext |= (state.pins[i] == gpio::Pinstate::HIGH ? 1 : 0) << (i + 8);
			// cout << i + 8 << endl;
		} else if (i >= 9 && i <= 13) {
			ext |= (state.pins[i] == gpio::Pinstate::HIGH ? 1 : 0) << (i + 6);
			;
		}
		// rest is not connected.
	}
	return ext;
}

uint8_t translatePinToGpioOffs(uint8_t pin) {
	if (pin < 8) {
		return pin + 16;  // PIN_0_OFFSET
	}
	if (pin >= 8 && pin < 14) {
		return pin - 8;
	}
	// ignoring non-wired pin 14 <==> 8
	if (pin > 14 && pin < 20) {
		return pin - 6;
	}

	return 0;
}

void printBin(char* buf, uint8_t len) {
	for (uint16_t byte = 0; byte < len; byte++) {
		for (int8_t bit = 7; bit >= 0; bit--) {
			printf("%c", buf[byte] & (1 << bit) ? '1' : '0');
		}
		printf(" ");
	}
	printf("\n");
}
