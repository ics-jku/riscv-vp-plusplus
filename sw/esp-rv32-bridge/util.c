/*
* utility functions for embedded usage where c-libs are too big
*/

//----------------------------
// integer to ascii (itoa) with util functions 
//----------------------------

// function to swap two numbers
void swap(char *x, char *y) {
    char t = *x; *x = *y; *y = t;
}
 
// function to reverse buffer[i..j]
char* reverse(char *buffer, int i, int j) {
    while (i < j)
        swap(&buffer[i++], &buffer[j--]);
    return buffer;
}
 
// Iterative function to implement itoa() function in C
char* itoa(int value, char* buffer, int base) {
    // invalid input
    if (base < 2 || base > 32)
        return buffer;
    // consider absolute value of number
    int n = (value < 0) ? -value : value;
    int i = 0;
    while (n) {
        int r = n % base;
        if (r >= 10) 
            buffer[i++] = 65 + (r - 10);
        else
            buffer[i++] = 48 + r;
        n = n / base;
    }
 
    // if number is 0
    if (i == 0)
        buffer[i++] = '0';
 
    // If base is 10 and value is negative, the resulting string 
    // is preceded with a minus sign (-)
    // With any other base, value is always considered unsigned
    if (value < 0 && base == 10)
        buffer[i++] = '-';
 
    buffer[i] = '\0'; // null terminate string
 
    // reverse the string and return it
    return reverse(buffer, 0, i - 1);
}

/*
enum {
	US_PER_SEC = 1000000
};

// This is used to quantize a 1MHz value to the closest 32768Hz value
#define DIVIDEND ((uint64_t)15625/(uint64_t)512)

// Bitmask to extract exception code from mcause register
#define MCAUSE_CAUSE 0x7FFFFFFF

// Exception code for timer interrupts
#define MACHINE_TIMER 7

// Set after timer interrupt was received
static bool terminate = false;

static volatile uint64_t *MTIMECMP_REG = (uint64_t *)0x2004000;
static volatile uint64_t *MTIME_REG = (uint64_t *)0x200bff8;

void irq_handler(void) {
	uint32_t mcause;
	uint32_t code;

	mcause = 0;
	__asm__ volatile ("csrr %[ret], mcause"
	                  : [ret] "=r" (mcause));

	code = mcause & MCAUSE_CAUSE;
	if (code != MACHINE_TIMER || terminate)
		__asm__ volatile ("ebreak");

	// Attempt to clear timer interrupt
	*MTIMECMP_REG = UINT64_MAX;

	terminate = true;
	return;
}

void sleep_ms(uint64_t sleep_time){
	uint64_t usec = sleep_time * US_PER_SEC;
	uint64_t ticks = usec / DIVIDEND;

	uint64_t target = *MTIME_REG + ticks;
	*MTIMECMP_REG = target;

	while (!terminate)
		__asm__ volatile ("wfi");
}*/
