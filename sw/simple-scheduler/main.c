#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

#define REG_SP 1
#define REG_S0 7

typedef void (*entrypoint_t)(void *);

enum state_t {
	UNKNOWN,
	START,
	ACTIVE,
	FINISHED
};

typedef struct {
	uint32_t pc;
	uint32_t regs[31];
} context_t;

typedef struct {
	uint8_t stack[16384];
	context_t ctx;
	entrypoint_t entry;
	void *arg;
	enum state_t stat;
} coroutine_t;


context_t main_context;
context_t *active_context;

extern void contextswitch(context_t *current, context_t *next);

extern void coroutine_entry();


void switch_to_scheduler() {
	contextswitch(active_context, &main_context);
}

void switch_to_coroutine(coroutine_t *cor) {
	active_context = &cor->ctx;
	contextswitch(&main_context, &cor->ctx);
}


void launch_coroutine(coroutine_t *cor) {
	cor->stat = ACTIVE;
	cor->entry(cor->arg);
	cor->stat = FINISHED;
	assert (active_context == &cor->ctx);
	switch_to_scheduler();
}


void function_A(void *arg) {
	printf("A fn: 1\n");
	switch_to_scheduler();
	printf("A fn: 2\n");
}

void function_B(void *arg) {
	printf("B fn: 1\n");
	switch_to_scheduler();
	printf("B fn: 2\n");
	switch_to_scheduler();
	printf("B fn: 3\n");
}


coroutine_t *create_coroutine(entrypoint_t fn, void *arg) {
	coroutine_t *cor = (coroutine_t *)malloc(sizeof(coroutine_t));

	for (int i=0; i<31; ++i)
		cor->ctx.regs[i] = 0;

	cor->ctx.regs[REG_SP] = (uint32_t)(&cor->stack[16384]);
	cor->ctx.regs[REG_S0] = (uint32_t)cor;

	cor->entry = fn;
	cor->arg = arg;
	cor->stat = START;

	cor->ctx.pc = (uint32_t)coroutine_entry;

	return cor;
}

void free_coroutine(coroutine_t *cor) {
	free(cor);
}

int main() {
	coroutine_t *A_cor = create_coroutine(function_A, 0);
	coroutine_t *B_cor = create_coroutine(function_B, 0);

	_Bool done = 0;
	while (!done) {
		done = 1;

		if (A_cor->stat != FINISHED) {
			switch_to_coroutine(A_cor);
			done = 0;
		}

		if (B_cor->stat != FINISHED) {
			switch_to_coroutine(B_cor);
			done = 0;
		}
	}

	free_coroutine(A_cor);
	free_coroutine(B_cor);
	return 0;
}
