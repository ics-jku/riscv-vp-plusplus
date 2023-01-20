all : main.c
	riscv32-unknown-elf-gcc main.c cor.S bootstrap.S -o main -march=rv32ima -mabi=ilp32 -nostartfiles

sim: all
	riscv-vp --intercept-syscalls main

dump-elf: all
	riscv32-unknown-elf-readelf -a main

dump-code: all
	riscv32-unknown-elf-objdump -D main

clean:
	rm -f main
