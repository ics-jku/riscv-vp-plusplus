OBJECTS  = main.o bootstrap.o
CFLAGS   = -march=rv32ima -mabi=ilp32
LDFLAGS  = -nostartfiles -Wl,--no-relax

VP       = tiny32-mc
VP_FLAGS = --error-on-zero-traphandler=true

include ../Makefile.common
