#include <stdint.h>
#include <stdio.h>
// #include <string.h>
#include "platform.h"
#include "uart.h"
#include "util.h"

void delay(uint32_t loops) {
	for(uint32_t i = 0; i < loops; i++){
		asm volatile("nop");
	}
}

int main() {
	delay(10000);
	char buf[3];
	char *txt1 = "uart rx failed?!\n";
	delay(10000);
	delay(10000);
	delay(10000);
	delay(10000);

	char rxBuf[5];
	int rxEmpty = 1;
	int maxCnt = 20;
	int cnt = 0;
	rxEmpty = UART->RXEMPT;
	if(!rxEmpty) {
		// read rx data register from uart peripheral
		do{
			rxBuf[0] = UART->RXDATA;
			putChr(rxBuf[0]);
			cnt++;
		}while(UART->RXEMPT || cnt <= maxCnt);
		putChr('\n');
		putChr('\n');
	} else {
		sendString(txt1,1600);
		putChr('\n');
	}

	return 0;
}