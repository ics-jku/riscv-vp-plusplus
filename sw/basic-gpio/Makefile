OBJECTS  = bootstrap.o main.o uart.o
CFLAGS   = -g3 -march=rv32i -mabi=ilp32
LDLIBS   = -Tlink.ld -nostartfiles -Wl,--no-relax

VP       = microrv32-vp
VP_FLAGS =

hex: $(EXECUTABLE)
	../elf2bin.py ./main gpio.hex 0x4020

include ../Makefile.common
