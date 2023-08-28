#ifndef RISCV_VP_MMU_MEM_IF_H
#define RISCV_VP_MMU_MEM_IF_H

#include <stdint.h>

enum MemoryAccessType { FETCH, LOAD, STORE };

struct mmu_memory_if {
	virtual ~mmu_memory_if() {}

	virtual uint64_t v2p(uint64_t vaddr, MemoryAccessType type) = 0;
	virtual uint64_t mmu_load_pte64(uint64_t addr) = 0;
	virtual uint64_t mmu_load_pte32(uint64_t addr) = 0;
	virtual void mmu_store_pte32(uint64_t addr, uint32_t value) = 0;
};

#endif  // RISCV_VP_MMU_MEM_IF_H
