.globl _start
.globl main

.equ SYSCALL_ADDR, 0x02010000

# NOTE: this will exit the whole simulation, i.e. stop all harts
.macro SYS_EXIT, exit_code
li   a7, 93
li   a0, \exit_code
li   t0, SYSCALL_ADDR
csrr a6, mhartid
sw   a6, 0(t0)
.endm

# NOTE: each core will start here with execution
_start:

# initialize global pointer (see crt0.S of the RISC-V newlib C-library port)
.option push
.option norelax
1:auipc gp, %pcrel_hi(__global_pointer$)
  addi  gp, gp, %pcrel_lo(1b)
.option pop

csrr a0, mhartid   # return a core specific number 0 or 1
li t0, 0
beq a0, t0, core0
li t0, 1
beq a0, t0, core1
core0:
la sp, stack0_end  # code executed only by core0
j end
core1:
la sp, stack1_end  # code executed only by core1
j end
end:

jal main

# wait until all two cores have finished
la t0, exit_counter
li t1, 1
li t2, 1
amoadd.w a0, t1, 0(t0)
1:
blt a0, t2, 1b

# call exit (SYS_EXIT=93) with exit code 0 (argument in a0)
SYS_EXIT 0

.align 8
stack0_begin:
.zero 32768
stack0_end:

.align 8
stack1_begin:
.zero 32768
stack1_end:

.align 8
exit_counter:
.word 0
