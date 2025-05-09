#include <stdint.h>
#include <stdio.h>

unsigned int spin_lock(uint32_t *lock) {
	uint32_t oval = 0;
	uint32_t nval = 1;
	unsigned int n_iter = -1;

	do {
		n_iter++;
		asm volatile("amoswap.w.aq %0, %1, 0(%2)" : "=r"(oval) : "r"(nval), "r"(lock) : "memory");
	} while (oval == 1);

	return n_iter;
}

void spin_unlock(volatile uint32_t *lock) {
	asm volatile("amoswap.w.rl zero, zero, 0(%0)" : : "r"(lock) : "memory");
}

static uint32_t print_lock;

int main(unsigned hart_id) {
	unsigned int n_iter = 15 * (hart_id + 1);

	// behave differently based on the core (i.e. hart) id
	for (unsigned i = 0; i < n_iter; ++i) {
		// simulate independent processing
		for (unsigned int j = 0; j < i * (10 * (hart_id + 1)); j++)
			;

		// print (shared ressource; locking)
		unsigned int lock_n_iter = spin_lock(&print_lock);
		printf("hart_id %u: iteration %u/%u (spin_lock loops = %u)\n", hart_id, i + 1, n_iter, lock_n_iter);
		spin_unlock(&print_lock);
	}

	return 0;
}
