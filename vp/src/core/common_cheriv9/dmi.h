#pragma once

#include <stdint.h>

#include <cstring>

namespace cheriv9 {
const uint8_t CLEN = 16;  // TODO Only true for RV64 // TODO Get this from ISS // CLEN = 2*MXLEN

class MemoryDMI {
	uint8_t *mem;
	uint64_t start;
	uint64_t size;
	uint64_t end;
	std::vector<bool> *tags;

	MemoryDMI(uint8_t *mem, uint64_t start, uint64_t size) : mem(mem), start(start), size(size), end(start + size) {}
	MemoryDMI(uint8_t *mem, uint64_t start, uint64_t size, std::vector<bool> *tags)
	    : mem(mem), start(start), size(size), end(start + size), tags(tags) {}

   public:
	static MemoryDMI create_start_end_mapping(uint8_t *mem, uint64_t start, uint64_t end) {
		assert(end > start);
		return create_start_size_mapping(mem, start, end - start);
	}

	static MemoryDMI create_start_end_mapping(uint8_t *mem, uint64_t start, uint64_t end, std::vector<bool> *tags) {
		assert(end > start);
		return create_start_size_mapping(mem, start, end - start, tags);
	}

	static MemoryDMI create_start_size_mapping(uint8_t *mem, uint64_t start, uint64_t size) {
		assert(start + size > start);
		return MemoryDMI(mem, start, size);
	}

	static MemoryDMI create_start_size_mapping(uint8_t *mem, uint64_t start, uint64_t size, std::vector<bool> *tags) {
		assert(start + size > start);
		return MemoryDMI(mem, start, size, tags);
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

	bool get_tag_from_addr(uint64_t addr) {
		return tags->at((addr - start) / CLEN);
	}

	inline void set_tag_for_addr(uint64_t addr, bool tag) {
		this->tags->at((addr - start) / CLEN) = tag;
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

	// For tagged data (CHERI)
	bool load(uint64_t addr, __uint128_t *p_data) {
		__uint128_t *src = get_mem_ptr_to_global_addr<__uint128_t>(addr);
		/*
		 * use memcpy to avoid problems with unaligned loads into standard C++ data types
		 * see https://blog.quarkslab.com/unaligned-accesses-in-cc-what-why-and-solutions-to-do-it-properly.html
		 * memcpy with fixed size will be optimized-out compilers and replaced with single load/stores on achitectures
		 * which allow unaligned accesses
		 */
		memcpy(p_data, src, sizeof(__uint128_t));
		// TODO Tag
		return get_tag_from_addr(addr);
	}

	template <typename T>
	void store(uint64_t addr, T value) {
		static_assert(std::is_integral<T>::value, "integer type required");
		T *dst = get_mem_ptr_to_global_addr<T>(addr);
		/* memcpy -> see note in load */
		memcpy(dst, &value, sizeof(value));
		set_tag_for_addr(addr, false);  // Every non capability store clears the tag
	}

	// For tagged data (CHERI)
	void store(uint64_t addr, __uint128_t data, bool tag) {
		__uint128_t *dst = get_mem_ptr_to_global_addr<__uint128_t>(addr);
		memcpy(dst, &data, sizeof(data));
		set_tag_for_addr(addr, tag);
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
} /* namespace cheriv9 */
