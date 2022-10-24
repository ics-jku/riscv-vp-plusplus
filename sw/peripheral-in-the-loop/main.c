#include <stdint.h>
#include "../simple-sensor/irq.h"

typedef uint32_t BUS_BRIDGE_TYPE;
static volatile BUS_BRIDGE_TYPE * const BUS_BRIDGE_START = (BUS_BRIDGE_TYPE * const) 0x50000000;
static volatile char * const TERMINAL_ADDR = (char * const)0x20000000;
#define BUS_BRIDGE_ITR 2
#define num_bytes 128

void read_stuff() {
	for (int i=0; i<(num_bytes/sizeof(BUS_BRIDGE_TYPE)); i++) {
		const BUS_BRIDGE_TYPE datum = BUS_BRIDGE_START[i];
		for(int c = 0; c < sizeof(BUS_BRIDGE_TYPE); c++)
			*TERMINAL_ADDR = ((uint8_t*)&datum)[c];
	}
	*TERMINAL_ADDR = '\n';
}

void write_stuff() {
	for (int i=0; i<(num_bytes/sizeof(BUS_BRIDGE_TYPE)); ++i) {
		const BUS_BRIDGE_TYPE datum = 'a' + i;
		for(int c = 0; c < sizeof(BUS_BRIDGE_TYPE); c++)
			*TERMINAL_ADDR = ((uint8_t*)&datum)[c];
		BUS_BRIDGE_START[i] = datum;
	}
	*TERMINAL_ADDR = '\n';
}

volatile int was_itr_triggered = 0;
void virtual_bus_irq_handler() {
	was_itr_triggered = 1;
}

char* hi = "Interrupt was triggered\n";

int main() {
	register_interrupt_handler(BUS_BRIDGE_ITR, virtual_bus_irq_handler);

	write_stuff();

	while(!was_itr_triggered)
		asm volatile ("wfi");

	for(int i = 0; hi[i]; i++)
		*TERMINAL_ADDR = hi[i];

	read_stuff();

	return 0;
}
