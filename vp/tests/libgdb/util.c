#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>

#include <libgdb/parser1.h>
#include <libgdb/parser2.h>

#include "util.h"

/* From internal.h from libgdb */
extern int calc_csum(const char *);

gdb_packet_t *
parse_pkt(char *path)
{
	FILE *stream;
	gdb_packet_t *pkt;

	if (!(stream = fopen(path, "r")))
		err(EXIT_FAILURE, "fopen failed for '%s'", path);
	pkt = gdb_parse_pkt(stream);

	if (fclose(stream))
		err(EXIT_FAILURE, "fclose failed for '%s'", path);
	return pkt;
}

static void
make_packet(gdb_packet_t *pkt, char *data)
{
	int ret;
	char strcsum[GDB_CSUM_LEN + 1]; /* +1 for snprintf nullbyte */

	ret = snprintf(strcsum, sizeof(strcsum), "%.2x", calc_csum(data));
	assert(ret == GDB_CSUM_LEN);

	pkt->kind = GDB_KIND_PACKET;
	pkt->data = data;
	memcpy(&pkt->csum, strcsum, GDB_CSUM_LEN);
}

gdb_command_t *
parse_cmd(char *data)
{
	gdb_packet_t pkt;

	make_packet(&pkt, data);
	return gdb_parse_cmd(&pkt);
}
