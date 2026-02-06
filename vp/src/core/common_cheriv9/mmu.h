#pragma once

#include <stdint.h>
#include <tlm_utils/tlm_quantumkeeper.h>

#include <systemc>

#include "core/common/irq_if.h"
#include "core/common/mmu_mem_if.h"
#include "util/propertytree.h"

namespace cheriv9 {
constexpr unsigned PTE_PPN_SHIFT = 10;
constexpr unsigned PGSHIFT = 12;
constexpr unsigned PGSIZE = 1 << PGSHIFT;
constexpr unsigned PGMASK = PGSIZE - 1;

constexpr unsigned PTE_V = 1;
constexpr unsigned PTE_R = 1 << 1;
constexpr unsigned PTE_W = 1 << 2;
constexpr unsigned PTE_X = 1 << 3;
constexpr unsigned PTE_U = 1 << 4;
constexpr unsigned PTE_G = 1 << 5;
constexpr unsigned PTE_A = 1 << 6;
constexpr unsigned PTE_D = 1 << 7;
constexpr unsigned PTE_RSW = 0b11 << 8;
// CHERI Extension
constexpr uint64_t CHERI_PERM_MASK = 0x7FFFFFFFFFFFFFF;  // Upmost 5 bit are reserverd for CHERI
constexpr uint64_t PTE_CW = 1ULL << 63;
constexpr uint64_t PTE_CR = 1ULL << 62;
constexpr uint64_t PTE_CD = 1ULL << 61;
constexpr uint64_t PTE_CRM = 1ULL << 60;
constexpr uint64_t PTE_CRG = 1ULL << 59;

enum cheri_load_perms {
	CAP_LOAD_STRIP_TAGS = 0b000,
	CAP_LOAD_FAULTS = 0b010,
	CAP_LOAD_UNALTERED = 0b100,
	// All others are reserved for future use
};

struct pte_t {
	uint64_t value;

	bool V() {
		return value & PTE_V;
	}
	bool R() {
		return value & PTE_R;
	}
	bool W() {
		return value & PTE_W;
	}
	bool X() {
		return value & PTE_X;
	}
	bool U() {
		return value & PTE_U;
	}
	bool G() {
		return value & PTE_G;
	}
	bool A() {
		return value & PTE_A;
	}
	bool D() {
		return value & PTE_D;
	}

	bool CW() {
		return value & PTE_CW;
	}

	bool CR() {
		return value & PTE_CR;
	}

	bool CD() {
		return value & PTE_CD;
	}

	bool CRM() {
		return value & PTE_CRM;
	}

	bool CRG() {
		return value & PTE_CRG;
	}

	cheri_load_perms CHERI_LOAD_PERMS() {
		return static_cast<cheri_load_perms>(CR() << 2 | CRM() << 1 | CRG());
	}

	operator uint64_t() {
		return value;
	}
};

struct vm_info {
	int levels;
	int idxbits;
	int ptesize;
	uint64_t ptbase;
};

template <typename T_RVX_ISS>
struct MMU_T {
	/* config properties */
	sc_core::sc_time prop_clock_cycle_period = sc_core::sc_time(10, sc_core::SC_NS);
	unsigned int prop_mmu_access_clock_cycles = 3;

	T_RVX_ISS &core;
	tlm_utils::tlm_quantumkeeper &quantum_keeper;
	sc_core::sc_time mmu_access_delay;

	mmu_memory_if *mem = nullptr;
	bool page_fault_on_AD = false;

	struct tlb_entry_t {
		uint64_t ppn = -1;
		uint64_t vpn = -1;
	};

	static constexpr unsigned TLB_ENTRIES = 256;
	static constexpr unsigned NUM_MODES = 2;         // User and Supervisor
	static constexpr unsigned NUM_ACCESS_TYPES = 3;  // FETCH, LOAD, STORE

	tlb_entry_t tlb[NUM_MODES][NUM_ACCESS_TYPES][TLB_ENTRIES];

	MMU_T(T_RVX_ISS &core) : core(core), quantum_keeper(core.quantum_keeper) {
		/*
		 * get config properties from global property tree (or use default)
		 * Note: Instance has no name -> use the owners name is used as instance identifier
		 */
		VPPP_PROPERTY_GET("MMU." + core.name(), "clock_cycle_period", sc_core::sc_time, prop_clock_cycle_period);
		VPPP_PROPERTY_GET("MMU." + core.name(), "mmu_access_clock_cycles", uint64_t, prop_mmu_access_clock_cycles);

		mmu_access_delay = prop_clock_cycle_period * prop_mmu_access_clock_cycles;

		flush_tlb();
	}

