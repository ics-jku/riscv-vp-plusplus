.globl contextswitch
.globl coroutine_entry
.globl launch_coroutine


coroutine_entry:
	// move the launch argument to the a0 register (i.e. first argument of the following function)
	mv    a0,s0
	call  launch_coroutine

	// the above call should not return	(i.e the scheduler should not call a finished coroutine)
	ebreak


/*
 * registers a0 and a1 point to the current and next context, respectively
 *
 * 1) store all current registers and pc in the current context
 * 2) load register values from next context
 * 3) jump to pc in the next context
 */
contextswitch:
	// store registers
	sw    x1,4(a0)	    // ra
	sw    x2,8(a0)		// sp
	sw    x8,32(a0)		// s0
	sw    x9,36(a0)		// s1
	sw    x18,72(a0)	// s2
	sw    x19,76(a0)	// s3
	sw    x20,80(a0)	// s4
	sw    x21,84(a0)	// s5
	sw    x22,88(a0)	// s6
	sw    x23,92(a0)	// s7
	sw    x24,96(a0)	// s8
	sw    x25,100(a0)	// s9
	sw    x26,104(a0)	// s10
	sw    x27,108(a0)	// s11

	// store pc
	la    t0,_resume
	sw    t0,0(a0)

	// restore other registers (NOTE: callee saved only + ra)
	lw    x1,4(a1)		// ra
	lw    x2,8(a1)		// sp
	lw    x8,32(a1)		// s0
	lw    x9,36(a1)		// s1
	lw    x18,72(a1)	// s2
	lw    x19,76(a1)	// s3
	lw    x20,80(a1)	// s4
	lw    x21,84(a1)	// s5
	lw    x22,88(a1)	// s6
	lw    x23,92(a1)	// s7
	lw    x24,96(a1)	// s8
	lw    x25,100(a1)	// s9
	lw    x26,104(a1)	// s10
	lw    x27,108(a1)	// s11

	// load new program counter and perform context switch
	lw	  t0,0(a1)
	jr    t0

	_resume:

	ret

