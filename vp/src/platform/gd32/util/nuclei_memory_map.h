#include "util/memory_map.h"

template <unsigned Modulo>
struct ModRegisterRange : public RegisterRange {
	ModRegisterRange(uint64_t start, uint64_t size) : RegisterRange(start, size) {
		end = start + Modulo * (size - 1);
		assert(end >= start);
	}

	uint64_t to_local(uint64_t addr) override {
		return ((addr - start) / 4);
	}

	bool contains(uint64_t addr) override {
		return addr >= start && addr <= end && (addr - start) % Modulo == 0;
	}

	void assert_range(size_t len, uint64_t local_addr) override {
		assert(len == 1);
	}
};