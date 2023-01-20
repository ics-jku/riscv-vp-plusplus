In the first terminal run our VP in debug mode (wraps the core iss in a debug stub):

	riscv-vp --debug-mode --intercept-syscalls main


In the other terminal run:

	riscv32-unknown-elf-gdb

Inside of gdb then (for example):

	file main
	target remote :5005
	b main.c:14
	continue
	p x
	p y
	step
	continue

This will load the symbols from the *main* elf file (note: this one should match the one loaded in our VP). The *target remote* command is passed host and port number. In this case localhost is used.

Note: you can use 'set debug remote 1' in gdb to observe the packets send and received by gdb (might be useful for debugging).
