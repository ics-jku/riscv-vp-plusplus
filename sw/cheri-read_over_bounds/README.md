# CHERI "Read over Bounds" example

This simple program shows how CHERI limits the range of addresses that can be dereferenced.
Reading a value outside this range results in a "CHERI exception: LengthViolation."


You must have a CHERI RVY toolchain in your executable search path!
Make sure to point towards that toolchain when using the Makefile
e.g.:
RISCV_PREFIX=~/cheri/llvm-099/bin make