#include "stdio.h"

int main() {
	int i = 0;

	while (i < 10) {
		if (i < 5) {
			printf("i < 5\n");
		}

		if (i % 2 == 0) {
			printf("i mod 2 == 0\n");
		}

		if (i == 10) {
			printf("i == 10\n");
		}

		++i;
	}

	return 0;
}
