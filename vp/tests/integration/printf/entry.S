.globl _start
.globl _reset_vector
.equ SYSCALL_ADDR, 0x02010000

trap_handler:
	li   t0, SYSCALL_ADDR
	csrr t1, mhartid
	sw   t1, 0(t0)
	csrr t0, mepc
	addi t0, t0, 4
	csrw mepc, t0
	mret

_reset_vector:
	la t0, trap_handler
	csrw mtvec, t0
	j _start
