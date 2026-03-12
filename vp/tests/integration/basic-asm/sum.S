.globl _start
.equ SYSCALL_ADDR, 0x02010000

.macro SYS_EXIT, exit_code
	li   a7, 93
	li   a0, \exit_code
	li   t0, SYSCALL_ADDR
	csrr a6, mhartid
	sw   a6, 0(t0)
.endm

# program entry-point
_start:
	li a0, 50
	li a1, 100
	li a2, 0
loop:
	bgt a0, a1, end
	add a2, a2, a0
	addi a0, a0, 1
	j loop

# call exit (SYS_EXIT=93) with exit code 0 (argument in a0)
end:
	SYS_EXIT 0
