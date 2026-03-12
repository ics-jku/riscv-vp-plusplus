#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
	int n = printf("ABCDEFX %s\n", "Done");
	int e = errno;
	puts(strerror(e));
	
	puts ("Hello World!");
	puts ("  Hello World!");
	puts (" X A ");
	putchar('a');
	putchar('b');
	putchar('c');
	putchar('\n');
	printf("  X Test\n");
	printf("%d\n", 12);
	puts ("Hello World!");

	return 0;
}
