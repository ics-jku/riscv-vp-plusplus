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
#ifdef MICRORV
	uint32_t tx_rdy = 0;
	*UART_TX_ADDR = chr;
	// send character stored in uart_tx_addr
	*UART_TX_CTRL = 1;
	// wait until tx is ready again for sening another character
	do {
		// read uart tx status
		tx_rdy = *UART_TX_CTRL;
		asm volatile ("nop");
	} while(!tx_rdy);
#else
	uint32_t tx_rdy = 0;
	*UART_TX_DATA_ADDR = chr;
	// send character stored in uart_tx_addr
	*UART_TX_CTRL_ADDR = 1;
	// wait until tx is ready again for sening another character
	do {
		// read uart tx status
		tx_rdy = *UART_TX_CTRL_ADDR;
		asm volatile ("nop");
	} while(!tx_rdy);
#endif
	return;
}