	void flush_tlb() {
		memset(&tlb[0], -1, NUM_MODES * NUM_ACCESS_TYPES * TLB_ENTRIES * sizeof(tlb_entry_t));
	}

	uint64_t translate_virtual_to_physical_addr(uint64_t vaddr, MemoryAccessType type) {
		bool trap_if_cap = false;
		bool strip_tag = false;
		return translate_virtual_to_physical_addr(vaddr, type, false, &trap_if_cap, &strip_tag);
	}

	uint64_t translate_virtual_to_physical_addr(uint64_t vaddr, MemoryAccessType type, bool tag, bool *trap_if_cap,
	                                            bool *strip_tag) {
		if (core.csrs.satp.reg.fields.mode == SATP_MODE_BARE)
			return vaddr;

		auto mode = core.prv;

		if (type != FETCH) {
			if (core.csrs.mstatus.reg.fields.mprv)
				mode = core.csrs.mstatus.reg.fields.mpp;
		}

		if (mode == MachineMode)
			return vaddr;

		// optional timing
		quantum_keeper.inc(mmu_access_delay);

		// optimization only, to void page walk
		assert(mode == 0 || mode == 1);
		assert(type == 0 || type == 1 || type == 2);
		auto vpn = (vaddr >> PGSHIFT);
		auto idx = vpn % TLB_ENTRIES;
		auto &x = tlb[mode][type][idx];
		if (x.vpn == vpn)
			return x.ppn | (vaddr & PGMASK);

		uint64_t paddr = walk(vaddr, type, mode, tag, trap_if_cap, strip_tag);

		// optimization only, to void page walk
		x.ppn = (paddr & ~((uint64_t)PGMASK));
		x.vpn = vpn;

		return paddr;
	}

	vm_info decode_vm_info(PrivilegeLevel prv) {
		assert(prv <= SupervisorMode);
		uint64_t ptbase = (uint64_t)core.csrs.satp.reg.fields.ppn << PGSHIFT;
		unsigned mode = core.csrs.satp.reg.fields.mode;
		switch (mode) {
			case SATP_MODE_SV32:
				return {2, 10, 4, ptbase};
			case SATP_MODE_SV39:
				return {3, 9, 8, ptbase};
			case SATP_MODE_SV48:
				return {4, 9, 8, ptbase};
			case SATP_MODE_SV57:
				return {5, 9, 8, ptbase};
			case SATP_MODE_SV64:
				return {6, 9, 8, ptbase};
			default:
				throw std::runtime_error("unknown Sv (satp) mode " + std::to_string(mode));
		}
	}

	bool check_vaddr_extension(uint64_t vaddr, const vm_info &vm) {
		int highbit = vm.idxbits * vm.levels + PGSHIFT - 1;
		assert(highbit > 0);
		uint64_t ext_mask = (uint64_t(1) << (core.xlen - highbit)) - 1;
		uint64_t bits = (vaddr >> highbit) & ext_mask;
		bool ok = (bits == 0) || (bits == ext_mask);
		return ok;
	}

