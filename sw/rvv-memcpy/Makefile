OBJECTS  = bootstrap.o main.o

# RV32
CFLAGS   = -march=rv32imafdv_zicsr -mabi=ilp32
VP       = riscv32-vp

# RV64
#CFLAGS   = -march=rv64imafdv_zicsr -mabi=lp64
#VP       = riscv64-vp

LDFLAGS  = -nostartfiles -Wl,--no-relax

VP_FLAGS = --intercept-syscalls --error-on-zero-traphandler=true

include ../Makefile.common
