#include <err.h>
#include <libgdb/parser1.h>
#include <libgdb/response.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

static char kind_to_char(gdb_kind_t kind) {
	switch (kind) {
		case GDB_KIND_NOTIFY:
			return '#';
		case GDB_KIND_PACKET:
			return '$';
		case GDB_KIND_NACK:
			return '-';
		case GDB_KIND_ACK:
			return '+';
		default:
			xassert(0);
			return -1;
	}
}

char *gdb_serialize(gdb_kind_t kind, const char *data) {
	size_t pktlen;
	char *serialized;
	char pktkind;
	int csum, ret;

	pktkind = kind_to_char(kind);
	if (kind == GDB_KIND_NACK || kind == GDB_KIND_ACK) {
		xassert(data == NULL);
		serialized = xmalloc(2); /* kind + nullbyte */

		serialized[0] = pktkind;
		serialized[1] = '\0';

		return serialized;
	}

	csum = calc_csum(data);

	/* + 3 → nullbyte, checksum delimiter, kind */
	pktlen = strlen(data) + GDB_CSUM_LEN + 3;
	serialized = xmalloc(pktlen);

	ret = snprintf(serialized, pktlen, "%c%s#%.2x", pktkind, data, csum);
	if (ret < 0)
		err(EXIT_FAILURE, "snprintf failed");
	else if ((size_t)ret >= pktlen)
		errx(EXIT_FAILURE, "insufficient snprintf buffer size");

	return serialized;
}
