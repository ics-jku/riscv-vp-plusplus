all : main.c
	riscv32-unknown-elf-gcc main.c --coverage -o main -march=rv32ima -mabi=ilp32

sim: main.gcda

main.gcda : all
	riscv-vp --intercept-syscalls --error-on-zero-traphandler=true main

# use -b option to display branch coverage too
dump-coverage: main.gcda
	riscv32-unknown-elf-gcov main.c

dump-html-coverage: main.gcda
	gcovr --gcov-executable riscv32-unknown-elf-gcov -b -r . --html-details -o cov.html

dump-elf: all
	riscv32-unknown-elf-readelf -a main

dump-code: all
	riscv32-unknown-elf-objdump -D main

clean:
	rm -f main main.c.gcov main.gcda main.gcno cov*.html
