OBJECTS  = main.o bootstrap.o
CFLAGS   = -g3 -march=rv32im -mabi=ilp32
LDFLAGS  = -nostartfiles -Wl,--no-relax
VP_FLAGS = --intercept-syscalls --error-on-zero-traphandler=true --debug-mode

include ../Makefile.common
