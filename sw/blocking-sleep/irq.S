.globl register_handler
.globl lvl0_handler

.align 4
lvl0_handler:
    # Store all register values on the stack
    addi sp, sp, -4 * 32
    sw x1, 0x0(sp)
    sw x4, 3 * 4(sp)
    sw x5, 4 * 4(sp)
    sw x6, 5 * 4(sp)
    sw x7, 6 * 4(sp)
    sw x10, 9 * 4(sp)
    sw x11, 10 * 4(sp)
    sw x12, 11 * 4(sp)
    sw x13, 12 * 4(sp)
    sw x14, 13 * 4(sp)
    sw x15, 14 * 4(sp)
    sw x16, 15 * 4(sp)
    sw x17, 16 * 4(sp)
    sw x28, 27 * 4(sp)
    sw x29, 28 * 4(sp)
    sw x30, 29 * 4(sp)
    sw x31, 30 * 4(sp)

    jal irq_handler

    # Load all register values from the stack and return
    lw x1, 0x0(sp)
    lw x4, 3 * 4(sp)
    lw x5, 4 * 4(sp)
    lw x6, 5 * 4(sp)
    lw x7, 6 * 4(sp)
    lw x10, 9 * 4(sp)
    lw x11, 10 * 4(sp)
    lw x12, 11 * 4(sp)
    lw x13, 12 * 4(sp)
    lw x14, 13 * 4(sp)
    lw x15, 14 * 4(sp)
    lw x16, 15 * 4(sp)
    lw x17, 16 * 4(sp)
    lw x28, 27 * 4(sp)
    lw x29, 28 * 4(sp)
    lw x30, 29 * 4(sp)
    lw x31, 30 * 4(sp)
    addi sp, sp, 4 * 32
    mret

register_handler:
    # Use lvl0_handler as the trap handler
    la t0, lvl0_handler
    csrw mtvec, t0

    # Enable machine external interrupts (MIE bit)
    li t1, 0x888
    csrw mie, t1

    # Globally enable machine mode interrupts (MIE bit)
    li t1, 8
    csrw mstatus, t1

    # Return
    ret
