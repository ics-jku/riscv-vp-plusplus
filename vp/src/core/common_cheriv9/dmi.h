#pragma once

#include <stdint.h>

class MemoryDMI {
	uint8_t *mem;
	uint64_t start;
	uint64_t size;
	uint64_t end;

	MemoryDMI(uint8_t *mem, uint64_t start, uint64_t size) : mem(mem), start(start), size(size), end(start + size) {}

   public:
	static MemoryDMI create_start_end_mapping(uint8_t *mem, uint64_t start, uint64_t end) {
		assert(end > start);
		return create_start_size_mapping(mem, start, end - start);
	}

	static MemoryDMI create_start_size_mapping(uint8_t *mem, uint64_t start, uint64_t size) {
		assert(start + size > start);
		return MemoryDMI(mem, start, size);
	}

	uint8_t *get_raw_mem_ptr() {
		return mem;
	}

	template <typename T>
	T *get_mem_ptr_to_global_addr(uint64_t addr) {
		assert(contains(addr));
		assert((addr + sizeof(T)) <= end);
		// assert ((addr % sizeof(T)) == 0 && "unaligned access");   //NOTE: due to compressed instructions, fetching
		// can be unaligned
		return reinterpret_cast<T *>(mem + (addr - start));
	}

	template <typename T>
	T load(uint64_t addr) {
		static_assert(std::is_integral<T>::value, "integer type required");
		T ans;
		T *src = get_mem_ptr_to_global_addr<T>(addr);
		/*
		 * use memcpy to avoid problems with unaligned loads into standard C++ data types
		 * see https://blog.quarkslab.com/unaligned-accesses-in-cc-what-why-and-solutions-to-do-it-properly.html
		 * memcpy with fixed size will be optimized-out compilers and replaced with single load/stores on achitectures
		 * which allow unaligned accesses
		 */
		memcpy(&ans, src, sizeof(T));
		return ans;
	}

	template <typename T>
	void store(uint64_t addr, T value) {
		static_assert(std::is_integral<T>::value, "integer type required");
		T *dst = get_mem_ptr_to_global_addr<T>(addr);
		/* memcpy -> see note in load */
		memcpy(dst, &value, sizeof(value));
	}

	uint64_t get_start() {
		return start;
	}

	uint64_t get_end() {
		return start + size;
	}

	uint64_t get_size() {
		return size;
	}

	bool contains(uint64_t addr) {
		return addr >= start && addr < end;
	}
};
