#include <stdint.h>
#include <stdio.h>
#include "platform.h"
#include "uart.h"
#include "util.h"

int main(int argc, char **argv) {

	char buf[3];
	char *txt1 = "uart rx failed?!";

	char rxBuf[5];
	int rxEmpty = 1;
	rxEmpty = *UART_RX_FIFO_EMPTY_ADDR;
	if(rxEmpty) {
		// read rx data register from uart peripheral
		rxBuf[0] = *UART_RX_DATA_ADDR;
	} else {
		sendString(txt1,10);
		putChr('\n');
	}

	return 0;
}
