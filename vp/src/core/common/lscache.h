/*
 * Copyright (C) 2024 Manfred Schlaegl <manfred.schlaegl@gmx.at>
 *
 * Load/Store Cache
 * Allows direct translation of in-simulation virtual addresses to (dmi-capable) host system memory addresses, to speed
 * up load and stores on data memory. For hits, calls to the memory interface (including virtual address translation)
 * are omitted. Instead, data is directly accessed via dereferencing cached page pointers + page offsets.
 *
 * More details on the initial version can be found in the paper
 * "Fast Interpreter-Based Instruction Set Simulation for Virtual Prototypes"
 * by Manfred Schlaegl and Daniel Grosse
 * presented at the Design, Automation and Test in Europe Conference 2025
 */

#ifndef RISCV_ISA_LSCACHE_H
#define RISCV_ISA_LSCACHE_H

#include <mem_if.h>

#include <climits>
#include <cstdint>
#include <string>

#include "lscache_stats.h"
#include "util/common.h"

/******************************************************************************
 * BEGIN: CONFIG
 ******************************************************************************/

/*
 * enable/disable cache
 * if disabled, the dummy implementation is used
 */
#define LSCACHE_ENABLED
// #undef LSCACHE_ENABLED

/*
 * forces the cache to be always enabled, independent of the runtime configuration
 * this eliminates the runtime checks (located in cache miss) -> max performance
 */
// #define LSCACHE_FORCED_ENABLED
#undef LSCACHE_FORCED_ENABLED

/*
 * enable statistics
 * (expensive)
 */
// #define LSCACHE_STATS_ENABLED
#undef LSCACHE_STATS_ENABLED

/******************************************************************************
 * END: CONFIG
 ******************************************************************************/

/******************************************************************************
 * BEGIN: DUMMY/INTERFACE IMPLEMENTATION
 ******************************************************************************/

template <typename T_sxlen_t, typename T_uxlen_t>
class LSCache_IF_T {
   protected:
	using dmemif_t = data_memory_if_T<T_sxlen_t, T_uxlen_t>;
	bool enabled;
	uint64_t hartId = 0;
	dmemif_t *data_mem = nullptr;

   public:
	LSCache_IF_T() {
		init(false, 0, nullptr);
	}

	void init(bool enabled, uint64_t hartId, dmemif_t *data_mem) {
		this->enabled = enabled;
		this->hartId = hartId;
		this->data_mem = data_mem;
	}

	__always_inline bool is_enabled() {
#ifdef LSCACHE_FORCED_ENABLED
		return true;
#else
		return enabled;
#endif
	}

	__always_inline void fence() {
		// not using out of order execution/caches so can be ignored
	}

	__always_inline void fence_vma() {
		data_mem->flush_tlb();
	}

	__always_inline int64_t load_double(uint64_t addr) {
		return data_mem->load_double(addr);
	}
	__always_inline T_sxlen_t load_word(uint64_t addr) {
		return data_mem->load_word(addr);
	}
	__always_inline T_sxlen_t load_half(uint64_t addr) {
		return data_mem->load_half(addr);
	}
	__always_inline T_sxlen_t load_byte(uint64_t addr) {
		return data_mem->load_byte(addr);
	}
	__always_inline T_uxlen_t load_uword(uint64_t addr) {
		return data_mem->load_uword(addr);
	}
	__always_inline T_uxlen_t load_uhalf(uint64_t addr) {
		return data_mem->load_uhalf(addr);
	}
	__always_inline T_uxlen_t load_ubyte(uint64_t addr) {
		return data_mem->load_ubyte(addr);
	}

	__always_inline void store_double(uint64_t addr, uint64_t value) {
		data_mem->store_double(addr, value);
	}
	__always_inline void store_word(uint64_t addr, uint32_t value) {
		data_mem->store_word(addr, value);
	}
	__always_inline void store_half(uint64_t addr, uint16_t value) {
		data_mem->store_half(addr, value);
	}
	__always_inline void store_byte(uint64_t addr, uint8_t value) {
		data_mem->store_byte(addr, value);
	}
};
template <typename T_sxlen_t, typename T_uxlen_t>
using LSCacheDummy_T = LSCache_IF_T<T_sxlen_t, T_uxlen_t>;

/******************************************************************************
 * END: DUMMY/INTERFACE IMPLEMENTATION
 ******************************************************************************/

/******************************************************************************
 * BEGIN: FUNCTIONAL IMPLEMENTATION
 ******************************************************************************/

