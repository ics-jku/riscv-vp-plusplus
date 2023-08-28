#pragma once

#include "core/common/mmu.h"
#include "iss.h"

namespace rv32 {

typedef GenericMMU<ISS> MMU;

}  // namespace rv32