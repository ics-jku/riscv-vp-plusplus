OBJECTS  = main.o sum.o bootstrap.o
CFLAGS   = -march=rv32i -mabi=ilp32
LDFLAGS  = -nostartfiles -Wl,--no-relax
VP_FLAGS = --intercept-syscalls --error-on-zero-traphandler=true

include ../Makefile.common
