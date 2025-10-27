#ifndef RISCV_ISA_MEM_IF_H
#define RISCV_ISA_MEM_IF_H

/*
 * TODO: improve/cleanup cheriv9 interface
 */

#include <stdint.h>

#include "core/common_cheriv9/cheri_capability.h"

struct instr_memory_if {
	virtual uint32_t load_instr(uint64_t pc) = 0;

	/* for cheriv9 (optional for others) */
	virtual uint16_t load_instr_half(uint64_t pc) {
		assert(0);
	}

	/* for cheriv9 (optional for others) */
	virtual uint64_t translate_pc(uint64_t pc) {
		assert(0);
	}
};

/*
 * T_sxlen_t .. signed XLEN type (e.g. RV32: int32_t)
 * T_uxlen_t .. unsigned XLEN type (e.g. RV32: uint32_t)
 */
template <typename T_sxlen_t, typename T_uxlen_t>
struct data_memory_if_T {
	/* also used on RV32 for floating point D extension! */
	virtual int64_t load_double(uint64_t addr) = 0;

	virtual T_sxlen_t load_word(uint64_t addr) = 0;
	virtual T_sxlen_t load_half(uint64_t addr) = 0;
	virtual T_sxlen_t load_byte(uint64_t addr) = 0;

	/* unused on RV32 */
	virtual T_uxlen_t load_uword(uint64_t addr) = 0;

	virtual T_uxlen_t load_uhalf(uint64_t addr) = 0;
	virtual T_uxlen_t load_ubyte(uint64_t addr) = 0;

	/* for cheriv9 (optional for others) */
	virtual Capability load_cap(uint64_t addr) {
		assert(0);
	}

	virtual void store_double(uint64_t addr, uint64_t value) = 0;
	virtual void store_word(uint64_t addr, uint32_t value) = 0;
	virtual void store_half(uint64_t addr, uint16_t value) = 0;
	virtual void store_byte(uint64_t addr, uint8_t value) = 0;

	/* for cheriv9 (optional for others) */
	virtual void store_cap(uint64_t addr, Capability value) {
		assert(0);
	}

	virtual T_sxlen_t atomic_load_word(uint64_t addr) = 0;
	virtual void atomic_store_word(uint64_t addr, uint32_t value) = 0;
	virtual T_sxlen_t atomic_load_reserved_word(uint64_t addr) = 0;
	virtual bool atomic_store_conditional_word(uint64_t addr, uint32_t value) = 0;
	virtual void atomic_unlock() = 0;

	/* unused on RV32 */
	virtual int64_t atomic_load_double(uint64_t addr) = 0;
	virtual void atomic_store_double(uint64_t addr, uint64_t value) = 0;
	virtual int64_t atomic_load_reserved_double(uint64_t addr) = 0;
	virtual bool atomic_store_conditional_double(uint64_t addr, uint64_t value) = 0;

	/* returns true if the bus is locked */
	virtual bool is_bus_locked() = 0;
	/*
	 * returns the host page start address, if the last access was using dmi
	 * returns nullptr otherwise
	 * CAUTION: The result is only valid directly after an access, i.e. no SystemC context switch between the access and
	 * a call of this method
	 */
	virtual void *get_last_dmi_page_host_addr() = 0;

	virtual void flush_tlb() = 0;

	/*
	 * cheriv9 helper interfaces
	 * TODO: needs major cleanup
	 */

	virtual void handle_store_data_via_cap(Capability rs2, uint64_t auth_idx, Capability auth_val, uint64_t addr,
	                                       uint8_t width) {
		assert(0);
	}
	virtual Capability handle_load_cap_via_cap(uint64_t auth_idx, Capability auth_val, uint64_t addr) {
		assert(0);
	}
	virtual uint64_t handle_load_data_via_cap(uint64_t auth_idx, Capability auth_val, uint64_t addr, bool is_unsigned,
	                                          uint8_t width) {
		assert(0);
	}
	virtual uint8_t load_tags(uint64_t addr) {
		assert(0);
	}
	virtual void reset(uint64_t start, uint64_t end) {
		assert(0);
	}
	virtual uint64_t atomic_load_reserved_data_via_cap(uint64_t auth_idx, Capability auth_val, uint64_t addr,
	                                                   uint8_t width) {
		assert(0);
	}
	virtual Capability atomic_load_reserved_cap_via_cap(uint64_t auth_idx, Capability auth_val, uint64_t addr) {
		assert(0);
	}
	virtual bool atomic_store_conditional_data_via_cap(Capability rs2, uint64_t auth_idx, Capability auth_val,
	                                                   uint64_t addr, uint8_t width) {
		assert(0);
	}
	virtual void atomic_store_cap(uint64_t addr, Capability value) {
		assert(0);
	}
	virtual Capability atomic_load_cap(uint64_t addr) {
		assert(0);
	}
	virtual T_sxlen_t atomic_load_word_via_cap(uint64_t addr, Capability auth_val, uint64_t auth_idx) {
		assert(0);
	}
	virtual void atomic_store_word_via_cap(uint64_t addr, uint32_t value, Capability auth_val, uint64_t auth_idx) {
		assert(0);
	}
	virtual T_sxlen_t atomic_load_double_via_cap(uint64_t addr, Capability auth_val, uint64_t auth_idx) {
		assert(0);
	}
	virtual void atomic_store_double_via_cap(uint64_t addr, uint64_t value, Capability auth_val, uint64_t auth_idx) {
		assert(0);
	}
};

#endif /* RISCV_ISA_MEM_IF_H */
