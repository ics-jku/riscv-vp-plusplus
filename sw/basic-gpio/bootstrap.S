.globl _start
.globl main

.equ SYSCALL_ADDR, 0x02010000

.macro INIT_HW_PLATFORM
and x1,x0,x0
and x2,x0,x0
and x3,x0,x0
and x4,x0,x0
and x5,x0,x0
and x6,x0,x0
and x7,x0,x0
and x8,x0,x0
and x9,x0,x0
and x10,x0,x0
and x11,x0,x0
and x12,x0,x0
and x13,x0,x0
and x14,x0,x0
and x15,x0,x0
and x16,x0,x0
and x17,x0,x0
and x18,x0,x0
and x19,x0,x0
and x20,x0,x0
and x21,x0,x0
and x22,x0,x0
and x23,x0,x0
and x24,x0,x0
and x25,x0,x0
and x26,x0,x0
and x27,x0,x0
and x28,x0,x0
and x29,x0,x0
and x30,x0,x0
and x31,x0,x0
.endm

.macro SYS_EXIT, exit_code
li   a7, 93
li   a0, \exit_code
li   t0, SYSCALL_ADDR
csrr a6, mhartid
sw   a6, 0(t0)
.endm

# .macro SYS_EXIT, exit_code
# li   a7, 93
# li   a0, \exit_code
# li   t0, SYSCALL_ADDR
# sw   a7, 0(t0)
# .endm

_start:
# initialize hw-platform
INIT_HW_PLATFORM
la sp, stack_end
jal main

# call exit (SYS_EXIT=93) with exit code 0 (argument in a0)
SYS_EXIT 0

.align 4
stack_begin:
.zero 1024
stack_end:
