#ifndef RISCV_DEBUG
#define RISCV_DEBUG

#include <stdint.h>

#include <unordered_set>
#include <vector>

#include "core_defs.h"

// Capability as seen by a debugger: tag + raw (compressed) capability bits
struct debug_capability {
	bool tag = false;
	__uint128_t bits = 0;
};

struct debug_target_cheri_if;

struct debug_target_if {
	virtual ~debug_target_if() {}

	virtual void enable_debug(void) = 0;
	virtual CoreExecStatus get_status(void) = 0;
	virtual void set_status(CoreExecStatus) = 0;
	virtual void block_on_wfi(bool) = 0;

	virtual void insert_breakpoint(uint64_t) = 0;
	virtual void remove_breakpoint(uint64_t) = 0;

	virtual Architecture get_architecture(void) = 0;
	virtual uint64_t get_hart_id(void) = 0;

	// true if the hart implements the E base ISA (x0-x15 only)
	virtual bool has_E_base_isa(void) = 0;

	virtual uint64_t get_progam_counter(void) = 0;
	virtual uint64_t read_register(unsigned) = 0;
	// Raw (NaN-boxed) value of floating point register f0-f31
	virtual uint64_t read_fp_register(unsigned idx) = 0;
	// Debugger read of a CSR, returns false if the CSR is not available.
	// Must not have side effects on the simulation, so it intentionally does not use the regular CSR read path of the
	// ISS (which e.g. updates the time and cycle CSRs or raises traps). Returns the currently stored value instead.
	virtual bool read_csr_gdb(unsigned addr, uint64_t& out) = 0;
	// Current privilege level (PrivilegeLevel)
	virtual unsigned get_privilege(void) = 0;
	// Debug interface for the RVY (CHERI) extension, nullptr if not implemented by this hart
	virtual debug_target_cheri_if* get_cheri_debug_if(void) {
		return nullptr;
	}

	virtual void run(void) = 0;
	virtual void run_step(void) = 0;

	/* other debug stuff (not needed for gdb) */
	virtual void enable_trace(bool ena) {}
	virtual bool trace_enabled(void) {
		return false;
	}
	virtual void enable_datadmi(bool ena) {}
	virtual bool datadmi_enabled(void) {
		return false;
	}
	virtual void enable_dbbcache(bool ena) {}
	virtual bool dbbcache_enabled(void) {
		return false;
	}
	virtual void enable_lscache(bool ena) {}
	virtual bool lscache_enabled(void) {
		return false;
	}
	virtual void enable_rvfi_dii(bool ena) {}
	virtual bool rvfi_dii_enabled(void) {
		return false;
	}
	virtual void enable_capability_exception_enforcement(bool ena) {}
	virtual bool capability_exception_enforcement_enabled(void) {
		return false;
	}
	virtual void print_stats(void) {};
};

/*
 * Optional debug interface of harts implementing the RVY (CHERI) extension
 * (see debug_target_if::get_cheri_debug_if)
 */
struct debug_target_cheri_if {
	virtual ~debug_target_cheri_if() {}

	// capability register c0-c31
	virtual debug_capability read_capability_register(unsigned idx) = 0;
	virtual debug_capability read_pcc(void) = 0;
	virtual debug_capability read_ddc(void) = 0;
	// Debugger read of a capability CSR (e.g. mtvecc), returns false if not available.
	// Same contract as debug_target_if::read_csr_gdb: no side effects.
	virtual bool read_ycsr_gdb(unsigned addr, debug_capability& out) = 0;
};

#endif
