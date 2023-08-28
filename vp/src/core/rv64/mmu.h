#pragma once

#include "core/common/mmu.h"
#include "iss.h"

namespace rv64 {

typedef GenericMMU<ISS> MMU;

}  // namespace rv64