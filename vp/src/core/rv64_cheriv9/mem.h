#ifndef RISCV_ISA64_MEM_H
#define RISCV_ISA64_MEM_H

#include <stdint.h>

#include "core/common/mem.h"
#include "iss.h"
#include "mmu.h"

namespace rv64 {
using CombinedMemoryInterface = CombinedMemoryInterface_T<ISS, sxlen_t, uxlen_t>;
using InstrMemoryProxy = InstrMemoryProxy_T<ISS>;
}  // namespace rv64

#endif /* RISCV_ISA64_MEM_IF_H */
