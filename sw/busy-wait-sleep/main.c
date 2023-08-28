#include <stdint.h>

static volatile uint64_t *MTIMECMP_REG = (uint64_t *)0x2004000;
static volatile uint64_t *MTIME_REG = (uint64_t *)0x200bff8;

enum {
	SLEEP_TIME = 5,  // In seconds
	US_PER_SEC = 1000000,
};

/* This is used to quantize a 1MHz value to the closest 32768Hz value */
#define DIVIDEND ((uint64_t)15625 / (uint64_t)512)

int main() {
	uint64_t usec = SLEEP_TIME * US_PER_SEC;
	uint64_t ticks = usec / DIVIDEND;

	uint64_t target = *MTIME_REG + ticks;
	while (*MTIME_REG < target)
		;

	return 0;
}
