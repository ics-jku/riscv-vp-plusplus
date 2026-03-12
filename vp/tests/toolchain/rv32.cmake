# CMake cross toolchain for RV32 with GCC
# See: https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling-for-linux

set(RISCV_PREFIX $ENV{RISCV_PREFIX})
if(NOT RISCV_PREFIX)
  set(RISCV_PREFIX "riscv32-unknown-elf-")
endif()

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv)

set(CMAKE_C_COMPILER ${RISCV_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${RISCV_PREFIX}g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)

# Set Architecture and ABI flags
set(CMAKE_C_FLAGS_INIT "-misa-spec=2.2 -march=rv32imac -mabi=ilp32")
set(CMAKE_CXX_FLAGS_INIT "-misa-spec=2.2 -march=rv32imac -mabi=ilp32")
set(CMAKE_ASM_FLAGS_INIT "-misa-spec=2.2 -march=rv32imac -mabi=ilp32")
