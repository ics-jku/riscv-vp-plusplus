#include <stdint.h>
#include <stdio.h>
#include "platform.h"
#include "uart.h"
#include "util.h"
//#include "gpio.h"
void wait(uint32_t nOps) {
	/*
	 * TODO correlate nOps to clocks/time waited
	 */
	for(uint32_t i = 0; i < nOps; i++) {
		asm("nop");
	}
}

int main(int argc, char **argv) {
	uint8_t val = 1;

	*GPIO_BANK_A_DIRECTION_ADDR = 0x000000ff; // set all pins to output
	*GPIO_BANK_A_OUTPUT_ADDR = 0x01; // output 0x01 to all 8 pins of gpio bank A
	wait(10);
	*LED_ADDR = 0x05;
	wait(10);
	*LED_ADDR = 0x0A;
	wait(10);
	// bit shift pattern until only 0x55 and 0xAA are present
	for(uint8_t i=0; i<10; i++) {
		*GPIO_BANK_A_OUTPUT_ADDR = val;
		wait(10);
		val = (val << 1) | (i & 1);
	}
	*LED_ADDR = 0x0A;

	return 0;
}
