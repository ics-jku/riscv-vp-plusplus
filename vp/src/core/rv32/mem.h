#ifndef RISCV_ISA32_MEM_H
#define RISCV_ISA32_MEM_H

#include <stdint.h>

#include "core/common/mem.h"
#include "iss.h"
#include "mmu.h"

namespace rv32 {
using CombinedMemoryInterface = CombinedMemoryInterface_T<ISS, sxlen_t, uxlen_t>;
using InstrMemoryProxy = InstrMemoryProxy_T<ISS>;
}  // namespace rv32

#endif /* RISCV_ISA32_MEM_IF_H */
