OBJECTS  = main.o entry.o
CFLAGS   = -march=rv32i_zicsr -mabi=ilp32
LDFLAGS  = -Wl,-e_reset_vector
VP_FLAGS = --error-on-zero-traphandler=true

include ../Makefile.common
