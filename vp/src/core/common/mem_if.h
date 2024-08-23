#ifndef RISCV_ISA_MEM_IF_H
#define RISCV_ISA_MEM_IF_H

#include <stdint.h>

struct instr_memory_if {
	virtual uint32_t load_instr(uint64_t pc) = 0;
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

	virtual void store_double(uint64_t addr, uint64_t value) = 0;
	virtual void store_word(uint64_t addr, uint32_t value) = 0;
	virtual void store_half(uint64_t addr, uint16_t value) = 0;
	virtual void store_byte(uint64_t addr, uint8_t value) = 0;

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
};

#endif /* RISCV_ISA_MEM_IF_H */
