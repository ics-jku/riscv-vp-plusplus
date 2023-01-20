#include "uart.h"
#include "platform.h"
#include <stdint.h>

int sendString(char* str, long len) {
	long cnt = len;
	const char *s = (const char *)str;
	while (cnt > 0) {
		--cnt;
		putChr(*s);
		s++;
	}
	return len;
}

void putChr(char chr) {
	uint32_t tx_rdy = 0;
	UART->TXDATA = chr;
	// send character stored in uart_tx_addr
	UART->TXCTRL = 1;
	// wait until tx is ready again for sening another character
	do {
		// read uart tx status
		tx_rdy = UART->TXCTRL;
		asm volatile ("nop");
	} while(!tx_rdy);
	return;
}