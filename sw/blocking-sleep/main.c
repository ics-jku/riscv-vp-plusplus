#include <stdint.h>
#include <stdbool.h>

enum {
	SLEEP_TIME = 5, // In seconds
	US_PER_SEC = 1000000,
};

/* This is used to quantize a 1MHz value to the closest 32768Hz value */
#define DIVIDEND ((uint64_t)15625/(uint64_t)512)

/* Bitmask to extract exception code from mcause register */
#define MCAUSE_CAUSE 0x7FFFFFFF

/* Exception code for timer interrupts */
#define MACHINE_TIMER 7

/* Set after timer interrupt was received */
static volatile bool terminate = false;

static volatile uint64_t *MTIMECMP_REG = (uint64_t *)0x2004000;
static volatile uint64_t *MTIME_REG = (uint64_t *)0x200bff8;

void irq_handler(void) {
	uint32_t mcause;
	uint32_t code;

	mcause = 0;
	__asm__ volatile ("csrr %[ret], mcause"
	                  : [ret] "=r" (mcause));

	code = mcause & MCAUSE_CAUSE;
	if (code != MACHINE_TIMER || terminate)
		__asm__ volatile ("ebreak");

	// Attempt to clear timer interrupt
	*MTIMECMP_REG = UINT64_MAX;

	terminate = true;
	return;
}

int main() {
	uint64_t usec = SLEEP_TIME * US_PER_SEC;
	uint64_t ticks = usec / DIVIDEND;

	uint64_t target = *MTIME_REG + ticks;
	*MTIMECMP_REG = target;

	while (!terminate)
		__asm__ volatile ("wfi");

	return 0;
}
