#ifndef RISCV_CHERIV9_ISA64_MEM_H
#define RISCV_CHERIV9_ISA64_MEM_H

#include <stdint.h>

#include "cheri_mem.h"
#include "core/common_cheriv9/mem.h"
#include "iss.h"
#include "mmu.h"

namespace cheriv9::rv64 {
using CombinedMemoryInterface = CombinedMemoryInterface_T<ISS, sxlen_t, uxlen_t>;
using InstrMemoryProxy = InstrMemoryProxy_T<ISS>;
} /* namespace cheriv9::rv64 */

#endif /* RISCV_CHERIV9_ISA64_MEM_IF_H */
