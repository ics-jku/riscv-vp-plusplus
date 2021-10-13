all : main.c entry.S
	riscv32-unknown-elf-gcc main.c entry.S -o main -march=rv32i -mabi=ilp32 -Wl,-e_reset_vector
	
sim: all
	riscv-vp main
	
dump-elf: all
	riscv32-unknown-elf-readelf -a main
	
dump-code: all
	riscv32-unknown-elf-objdump -D main
	
clean:
	rm -f main
