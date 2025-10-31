#include <stdint.h>

/*
 * comment to disable read over bounds -> no exception
 */
#define ENABLE_READ_OVER_BOUNDS

void read_value(int32_t *p) {
	uint32_t value = *p;
}

int main() {
	int32_t array[5];
	uint64_t length = sizeof(array) / sizeof(array[0]);

	// Manually set each element to 0, to avoid use of memset
	for (int i = 0; i < length; i++) {
		array[i] = 0;
	}

	int32_t *p_array = array;

	// Read within bounds bounds (no cheri exception)
	for (uint32_t i = 0; i < length; i++) {
		read_value(p_array + i);
	}

#ifdef ENABLE_READ_OVER_BOUNDS
	// Intended read over bounds (cheri exception)
	for (uint32_t i = 0; i < length + 1; i++) {
		read_value(p_array + i);
	}
#endif /* READ OVER BOUNDS */

	return 0;
}
