#include <string.h>

#include "stdio.h"

static char* const MRAM_START_ADDR = reinterpret_cast<char* const>(0x60000000);
static const unsigned int MRAM_SIZE = 0x0FFFFFFF;

int main() {
	unsigned long counter = 0;
	char buffer[10] = {0};

	memcpy(&counter, MRAM_START_ADDR, sizeof(unsigned long));
	memcpy(buffer, MRAM_START_ADDR + 10, 10);

	printf("Before:\n");
	printf("%lu, %0.*s\n", counter, 10, buffer);

	memcpy(MRAM_START_ADDR, &(++counter), sizeof(unsigned long));
	memcpy(MRAM_START_ADDR + 10, "Kokosnuss", 10);

	memcpy(&counter, MRAM_START_ADDR, sizeof(unsigned long));
	memcpy(buffer, MRAM_START_ADDR + 10, 10);

	printf("After:\n");
	printf("%lu, %0.*s\n", counter, 10, buffer);
}
