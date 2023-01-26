rvArch?=rv32i

OBJECTS  = bootstrap.o main.o uart.o util.o
CFLAGS   = -march=$(rvArch) -mabi=ilp32 -mno-strict-align
LDFLAGS  = -nostartfiles -Wl,--no-relax,--print-memory-usage  -T link.ld

VP       = microrv32-vp
VP_FLAGS =

include ../Makefile.common
