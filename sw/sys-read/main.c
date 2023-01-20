#include "stdio.h"

int main(int argc, char **argv) {
	printf("Enter a number: ");

	int c,n = 0;
	while (!scanf("%d", &n)) {
		printf("Error: unable to parse number\n");
		while((c = getchar()) != '\n' && c != EOF);
		printf("Enter a number: ");
	}

	printf("Success, you entered %i\n", n);
	return 0;
}
