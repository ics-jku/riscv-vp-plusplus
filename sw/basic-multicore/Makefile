OBJECTS  = main.o bootstrap.o
CFLAGS   = -march=rv32ima_zicsr -mabi=ilp32
LDFLAGS  = -nostartfiles -Wl,--no-relax

VP       = tiny32-mc-vp
VP_FLAGS = --error-on-zero-traphandler=true --intercept-syscalls

include ../Makefile.common
