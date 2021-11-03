override CC = riscv32-unknown-elf-gcc

CFLAGS  = -march=rv32i -mabi=ilp32
ASFLAGS = $(CFLAGS)
LDFLAGS = -nostartfiles -Wl,--no-relax

all: main
main: main.o bootstrap.o
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $^
sim: main
	tiny32-vp --intercept-syscalls $<

.PHONY: all sim
