OBJECTS  = main.o libDisplay.o
CXXFLAGS = -std=c++14 -march=rv32ima -mabi=ilp32

removeSHMEM:
	ipcrm --shmem-key 0x0000053a || true

include ../Makefile.common
override LD = $(CXX)
