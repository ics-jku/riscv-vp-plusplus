.global _start

_start:
	li t0, 0
	li t1, 2

twice:
	beq t0, t1, twice_exit
	addi t0, t0, 1
	jal twice # breakpoint is set here, hit once and removed
twice_exit:
	jal exit  # breakpoint is set here

exit:
	# call exit (SYS_EXIT=93) with exit code 0 (argument in a0)
	li a7, 93
	li a0, 0
	ecall
