.globl _start

# call exit (SYS_EXIT=93) with exit code 0 (argument in a0)
_start:
	li a7, 93
	li a0, 0
	ecall
