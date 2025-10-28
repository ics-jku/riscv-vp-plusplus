.globl _start
.globl main

_start:

# enable float and vector
li t1, 0x6600
csrs mstatus, t1

# jump to main
jal main

# call exit (SYS_EXIT=93) with exit code 0 (argument in a0)
li a7,93
li a0,0
ecall
