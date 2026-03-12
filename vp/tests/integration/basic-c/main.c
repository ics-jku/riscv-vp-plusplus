int sum(int);

static int x = 5;

int main(void) {
	int result = sum(x);

	/* copy result to register $a1 */
	asm volatile ("addi a1, %0, 0"
	              : /* no output operands */
	              : "r" (result)
	              : "a1");

	return 0;
}
