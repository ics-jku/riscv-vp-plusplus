uart_port = /dev/ttyUSB0
baud_rate = 9600

all : main.c uart.c util.c crc.c bootstrap.S
	riscv32-unknown-elf-gcc -v bootstrap.S main.c uart.c util.c crc.c -o main -march=rv32i -mabi=ilp32 -nostartfiles -Wl,--no-relax -T link.ld

sim: all
	microrv32-vp --intercept-syscalls --uart-port $(uart_port) --uart-baud-rate $(baud_rate) main
	
dump-elf: all
	riscv32-unknown-elf-readelf -a main
	
dump-code: all
	riscv32-unknown-elf-objdump -D main
	
dump-comment: all
	objdump -s --section .comment main

hex: all
	../elf2bin.py ./main crc.hex 0x4020
	
clean:
	rm -f main