	uint64_t walk(uint64_t vaddr, MemoryAccessType type, PrivilegeLevel mode, bool tag, bool *trap_if_cap,
	              bool *strip_tag) {
		bool s_mode = mode == SupervisorMode;
		bool sum = core.csrs.mstatus.reg.fields.sum;
		bool mxr = core.csrs.mstatus.reg.fields.mxr;

		vm_info vm = decode_vm_info(mode);

		if (!check_vaddr_extension(vaddr, vm))
			vm.levels = 0;  // skip loop and raise page fault

		uint64_t base = vm.ptbase;
		for (int i = vm.levels - 1; i >= 0; --i) {
			// obtain VPN field for current level, NOTE: all VPN fields have the same length for each separate VM
			// implementation
			int ptshift = i * vm.idxbits;
			unsigned vpn_field = (vaddr >> (PGSHIFT + ptshift)) & ((1 << vm.idxbits) - 1);

			auto pte_paddr = base + vpn_field * vm.ptesize;
			// TODO: PMP checks for pte_paddr with (LOAD, PRV_S)

			assert(vm.ptesize == 4 || vm.ptesize == 8);
			assert(mem);
			pte_t pte;
			if (vm.ptesize == 4)
				pte.value = mem->mmu_load_pte32(pte_paddr);
			else
				pte.value = mem->mmu_load_pte64(pte_paddr);

			uint64_t ppn = (pte & CHERI_PERM_MASK) >> PTE_PPN_SHIFT;

			if (!pte.V() || (!pte.R() && pte.W())) {
				// std::cout << "[mmu] !pte.V() || (!pte.R() && pte.W())" << std::endl;
				break;
			}

			if (!pte.R() && !pte.X()) {
				base = ppn << PGSHIFT;
				continue;
			}

			assert(type == FETCH || type == LOAD || type == STORE);
			if ((type == FETCH) && !pte.X()) {
				// std::cout << "[mmu] (type == FETCH) && !pte.X()" << std::endl;
				break;
			}
			if ((type == LOAD) && !pte.R() && !(mxr && pte.X())) {
				// std::cout << "[mmu] (type == LOAD) && !pte.R() && !(mxr && pte.X())" << std::endl;
				break;
			}
			if ((type == STORE) && !(pte.R() && pte.W())) {
				// std::cout << "[mmu] (type == STORE) && !(pte.R() && pte.W())" << std::endl;
				break;
			}

			if (pte.U()) {
				if (s_mode && ((type == FETCH) || !sum))
					break;
			} else {
				if (!s_mode)
					break;
			}
			// CHERI Extension
			if (tag && type == STORE) {
				if (!pte.CW()) {
					handle_cheri_exception(EXC_CHERI_STORE_FAULT, vaddr, &core.rvfi_dii_output);
				}
				if (!pte.CD()) {
					if (page_fault_on_AD) {
						handle_cheri_exception(EXC_CHERI_STORE_FAULT, vaddr, &core.rvfi_dii_output);
					} else {
						// TODO: PMP checks for pte_paddr with (STORE, PRV_S)
						// NOTE: the store has to be atomic with the above load of the PTE, i.e. lock the bus if
						// required NOTE: only need to update A / D flags, hence it is enough to store 32 bit (8 bit
						// might be enough too)
						mem->mmu_store_pte32(pte_paddr, pte | PTE_CD | PTE_D);
					}
				}
			}
			if (type == LOAD) {
				switch (pte.CHERI_LOAD_PERMS()) {
					case cheri_load_perms::CAP_LOAD_STRIP_TAGS:
						// Tag of the loaded capability must be stripped
						// This can not be done here, as the loaded capability is not known at this point
						// Instead the flag strip_tag is set to true, which must be checked by the caller
						*strip_tag = true;
						break;
					case cheri_load_perms::CAP_LOAD_UNALTERED:
						// Nothing to do, just load data afterwards
						*strip_tag = false;
						*trap_if_cap = false;
						break;
					case cheri_load_perms::CAP_LOAD_FAULTS:
						// Trap is dependent on tag bit
						// As tag is not known at this point, instead the flag trap_if_cap is set to true
						// This must be checked by the caller
						*trap_if_cap = true;
						break;
					default:
						// Trap is dependent on tag bit
						// As tag is not known at this point, instead the flag trap_if_cap is set to true
						// This must be checked by the caller
						//*trap_if_cap = true;
						// TODO: This occured in userland init
						// Although according to spec, this should raise a trap...
						break;
				}
			}
			// NOTE: all PPN (except the highest one) have the same bitwidth as the VPNs, hence ptshift can be used
			if ((ppn & ((uint64_t(1) << ptshift) - 1)) != 0)
				break;  // misaligned superpage

			uint64_t ad = PTE_A | ((type == STORE) * PTE_D);
			if ((pte & ad) != ad) {
				if (page_fault_on_AD) {
					break;  // let SW deal with this
				} else {
					// TODO: PMP checks for pte_paddr with (STORE, PRV_S)

					// NOTE: the store has to be atomic with the above load of the PTE, i.e. lock the bus if required
					// NOTE: only need to update A / D flags, hence it is enough to store 32 bit (8 bit might be enough
					// too)
					mem->mmu_store_pte32(pte_paddr, pte | ad);
				}
			}

			// translation successful, return physical address
			uint64_t mask = ((uint64_t(1) << ptshift) - 1);
			uint64_t vpn = vaddr >> PGSHIFT;
			uint64_t pgoff = vaddr & (PGSIZE - 1);
			uint64_t paddr = (((ppn & ~mask) | (vpn & mask)) << PGSHIFT) | pgoff;

			return paddr;
		}

		switch (type) {
			case FETCH:
				raise_trap(EXC_INSTR_PAGE_FAULT, vaddr, &core.rvfi_dii_output);
				break;
			case LOAD:
				raise_trap(EXC_LOAD_PAGE_FAULT, vaddr, &core.rvfi_dii_output);
				break;
			case STORE:
				raise_trap(EXC_STORE_AMO_PAGE_FAULT, vaddr, &core.rvfi_dii_output);
				break;
		}

		throw std::runtime_error("[mmu] unknown access type " + std::to_string(type));
	}
};
} /* namespace cheriv9 */
