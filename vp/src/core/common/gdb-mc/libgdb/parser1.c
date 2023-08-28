#include <libgdb/parser1.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "internal.h"
#include "mpc.h"

static mpc_val_t *gdbf_packet(int n, mpc_val_t **xs) {
	gdb_packet_t *pkt;
	char *start, *csum;

	xassert(n == 3);

	pkt = xmalloc(sizeof(*pkt));
	pkt->data = (char *)xs[1];

	start = (char *)xs[0];
	switch (*start) {
		case '%':
			pkt->kind = GDB_KIND_NOTIFY;
			break;
		case '$':
			pkt->kind = GDB_KIND_PACKET;
			break;
		default:
			xassert(0);
	}

	csum = (char *)xs[2];
	csum++; /* skip leading '#' */
	memcpy(pkt->csum, csum, GDB_CSUM_LEN);

	/* data (xs[1]) is still used in struct, don't free it */
	free(xs[0]);
	free(xs[2]);

	return pkt;
}

static mpc_val_t *gdbf_acknowledge(mpc_val_t *xs) {
	gdb_packet_t *pkt;
	char *str;

	pkt = xmalloc(sizeof(*pkt));
	pkt->data = NULL;
	memset(pkt->csum, 0, GDB_CSUM_LEN);

	str = (char *)xs;
	switch (*str) {
		case '+':
			pkt->kind = GDB_KIND_ACK;
			break;
		case '-':
			pkt->kind = GDB_KIND_NACK;
			break;
		default:
			xassert(0);
	}

	free(xs);
	return pkt;
}

static mpc_parser_t *gdb_csum(void) {
	mpc_parser_t *csum;

	csum = mpc_and(2, mpcf_strfold, mpc_any(), mpc_any(), free);
	return mpc_and(2, mpcf_strfold, mpc_char('#'), csum, free);
}

static mpc_parser_t *gdb_packet(void) {
	mpc_parser_t *data;

	data = mpc_many(mpcf_strfold, mpc_noneof("#$"));
	return mpc_and(3, gdbf_packet, mpc_char('$'), data, gdb_csum(), free, free);
}

static mpc_parser_t *gdb_notification(void) {
	mpc_parser_t *data;

	data = mpc_many(mpcf_strfold, mpc_noneof("#$%"));
	return mpc_and(3, gdbf_packet, mpc_char('%'), data, gdb_csum(), free, free);
}

static mpc_parser_t *gdb_acknowledge(void) {
	return mpc_apply(mpc_oneof("+-"), gdbf_acknowledge);
}

static mpc_parser_t *gdb_parse_stage1(void) {
	return mpc_or(3, gdb_acknowledge(), gdb_packet(), gdb_notification());
}

gdb_packet_t *gdb_parse_pkt(FILE *stream) {
	gdb_packet_t *pkt;
	mpc_parser_t *par;
	mpc_result_t r;

	pkt = NULL;
	par = mpc_predictive(gdb_parse_stage1());

	if (mpc_parse_pipe("<stream>", stream, par, &r)) {
		pkt = (gdb_packet_t *)r.output;
	} else {
#ifdef GDB_PARSER_DEBUG
		mpc_err_print(r.error);
#endif
		mpc_err_delete(r.error);
	}

	mpc_cleanup(1, par);
	return pkt;
}
