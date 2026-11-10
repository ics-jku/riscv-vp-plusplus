#ifndef RISCV_Y_ISA64_MEM_H
#define RISCV_Y_ISA64_MEM_H

#include <stdint.h>

#include "cheri_mem.h"
#include "core/common_y/mem.h"
#include "iss.h"
#include "mmu.h"

namespace rv64y {
using CombinedMemoryInterface = CombinedMemoryInterface_T<ISS, sxlen_t, uxlen_t>;
using InstrMemoryProxy = InstrMemoryProxy_T<ISS>;
} /* namespace rv64y */

#endif /* RISCV_Y_ISA64_MEM_IF_H */
