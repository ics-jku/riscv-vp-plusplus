#include <stdint.h>

#include "../simple-sensor/irq.h"

typedef uint32_t BUS_BRIDGE_TYPE;
static volatile BUS_BRIDGE_TYPE* const BUS_BRIDGE_START = (BUS_BRIDGE_TYPE* const)0x50000000;
static volatile BUS_BRIDGE_TYPE* const BUS_BRIDGE_END = (BUS_BRIDGE_TYPE* const)0x5000000F;  // INCLUSIVE
static const unsigned BUS_BRIDGE_ITR = 2;
static volatile char* const TERMINAL_ADDR = (char* const)0x20000000;

static const unsigned num_words = (BUS_BRIDGE_END - BUS_BRIDGE_START) + 1;  // INCLUSIVE

void read_stuff() {
	for (int i = 0; i < num_words; i++) {
		const BUS_BRIDGE_TYPE datum = BUS_BRIDGE_START[i];
		for (int c = 0; c < sizeof(BUS_BRIDGE_TYPE); c++) {
			*TERMINAL_ADDR = ((uint8_t*)&datum)[c];
		}
		*TERMINAL_ADDR = '\n';
	}
}

void write_stuff() {
	for (int i = 0; i < num_words; i++) {
		BUS_BRIDGE_TYPE datum;
		for (int c = 0; c < sizeof(BUS_BRIDGE_TYPE); c++) {
			((uint8_t*)&datum)[c] = 'a' + (i + c);
			*TERMINAL_ADDR = ((uint8_t*)&datum)[c];
		}
		BUS_BRIDGE_START[i] = datum;
		*TERMINAL_ADDR = '\n';
	}
}

volatile int was_itr_triggered = 0;
void virtual_bus_irq_handler() {
	static const char* hi = "Interrupt was triggered\n";
	was_itr_triggered = 1;
	for (int i = 0; hi[i]; i++) *TERMINAL_ADDR = hi[i];
}

int main() {
	register_interrupt_handler(BUS_BRIDGE_ITR, virtual_bus_irq_handler);

	read_stuff();

	/*
	while(!was_itr_triggered)
	    asm volatile ("wfi");
	*/

	write_stuff();

	return 0;
}
