OBJECTS  = main.o bootstrap.o
CFLAGS   = --target=riscv64-unknown-elf -march=rv64ixcheri -mabi=l64pc128 -nostdlib
LDFLAGS  = -Wl,--no-relax -T link.ld

VP = tiny64-cheriv9-vp

RISCV_PREFIX = riscv64-unknown-freebsd-

include ../Makefile.common

# enable purecap
VP_FLAGS += --cheri-purecap

# enable trace mode
VP_FLAGS += --trace-mode

override CC = $(RISCV_PREFIX)clang
override LD = $(RISCV_PREFIX)clang
