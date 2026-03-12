.global _start

# write register index to all registers
_start:
	li x1, 1
	li x2, 2
	li x3, 3
	li x4, 4
	li x5, 5
	li x6, 6
	li x7, 7
	li x8, 8
	li x9, 9
	li x10, 10
	li x11, 11
	li x12, 12
	li x13, 13
	li x14, 14
	li x15, 15
	li x16, 16
	li x17, 17
	li x18, 18
	li x19, 19
	li x20, 20
	li x21, 21
	li x22, 22
	li x23, 23
	li x24, 24
	li x25, 25
	li x26, 26
	li x27, 27
	li x28, 28
	li x29, 29
	li x30, 30
	li x31, 31
	jal exit # breakpoint is set here

# call exit (SYS_EXIT=93) with exit code 0 (argument in a0)
exit:
	li a7, 93
	li a0, 0
	ecall
