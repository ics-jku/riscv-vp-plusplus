#include "util/memory_map.h"

template <unsigned Modulo>
struct ModRegisterRange : public RegisterRange {
	ModRegisterRange(uint64_t start, uint64_t size) : RegisterRange(start, size) {
		end = start + Modulo * (size - 1);
		assert(end >= start);
	}

	bool contains(uint64_t addr) {
		return addr >= start && addr <= end && (addr - start) % Modulo == 0;
	}

	void assert_range(size_t len, uint64_t local_addr) {
		assert(len == 1);
	}
};