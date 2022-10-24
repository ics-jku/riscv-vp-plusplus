rvArch?=rv32i

all : main.c bootstrap.S
	riscv32-unknown-elf-gcc bootstrap.S main.c uart.c util.c -o main -march=$(rvArch) -mabi=ilp32 -mno-strict-align -nostartfiles -Wl,--no-relax,--print-memory-usage  -T link.ld

sim: all
	microrv32-vp --intercept-syscalls main

dump-elf: all
	riscv32-unknown-elf-readelf -a main

dump-code: all
	riscv32-unknown-elf-objdump -D -Mnumeric -Mno-aliases main

dump-comment: all
	objdump -s --section .comment main

clean:
	rm -f main
	rm -f *.hex
	rm -f *.log
	rm -f *.dump
