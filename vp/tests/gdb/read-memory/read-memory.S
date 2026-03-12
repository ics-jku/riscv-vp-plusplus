.global _start

# store value 42 on the stack
_start:
	li t1, 42
	sw t1, 0x0(sp)
	jal exit # breakpoint is set here

# call exit (SYS_EXIT=93) with exit code 0 (argument in a0)
exit:
	li a7, 93
	li a0, 0
	ecall
