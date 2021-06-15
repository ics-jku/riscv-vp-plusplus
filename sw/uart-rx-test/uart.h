#ifndef  UART_H

#include <stdint.h>

int sendString(char* str, long len);
void putChr(char chr);

int is_RX_empty();	// is RX FIFO empty
char read_char();	// read one character if FIFO is not empty
char read_line();	// read characters until \n (newline)

#endif /* UART_H */
