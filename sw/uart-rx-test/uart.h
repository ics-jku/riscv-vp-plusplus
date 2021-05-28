#ifndef  UART_H

#include <stdint.h>

int sendString(char* str, long len);
void putChr(char chr);

int isRXEmpty(); // returns if fifo is empty
char readChr(); // read one character if fifo not empty
char readLine(); // read character until \n

#endif /* UART_H */