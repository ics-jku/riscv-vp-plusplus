#define gdbf_fold(I, TYPE, FUNC)                               \
	static mpc_val_t *gdbf_packet_##I(int n, mpc_val_t **xs) { \
		int j;                                                 \
		gdb_command_t *cmd;                                    \
                                                               \
		/* Remove optional mpc_maybe() arguments */            \
		for (j = n - 1; j >= 0; j--)                           \
			if (!xs[j])                                        \
				--n;                                           \
                                                               \
		xassert(n > 0);                                        \
		cmd = gdb_new_cmd(xs[0], TYPE);                        \
		FUNC;                                                  \
                                                               \
		return cmd;                                            \
	}

#define GDBF_ARG_HCMD                              \
	do {                                           \
		xassert(n == 3);                           \
                                                   \
		cmd->v.hcmd.op = *((char *)xs[1]);         \
		cmd->v.hcmd.id = *((gdb_thread_t *)xs[2]); \
                                                   \
		free(xs[1]);                               \
		free(xs[2]);                               \
	} while (0)

#define GDBF_ARG_INT                   \
	do {                               \
		xassert(n == 2);               \
                                       \
		cmd->v.ival = *((int *)xs[1]); \
		free(xs[1]);                   \
	} while (0)

#define GDBF_ARG_VCONT                      \
	do {                                    \
		xassert(n == 2);                    \
		cmd->v.vval = (gdb_vcont_t *)xs[1]; \
	} while (0)

#define GDBF_ARG_MEMORY                        \
	do {                                       \
		xassert(n == 2);                       \
		cmd->v.mem = *((gdb_memory_t *)xs[1]); \
		free(xs[1]);                           \
	} while (0)

#define GDBF_ARG_MEMORYW                                 \
	do {                                                 \
		xassert(n == 4);                                 \
		cmd->v.memw.location = *((gdb_memory_t *)xs[1]); \
		cmd->v.memw.data = (char *)xs[3];                \
		free(xs[1]);                                     \
		free(xs[2]);                                     \
	} while (0)

#define GDBF_ARG_BREAK                                               \
	do {                                                             \
		int type;                                                    \
                                                                     \
		xassert(n == 4);                                             \
		type = *(int *)(xs[1]);                                      \
		xassert(type >= GDB_ZKIND_SOFT && type <= GDB_ZKIND_WATCHA); \
                                                                     \
		cmd->v.bval.type = (gdb_ztype_t)type;                        \
		cmd->v.bval.address = *((gdb_addr_t *)xs[2]);                \
		cmd->v.bval.kind = *(size_t *)(xs[3]);                       \
		free(xs[1]);                                                 \
		free(xs[2]);                                                 \
		free(xs[3]);                                                 \
	} while (0)

#define GDBF_ARG_THREAD                         \
	do {                                        \
		xassert(n == 2);                        \
		cmd->v.tval = *((gdb_thread_t *)xs[1]); \
		free(xs[1]);                            \
	} while (0)
