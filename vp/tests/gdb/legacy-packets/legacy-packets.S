.global _start

_start:
	li t0, 42
	jal exit # breakpoint is set here

exit:
	# call exit (SYS_EXIT=93) with exit code 0 (argument in a0)
	li a7, 93
	li a0, 0
	ecall
