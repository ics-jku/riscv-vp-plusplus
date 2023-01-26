OBJECTS  = main.o uart.o bootstrap.o
CFLAGS   = -march=rv32i -mabi=ilp32
LDFLAGS  = -nostartfiles -Wl,--no-relax -T link.ld

VP       = microrv32-vp
VP_FLAGS =

hex: all
	../elf2bin.py ./main crc.hex 0x4020

include ../Makefile.common
