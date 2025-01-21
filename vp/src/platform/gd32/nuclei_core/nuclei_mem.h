#ifndef RISCV_ISA32_NUCLEI_MEM_H
#define RISCV_ISA32_NUCLEI_MEM_H

#include <stdint.h>

#include "core/common/mem.h"
#include "nuclei_iss.h"

namespace rv32 {
using CombinedMemoryInterface = CombinedMemoryInterface_T<NUCLEI_ISS, sxlen_t, uxlen_t>;
using InstrMemoryProxy = InstrMemoryProxy_T<NUCLEI_ISS>;
}  // namespace rv32

#endif /* RISCV_ISA32_NUCLEI_MEM_IF_H */
