#ifndef LIBGDB_PARSER1_H
#define LIBGDB_PARSER1_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define GDB_CSUM_LEN 2

typedef enum {
	GDB_KIND_NOTIFY,
	GDB_KIND_PACKET,
	GDB_KIND_NACK,
	GDB_KIND_ACK,
} gdb_kind_t;

typedef struct {
	gdb_kind_t kind;
	char *data;
	char csum[GDB_CSUM_LEN];
} gdb_packet_t;

void gdb_free_packet(gdb_packet_t *);
gdb_packet_t *gdb_parse_pkt(FILE *);

#ifdef __cplusplus
}
#endif

#endif
