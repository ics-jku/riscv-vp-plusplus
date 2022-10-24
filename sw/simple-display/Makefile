all : main.cpp libDisplay.cpp
	riscv32-unknown-elf-g++ -g -std=c++14 main.cpp libDisplay.cpp -o main -march=rv32ima -mabi=ilp32

sim: all
	riscv-vp --intercept-syscalls --error-on-zero-traphandler=true main
	
dump-elf: all
	riscv32-unknown-elf-readelf -a main
	
dump-code: all
	riscv32-unknown-elf-objdump -D main

removeSHMEM:
	ipcrm --shmem-key 0x0000053a || true

clean: removeSHMEM
	rm -f main
