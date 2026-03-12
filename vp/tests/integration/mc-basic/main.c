int main(unsigned hart_id) {
	/* behave differently based on the core (i.e. hart) id */
	for (unsigned i = 0; i < 100 * hart_id; i++)
		;

	/* copy hart_id + 1 to register $a1 */
	unsigned id = hart_id + 1;
	asm volatile ("addi a1, %0, 0"
	              : /* no output operands */
	              : "r" (id)
	              : "a1");

	return 0;
}
