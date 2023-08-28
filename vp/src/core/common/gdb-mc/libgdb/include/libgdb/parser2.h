#ifndef LIBGDB_PARSER2_H
#define LIBGDB_PARSER2_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <libgdb/parser1.h>
#include <stddef.h>
#include <stdint.h>

#ifndef LIBGDB_ADDR_TYPE
#define LIBGDB_ADDR_TYPE uint64_t
#endif
#ifndef LIBGDB_ADDR_FORMAT
#define LIBGDB_ADDR_FORMAT SCNx64
#endif

typedef LIBGDB_ADDR_TYPE gdb_addr_t;
#undef LIBGDB_ADDR_TYPE

enum {
	GDB_THREAD_UNSET = -2,
	GDB_THREAD_ANY = 0,
	GDB_THREAD_ALL = -1,
};

typedef struct {
	int pid; /* requires multiprocess feature */
	int tid;
} gdb_thread_t;

typedef enum {
	GDB_ZKIND_SOFT = 0,
	GDB_ZKIND_HARD,
	GDB_ZKIND_WATCHW,
	GDB_ZKIND_WATCHR,
	GDB_ZKIND_WATCHA,
} gdb_ztype_t;

typedef struct {
	gdb_ztype_t type;
	gdb_addr_t address;
	size_t kind;
} gdb_breakpoint_t;

typedef struct {
	gdb_addr_t addr;
	size_t length;
} gdb_memory_t;

typedef struct {
	gdb_memory_t location;
	char *data; /* null-terminated hexstring */
} gdb_memory_write_t;

typedef struct {
	char op;
	gdb_thread_t id;
} gdb_cmd_h_t;

typedef struct _gdb_vcont_t gdb_vcont_t;

struct _gdb_vcont_t {
	char action;
	int sig; /* -1 if unset */
	gdb_thread_t thread;

	gdb_vcont_t *next; /* NULL on end */
};

typedef enum {
	GDB_ARG_NONE,
	GDB_ARG_VCONT,
	GDB_ARG_H,
	GDB_ARG_INT,
	GDB_ARG_MEMORY,
	GDB_ARG_MEMORYW,
	GDB_ARG_BREAK,
	GDB_ARG_THREAD,
} gdb_argument_t;

typedef struct {
	char *name;
	gdb_argument_t type;

	union {
		gdb_vcont_t *vval;
		gdb_cmd_h_t hcmd;
		int ival;
		gdb_memory_t mem;
		gdb_memory_write_t memw;
		gdb_breakpoint_t bval;
		gdb_thread_t tval;
	} v;
} gdb_command_t;

void gdb_free_cmd(gdb_command_t *);
gdb_command_t *gdb_parse_cmd(gdb_packet_t *);

#ifdef __cplusplus
}
#endif

#endif
