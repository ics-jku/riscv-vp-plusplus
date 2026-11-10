OBJECTS  = main.o bootstrap.o
CFLAGS   = --target=riscv64-unknown-elf \
           -march=rv64ymafdczyhybrid_zylevels1b -mabi=l64pc128d \
           -ffreestanding -mno-relax -g

LDFLAGS  = -nostdlib -fuse-ld=lld -Wl,--no-relax -T link.ld

VP = tiny64y-vp

include ../Makefile.common

# enable purecap
VP_FLAGS += --cheri-purecap

override CC = $(RISCV_PREFIX)clang
override LD = $(RISCV_PREFIX)clang
