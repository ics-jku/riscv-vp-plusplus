#include "stdint.h"
#include "irq.h"
#include "stdio.h"
#include "assert.h"

static void set_next_timer_interrupt() {
	assert (mtime && mtimecmp);
	*mtimecmp = *mtime + 1000;  // 1000 timer ticks, corresponds to 1 MS delay with current CLINT configuration
}

unsigned int num_ticks = 0;

void timer_irq_handler() {
	set_next_timer_interrupt();
	++num_ticks;
}

int main() {
	printf("num_ticks %d\n", num_ticks);
	register_timer_interrupt_handler(timer_irq_handler);
	set_next_timer_interrupt();
	while (num_ticks < 10) {
		printf("num_ticks %d\n", num_ticks);
	}
	printf("num_ticks %d\n", num_ticks);

	return 0;
}
