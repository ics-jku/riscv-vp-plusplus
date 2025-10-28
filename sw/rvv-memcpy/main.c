#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* naive, non-vectorized c implementation */
void memcpy_c(uint8_t *dest, uint8_t *src, unsigned long len) {
	for (int i = 0; i < len; i++) {
		dest[i] = src[i];
	}
}

/* vectorized implementation */
void memcpy_rvv(uint8_t *dest, uint8_t *src, unsigned long len) {
	unsigned long vl;

	while (len) {
		/* initialize [v0-v7](e32] with values from src (8 bit elements) */

		/* 32bit elements in groups of 8 vregs */
		asm volatile("vsetvli		%0, %1, e8, m8" : "=r"(vl) : "r"(len));

		/* load src [v0-v7] */
		asm volatile("vle8.v		v0, (%0)" : : "r"(src));
		src += vl;

		/* store dest from [v0-v7] */
		asm volatile("vse8.v		v0, (%0)" : : "r"(dest));
		dest += vl;

		len -= vl;
	}
}

/**********************************************************************************************************************
 * util
 **********************************************************************************************************************/

/* print differences of two int32 arrays */
void array_diff(int8_t res1[], int8_t res2[], unsigned long len) {
	unsigned long errcnt = 0;

	printf("Diff:\n");
	for (int i = 0; i < len; i++) {
		if (res1[i] != res2[i]) {
			printf("Error at index %i: %d != %d\n", i, res1[i], res2[i]);
			errcnt++;
		}
	}
	printf("-> %u error(s) in %u entries\n", errcnt, len);
}

/**********************************************************************************************************************
 * main
 **********************************************************************************************************************/

/* length of vectors */
#define LEN 20000

int main() {
	/* vectors */
	int8_t src[LEN], dest1[LEN], dest2[LEN];

	/* initialize src vectors */
	srand(123);
	for (unsigned long i = 0; i < LEN; i++) {
		src[i] = rand();
	}

	/* run non-vectorized memcpy */
	printf("memcpy (naive c) ... ");
	memcpy_c(dest1, src, LEN);
	printf("done\n");

	/* run vectorized memcpy */
	printf("memcpy (vector) ... ");
	memcpy_rvv(dest2, src, LEN);
	printf("done\n");

	/* uncomment to induce error for testing */
	// dest1[LEN / 2] = dest2[LEN / 2] + 1;

	/* check */
	array_diff(dest1, dest2, LEN);

	return 0;
}
