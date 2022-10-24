all : main.c
	riscv32-unknown-elf-gcc main.c -o main -march=rv32ima -mabi=ilp32
	
sim: all
	riscv-vp --intercept-syscalls --error-on-zero-traphandler=true main
	
dump-elf: all
	riscv32-unknown-elf-readelf -a main
	
dump-code: all
	riscv32-unknown-elf-objdump -D main
	
clean:
	rm -f main main2