/*
 * Cache configuration:
 *
 * 64 bit addr: 0xFFFF FFFF FFF|F F|FFF
 *                TAG          |IDX|OFFS (4KiB page)
 * OFF: 12 bit
 * IDX: 8 bit -> 256 pages
 * TAG: 64 - 12 - 8 = 44 bit -> uint64_t necessary, but
 * lowest bits is used as load/store valid
 * NOTE: we need a flag to indicate stores because a successful load on an address does not automatically mean, that
 * a store is allowed (permissions in page table)
 *
 * NOTES/TODOS:
 * We are using direct dereferencing of poiners without any costly checks, which leads to problem on unaligned accesses:
 * 1. See: dmi.h and https://blog.quarkslab.com/unaligned-accesses-in-cc-what-why-and-solutions-to-do-it-properly.html
 * 2. A unaligned access on a page boundary may leads to problems (address + length may be on different page) -> see
 * comment in mem.h CombinedMemoryInterface_T::load_instr However, load/stores are always aligned (see ISS), so we can
 * ignore this.
 * TODO: Check uses of load/stores in vector (v.h) -> implement dedicated load/stores for vector, that
 *  1. Properly handle unaligned access (will hurt performance)
 *  2. Are optimized for vector (load/store of multiple loads) (will improve performance)
 * TODO: check, if checks for bus lock on load are really necessary (atomic operations?)
 * TODO: check inline vs __always_inline
 */

template <typename T_sxlen_t, typename T_uxlen_t>
class LSCache_T : public LSCache_IF_T<T_sxlen_t, T_uxlen_t> {
   protected:
	using super = LSCache_IF_T<T_sxlen_t, T_uxlen_t>;
	using dmemif_t = data_memory_if_T<T_sxlen_t, T_uxlen_t>;
#ifdef LSCACHE_STATS_ENABLED
	using lscachestats_t = LSCacheStats_T<LSCache_T>;
#else
	using lscachestats_t = LSCacheStatsDummy_T<LSCache_T>;
#endif
	friend lscachestats_t;

	lscachestats_t stats = lscachestats_t(*this);

#define LSCACHE_SETS (1 << 8)
#define LSCACHE_OFF(_virt_page_addr) ((_virt_page_addr) & 0x00FFF)
#define LSCACHE_IDX(_virt_page_addr) (((_virt_page_addr) & 0xFF000) >> 12)
#define LSCACHE_TAG(_virt_page_addr) ((_virt_page_addr) & (~0xFFFFF))
#define LSCACHE_LOAD_VALID_BITS (1 << 0)
#define LSCACHE_STORE_VALID_BITS ((1 << 1) | LSCACHE_LOAD_VALID_BITS)  // load & store
#define LSCACHE_IS_LOAD_VALID(_virt_page_addr) ((_virt_page_addr) & LOAD_VALID_BITS)
#define LSCACHE_IS_STORE_VALID(_virt_page_addr) ((_virt_page_addr) & STORE_VALID_BITS)

	struct Entry {
		T_uxlen_t tag_valid;
		void *host_page_addr;
	};
	struct Entry cache[LSCACHE_SETS];

	inline void flush() {
		memset(cache, 0, LSCACHE_SETS * sizeof(Entry));
	}

	inline void update(uint64_t virt_addr, void *host_page_addr, uint32_t valid_bits) {
		int idx = LSCACHE_IDX(virt_addr);
		uint64_t tag_valid = LSCACHE_TAG(virt_addr) | valid_bits;
		cache[idx].tag_valid = tag_valid;
		cache[idx].host_page_addr = host_page_addr;
	}

	__always_inline void *try_get_from_cache_load(uint64_t virt_addr) {
		int idx = LSCACHE_IDX(virt_addr);
		uint64_t tag_valid = LSCACHE_TAG(virt_addr) | LSCACHE_LOAD_VALID_BITS;

		if (likely(tag_valid == (cache[idx].tag_valid & ~(1 << 1)))) {
			stats.inc_hit_load();
			return (((uint8_t *)cache[idx].host_page_addr) + LSCACHE_OFF(virt_addr));
		}
		return nullptr;
	}

	__always_inline void *try_get_from_cache_store(uint64_t virt_addr) {
		int idx = LSCACHE_IDX(virt_addr);
		uint64_t tag_valid = LSCACHE_TAG(virt_addr) | LSCACHE_STORE_VALID_BITS;

		if (likely(tag_valid == cache[idx].tag_valid)) {
			stats.inc_hit_store();
			return (((uint8_t *)cache[idx].host_page_addr) + LSCACHE_OFF(virt_addr));
		}
		return nullptr;
	}

