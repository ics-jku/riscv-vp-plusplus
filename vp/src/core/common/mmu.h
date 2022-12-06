#pragma once

#include "mmu_mem_if.h"

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

template <typename RVX_ISS>
struct GenericMMU {
    RVX_ISS &core;
    tlm_utils::tlm_quantumkeeper &quantum_keeper;
    sc_core::sc_time clock_cycle = sc_core::sc_time(10, sc_core::SC_NS);
    sc_core::sc_time mmu_access_delay = clock_cycle * 3;

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

    GenericMMU(RVX_ISS &core)
        : core(core), quantum_keeper(core.quantum_keeper) {
        flush_tlb();
    }

    void flush_tlb() {
        memset(&tlb[0], -1, NUM_MODES * NUM_ACCESS_TYPES * TLB_ENTRIES * sizeof(tlb_entry_t));
    }

    uint64_t translate_virtual_to_physical_addr(uint64_t vaddr, MemoryAccessType type) {
        if (core.csrs.satp.fields.mode == SATP_MODE_BARE)
            return vaddr;

        auto mode = core.prv;

        if (type != FETCH) {
            if (core.csrs.mstatus.fields.mprv)
                mode = core.csrs.mstatus.fields.mpp;
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

        uint64_t paddr = walk(vaddr, type, mode);

        // optimization only, to void page walk
        x.ppn = (paddr & ~PGMASK);
        x.vpn = vpn;

        return paddr;
    }

    vm_info decode_vm_info(PrivilegeLevel prv) {
        assert(prv <= SupervisorMode);
        uint64_t ptbase = (uint64_t)core.csrs.satp.fields.ppn << PGSHIFT;
        unsigned mode = core.csrs.satp.fields.mode;
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

    uint64_t walk(uint64_t vaddr, MemoryAccessType type, PrivilegeLevel mode) {
        bool s_mode = mode == SupervisorMode;
        bool sum = core.csrs.mstatus.fields.sum;
        bool mxr = core.csrs.mstatus.fields.mxr;

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

            uint64_t ppn = pte >> PTE_PPN_SHIFT;

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
                raise_trap(EXC_INSTR_PAGE_FAULT, vaddr);
                break;
            case LOAD:
                raise_trap(EXC_LOAD_PAGE_FAULT, vaddr);
                break;
            case STORE:
                raise_trap(EXC_STORE_AMO_PAGE_FAULT, vaddr);
                break;
        }

        throw std::runtime_error("[mmu] unknown access type " + std::to_string(type));
    }
};
