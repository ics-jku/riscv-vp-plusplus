.globl _start

# jump to hart specific code based on current hart
_start:
	csrr t0, mhartid    # core number in $t0
	beqz t0, core0      # dispatch for hart0
	bnez t0, core1      # dispatch for hart1

# call exit (SYS_EXIT=93) with exit code 0 (argument in a0)
exit:
	li a7, 93
	li a0, 0
	ecall

core0:
	wfi
	j core0

core1:
	li a0, 42
	jal exit            # breakpoint is set here
