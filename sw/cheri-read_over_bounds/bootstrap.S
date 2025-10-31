.section .text		# Ensure the code goes in the correct section
.globl _start		# Make _start visible to the linker
.p2align 6			# Align to 64 bytes (2^6 = 64)

_start:
	# initialize stack pointer capability (see link.ld)
	cllc	csp, _stack_top

	# call main
	cllc	ct0, main
	cjalr	ct0

exit:
	# Call exit (SYS_EXIT=93) with exit code 0 (argument in ca0)
	li		a7, 93		# System call number for exit
	li		a0, 0		# Set exit code (0) in ca0 (a0 in CHERI capability mode)
	ecall				# Make the system call
