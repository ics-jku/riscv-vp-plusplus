#include <stdint.h>

typedef uint32_t BUS_BRIDGE_TYPE;
static volatile BUS_BRIDGE_TYPE * const BUS_BRIDGE_START = (BUS_BRIDGE_TYPE * const) 0x50000000;
static volatile char * const TERMINAL_ADDR = (char * const)0x20000000;

const unsigned num_bytes = 128;

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

int main() {
	char hi[] = "lol\n";
	for(int i = 0; hi[i]; i++)
		*TERMINAL_ADDR = hi[i];

	write_stuff();
	read_stuff();

	return 0;
}
