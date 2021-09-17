#ifndef  UTIL_H

#include <stdint.h>
#include <stdbool.h>

void swap(char *x, char *y);
char* reverse(char *buffer, int i, int j);
char* itoa(int value, char* buffer, int base);

//void sleep_ms(uint64_t);

#endif /* UTIL_H */
