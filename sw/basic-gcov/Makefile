OBJECTS  = main.o
CFLAGS   = --coverage -march=rv32ima -mabi=ilp32
LDFLAGS  = --coverage
VP_FLAGS = --error-on-zero-traphandler=true

CLEAN_EXTRA = main.c.gcov main.gcda main.gcno cov*.html

main.gcda: main
	riscv-vp --intercept-syscalls --error-on-zero-traphandler=true main

# use -b option to display branch coverage too
dump-coverage: main.gcda
	$(RISCV_PREFIX)gcov main.c

dump-html-coverage: main.gcda
	gcovr --gcov-executable $(RISCV_PREFIX)gcov -b -r . --html-details -o cov.html

include ../Makefile.common
.DEFAULT_GOAL := main.gcda
