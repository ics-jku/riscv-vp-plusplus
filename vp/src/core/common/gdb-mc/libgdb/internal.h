#ifndef GDB_PROTOCOL_FNS
#define GDB_PROTOCOL_FNS

#include <assert.h>
#include <libgdb/parser2.h>
#include <stddef.h>

#ifdef NDEBUG
#define xassert(X) ((void)(X)) /* prevent -Wunused-parameter warning */
#else
#define xassert(X) (assert(X))
#endif

void *xrealloc(void *, size_t);
void *xmalloc(size_t);
char *xstrdup(char *);

gdb_command_t *gdb_new_cmd(char *, gdb_argument_t);

int calc_csum(const char *);
bool gdb_is_valid(gdb_packet_t *);
char *gdb_unescape(char *);
char *gdb_decode_runlen(char *);

#endif
