#include <err.h>
#include <libgdb/parser1.h>
#include <libgdb/parser2.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

#define GDB_RUNLEN_CHAR '*'
#define GDB_RUNLEN_OFF 29
#define GDB_RUNLEN_STEP 32
#define GDB_ESCAPE_CHAR '}'
#define GDB_ESCAPE_BYTE 0x20

int calc_csum(const char *data) {
	size_t i;
	int csum = 0;

	for (i = 0; i < strlen(data); i++) csum += (int)data[i];

	return csum % 256;
}

char *gdb_decode_runlen(char *data) {
	int rcount, j;
	size_t i, nlen, nrem;
	int runlen;
	char *ndat;

	nlen = 0;
	nrem = strlen(data);
	ndat = xmalloc(nrem);

	for (runlen = -1, i = 0; i < strlen(data); i++) {
		if (data[i] == GDB_RUNLEN_CHAR) {
			if (i <= 0)
				goto err;
			runlen = data[i - 1];
			continue;
		}

		if (runlen == -1) {
			runlen = data[i];
			rcount = 1;
		} else {
			rcount = (int)data[i] - GDB_RUNLEN_OFF;
			if (rcount <= 0)
				goto err;
		}

		for (j = 0; j < rcount; j++) {
			if (nrem-- == 0) {
				ndat = xrealloc(ndat, nlen + GDB_RUNLEN_STEP);
				nrem += GDB_RUNLEN_STEP;
			}

			xassert(runlen >= 0 && runlen <= CHAR_MAX);
			ndat[nlen++] = (char)runlen;
		}

		runlen = -1;
	}

	/* shrink to actual size */
	ndat = xrealloc(ndat, nlen + 1);
	ndat[nlen] = '\0';

	return ndat;
err:
	free(ndat);
	return NULL;
}

char *gdb_unescape(char *data) {
	size_t i, nlen;
	char *ndat;
	bool esc;

	ndat = xmalloc(strlen(data));
	nlen = 0;

	for (esc = false, i = 0; i < strlen(data); i++) {
		if (data[i] == GDB_ESCAPE_CHAR) {
			esc = true;
			continue;
		}

		ndat[nlen++] = (esc) ? data[i] ^ GDB_ESCAPE_BYTE : data[i];
		esc = false;
	}

	/* shrink to actual size */
	ndat = xrealloc(ndat, nlen + 1);
	ndat[nlen] = '\0';

	return ndat;
}

bool gdb_is_valid(gdb_packet_t *pkt) {
	int ret;
	int expcsum;
	char strcsum[GDB_CSUM_LEN + 1]; /* +1 for snprintf nullbyte */

	if (!pkt->data)
		return true;
	expcsum = calc_csum(pkt->data);

	ret = snprintf(strcsum, sizeof(strcsum), "%.2x", expcsum);
	xassert(ret == GDB_CSUM_LEN);

	return !strncmp(pkt->csum, strcsum, GDB_CSUM_LEN);
}

gdb_command_t *gdb_new_cmd(char *name, gdb_argument_t type) {
	gdb_command_t *cmd;

	cmd = xmalloc(sizeof(*cmd));
	cmd->name = name;
	cmd->type = type;

	return cmd;
}

void gdb_free_cmd(gdb_command_t *cmd) {
	gdb_vcont_t *parent, *next;

	if (cmd->type == GDB_ARG_MEMORYW)
		free(cmd->v.memw.data);

	if (cmd->type == GDB_ARG_VCONT) {
		parent = cmd->v.vval;
		while (parent) {
			next = parent->next;
			free(parent);
			parent = next;
		}
	}

	free(cmd->name);
	free(cmd);
}

void *xrealloc(void *ptr, size_t size) {
	void *r;

	if (!(r = realloc(ptr, size)))
		err(EXIT_FAILURE, "realloc failed");

	return r;
}

void *xmalloc(size_t size) {
	void *r;

	if (!(r = malloc(size)))
		err(EXIT_FAILURE, "malloc failed");

	return r;
}

char *xstrdup(char *s) {
	char *r;

	if (!(r = strdup(s)))
		err(EXIT_FAILURE, "strdup failed");

	return r;
}

void gdb_free_packet(gdb_packet_t *pkt) {
	if (pkt->data)
		free(pkt->data);
	free(pkt);
}
