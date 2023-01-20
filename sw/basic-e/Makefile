all : sum.o
	riscv32-unknown-elf-ld sum.o -o main

sim: all
	riscv-vp --use-E-base-isa --error-on-zero-traphandler=true main

sum.o : sum.S
	riscv32-unknown-elf-gcc -c sum.S -o sum.o -march=rv32e -mabi=ilp32

dump-elf: all
	riscv32-unknown-elf-readelf -a main

dump-code: all
	riscv32-unknown-elf-objdump -D main

clean:
	rm -f main sum.o
