#ifndef  UTIL_H

//#include "_stdint.h"
#include <stdint.h>
#include <stdbool.h>

void swap(char *x, char *y);
char* reverse(char *buffer, int i, int j);
char* itoa(int value, char* buffer, int base);

void delay(uint32_t);

//void sleep_ms(uint64_t);

#endif /* UTIL_H */
