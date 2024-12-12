#ifndef RISCV_VP_GPIO_IF_H
#define RISCV_VP_GPIO_IF_H

#include <stdint.h>

/* supports up to 64 gpios */
class GPIO_IF {
   public:
	const unsigned int N_GPIOS;
	const uint64_t N_GPIOS_MASK;

	GPIO_IF(unsigned int N_GPIOS) : N_GPIOS(N_GPIOS), N_GPIOS_MASK((1 << N_GPIOS) - 1) {}

	/* interface */
	virtual void set_gpios(uint64_t set, uint64_t mask) = 0;
	virtual uint64_t get_gpios() = 0;

	/* implementations */
	void set_gpio(unsigned int nr, bool val) {
		uint64_t set = (val ? 1 : 0) << nr;
		uint64_t mask = (1 << nr) & N_GPIOS_MASK;
		set_gpios(set, mask);
	}

	uint32_t get_gpio(unsigned int nr) {
		return get_gpios() & (1 << nr);
	}
};

#endif /* RISCV_VP_GPIO_IF_H */