	void try_add_to_cache(uint64_t virt_addr, uint32_t valid_bits) {
		void *host_page_addr = this->data_mem->get_last_dmi_page_host_addr();
		if (host_page_addr != nullptr) {
			stats.inc_dmi();
			// do not add to cache, if disabled
			if (likely(this->is_enabled())) {
				update(virt_addr, host_page_addr, valid_bits);
			}
		} else {
			stats.inc_no_dmi();
		}
	}

	template <typename RET_T>
	using Load_F = RET_T (dmemif_t::*)(uint64_t addr);

	template <typename RET_T, typename CAST_T, Load_F<RET_T> load_f>
	__always_inline RET_T load(uint64_t addr) {
		stats.inc_loads();
		if (unlikely(this->data_mem->is_bus_locked())) {
			stats.inc_bus_locked();
			return ((this->data_mem)->*(load_f))(addr);
		}

		CAST_T *haddr = (CAST_T *)try_get_from_cache_load(addr);
		if (unlikely(haddr == nullptr)) {
			CAST_T ret = ((this->data_mem)->*(load_f))(addr);
			try_add_to_cache(addr, LSCACHE_LOAD_VALID_BITS);
			return ret;
		}

		return *haddr;
	}

	template <typename ARG_T>
	using Store_F = void (dmemif_t::*)(uint64_t addr, ARG_T value);

	template <typename ARG_T, Store_F<ARG_T> store_f>
	__always_inline void store(uint64_t addr, ARG_T value) {
		stats.inc_stores();
		if (unlikely(this->data_mem->is_bus_locked())) {
			stats.inc_bus_locked();
			((this->data_mem)->*(store_f))(addr, value);
			return;
		}

		ARG_T *haddr = (ARG_T *)try_get_from_cache_store(addr);
		if (unlikely(haddr == nullptr)) {
			((this->data_mem)->*(store_f))(addr, value);
			try_add_to_cache(addr, LSCACHE_STORE_VALID_BITS);
			return;
		}

		*haddr = value;
	}

   public:
	LSCache_T() {
		init(false, 0, nullptr);
	}

	void init(bool enabled, uint64_t hartId, dmemif_t *data_mem) {
		flush();
		super::init(enabled, hartId, data_mem);
	}

	__always_inline void fence_vma() {
		stats.inc_flushs();
		flush();
		super::fence_vma();
	}

	__always_inline int64_t load_double(uint64_t addr) {
		return load<int64_t, int64_t, &dmemif_t::load_double>(addr);
	}
	__always_inline T_sxlen_t load_word(uint64_t addr) {
		return load<T_sxlen_t, int32_t, &dmemif_t::load_word>(addr);
	}
	__always_inline T_sxlen_t load_half(uint64_t addr) {
		return load<T_sxlen_t, int16_t, &dmemif_t::load_half>(addr);
	}
	__always_inline T_sxlen_t load_byte(uint64_t addr) {
		return load<T_sxlen_t, int8_t, &dmemif_t::load_byte>(addr);
	}
	__always_inline T_uxlen_t load_uword(uint64_t addr) {
		return load<T_uxlen_t, uint32_t, &dmemif_t::load_uword>(addr);
	}
	__always_inline T_uxlen_t load_uhalf(uint64_t addr) {
		return load<T_uxlen_t, uint16_t, &dmemif_t::load_uhalf>(addr);
	}
	__always_inline T_uxlen_t load_ubyte(uint64_t addr) {
		return load<T_uxlen_t, uint8_t, &dmemif_t::load_ubyte>(addr);
	}

	__always_inline void store_double(uint64_t addr, uint64_t value) {
		store<uint64_t, &dmemif_t::store_double>(addr, value);
	}
	__always_inline void store_word(uint64_t addr, uint32_t value) {
		store<uint32_t, &dmemif_t::store_word>(addr, value);
	}
	__always_inline void store_half(uint64_t addr, uint16_t value) {
		store<uint16_t, &dmemif_t::store_half>(addr, value);
	}
	__always_inline void store_byte(uint64_t addr, uint8_t value) {
		store<uint8_t, &dmemif_t::store_byte>(addr, value);
	}
};

/******************************************************************************
 * END: FUNCTIONAL IMPLEMENTATION
 ******************************************************************************/

/******************************************************************************
 * CACHE SELECT
 ******************************************************************************/

template <typename T_sxlen_t, typename T_uxlen_t>
#ifdef LSCACHE_ENABLED
using LSCacheDefault_T = LSCache_T<T_sxlen_t, T_uxlen_t>;
#else
using LSCacheDefault_T = LSCacheDummy_T<T_sxlen_t, T_uxlen_t>;
#endif

/******************************************************************************
 * CACHE SELECT
 ******************************************************************************/

#endif /* RISCV_ISA_LSCACHE_H */
