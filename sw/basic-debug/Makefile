# see: https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html for gcc optimization options

all : main.c bootstrap.S
	riscv32-unknown-elf-gcc main.c bootstrap.S -o main -g3 -march=rv32im -mabi=ilp32 -nostartfiles -Wl,--no-relax
	
sim: all
	riscv-vp --intercept-syscalls --error-on-zero-traphandler=true --debug-mode main
	
dump-elf: all
	riscv32-unknown-elf-readelf -a main
	
dump-code: all
	riscv32-unknown-elf-objdump -D main

# see: https://eli.thegreenplace.net/2011/02/07/how-debuggers-work-part-3-debugging-information for information on dwarf files and https://github.com/eliben/pyelftools for python elf/dwarf tools
dump-dwarf-info: all
	riscv32-unknown-elf-objdump --dwarf=info main
	
clean:
	rm -f main
