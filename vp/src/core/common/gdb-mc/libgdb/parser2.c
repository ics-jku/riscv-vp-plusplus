#include "parser2.h"

#include <errno.h>
#include <libgdb/parser1.h>
#include <libgdb/parser2.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "mpc.h"

static mpc_val_t *gdbf_ignarg(mpc_val_t *xs) {
	char *str, *sep;
	gdb_command_t *cmd;

	str = (char *)xs;
	if ((sep = strchr(str, ':')))
		*sep = '\0';

	cmd = gdb_new_cmd(xstrdup(str), GDB_ARG_NONE);
	free(xs);

	return cmd;
}

static mpc_val_t *gdbf_address(mpc_val_t *x) {
	int r;
	gdb_addr_t *v;

	v = xmalloc(sizeof(*v));

	r = sscanf((char *)x, "%" LIBGDB_ADDR_FORMAT, v);
	xassert(r == 1);

	free(x);
	return v;
}

static mpc_parser_t *gdb_address(void) {
	return mpc_expect(mpc_apply(mpc_hexdigits(), gdbf_address), "hexadecimal address");
}

static mpc_val_t *gdbf_uhex(mpc_val_t *x) {
	int r;
	size_t *v;

	v = xmalloc(sizeof(*v));

	r = sscanf((char *)x, "%zx", v);
	xassert(r == 1);

	free(x);
	return v;
}

static mpc_parser_t *gdb_uhex(void) {
	return mpc_apply(mpc_hexdigits(), gdbf_uhex);
}

static mpc_val_t *gdbf_negative(mpc_val_t *val) {
	int *neg;

	xassert(!strcmp((char *)val, "-1"));
	neg = xmalloc(sizeof(*neg));
	*neg = -1;

	free(val);
	return neg;
}

static mpc_val_t *gdbf_memory(int n, mpc_val_t **xs) {
	gdb_memory_t *mem;

	xassert(n == 3);
	xassert(*(char *)xs[1] == ',');

	mem = xmalloc(sizeof(*mem));
	mem->addr = *((gdb_addr_t *)xs[0]);
	mem->length = *((size_t *)xs[2]);

	free(xs[0]);
	free(xs[1]);
	free(xs[2]);

	return mem;
}

static mpc_parser_t *gdb_memory(void) {
	return mpc_and(3, gdbf_memory, gdb_address(), mpc_char(','), gdb_uhex(), free, free);
}

static mpc_val_t *gdbf_thread_id(int n, mpc_val_t **xs) {
	int *arg1, *arg2;
	gdb_thread_t *id;

	xassert(n == 2);

	id = xmalloc(sizeof(*id));
	arg1 = (int *)xs[0];
	arg2 = (int *)xs[1];

	if (arg1 != NULL)
		id->pid = *arg1;
	else
		id->pid = GDB_THREAD_UNSET;
	id->tid = *arg2;

	free(xs[0]);
	free(xs[1]);

	return id;
}

static mpc_parser_t *gdb_hexid(void) {
	/* Positive numbers with a target-specific interpretation,
	 * formatted as big-endian hex strings. Can also be a literal
	 * '-1' to indicate all threads, or '0' to pick any thread. */
	return mpc_or(2, mpc_hex(), mpc_apply(mpc_string("-1"), gdbf_negative));
}

static mpc_parser_t *gdb_thread_id(void) {
	mpc_parser_t *pid, *tid;

	tid = gdb_hexid();
	pid = mpc_and(3, mpcf_snd_free, mpc_char('p'), gdb_hexid(), mpc_char('.'), free, free);

	return mpc_and(2, gdbf_thread_id, mpc_maybe(pid), tid, free);
}

gdbf_fold(h, GDB_ARG_H, GDBF_ARG_HCMD)

    static mpc_parser_t *gdb_packet_h(void) {
	mpc_parser_t *op, *id;

	op = mpc_any(); /* (‘m’, ‘M’, ‘g’, ‘G’, et.al.). */
	id = gdb_thread_id();

	return mpc_and(3, gdbf_packet_h, mpc_char('H'), op, id, free, free);
}

gdbf_fold(p, GDB_ARG_INT, GDBF_ARG_INT)

    static mpc_parser_t *gdb_packet_p(void) {
	return mpc_and(2, gdbf_packet_p, mpc_char('p'), mpc_hex(), free);
}

static mpc_val_t *gdbf_vcont_action(int n, mpc_val_t **xs) {
	char *actstr;
	size_t actlen;
	gdb_vcont_t *vcont;

	xassert(n == 2);

	actstr = (char *)xs[0];
	actlen = strlen(actstr);

	vcont = xmalloc(sizeof(*vcont));
	vcont->action = *actstr;
	vcont->next = NULL;

	if (actlen == 1)
		vcont->sig = -1;
	else if (actlen == 3)
		vcont->sig = (int)strtol(actstr + 1, NULL, 16);
	else
		xassert(0);

	/* An action with no thread-id matches all threads. */
	if (!xs[1]) {
		vcont->thread.pid = GDB_THREAD_UNSET;
		vcont->thread.tid = GDB_THREAD_ALL;
	} else {
		vcont->thread = *((gdb_thread_t *)xs[1]);
	}

	free(xs[0]);
	free(xs[1]);

	return vcont;
}

static mpc_val_t *gdbf_vcont(int n, mpc_val_t **xs) {
	size_t i;
	gdb_vcont_t *vcont;

	xassert(n >= 1);

	for (vcont = (gdb_vcont_t *)xs[0], i = 1; i < (size_t)n; i++, vcont = vcont->next)
		vcont->next = (gdb_vcont_t *)xs[i];
	vcont->next = NULL;

	return (gdb_vcont_t *)xs[0];
}

gdbf_fold(vcont, GDB_ARG_VCONT, GDBF_ARG_VCONT)

    static mpc_parser_t *gdb_packet_vcont(void) {
	mpc_parser_t *action0, *action1, *action, *args, *thread;

	/* TODO: add support for r command */

	action0 = mpc_oneof("cst");
	action1 = mpc_and(2, mpcf_strfold, mpc_oneof("CS"), mpc_hexdigits());

	thread = mpc_and(2, mpcf_snd_free, mpc_char(':'), gdb_thread_id(), free);
	action = mpc_and(2, gdbf_vcont_action, mpc_or(2, action0, action1), mpc_maybe(thread),
	                 free); /* destructor incorrect but unused */

	args = mpc_many1(gdbf_vcont, mpc_and(2, mpcf_snd_free, mpc_char(';'), action, free));
	return mpc_and(2, gdbf_packet_vcont, mpc_string("vCont"), args, free);
}

gdbf_fold(m, GDB_ARG_MEMORY, GDBF_ARG_MEMORY)

    static mpc_parser_t *gdb_packet_m(void) {
	return mpc_and(2, gdbf_packet_m, mpc_char('m'), gdb_memory(), free);
}

gdbf_fold(M, GDB_ARG_MEMORYW, GDBF_ARG_MEMORYW)

    static mpc_parser_t *gdb_packet_M(void) {
	return mpc_and(4, gdbf_packet_M, mpc_char('M'), gdb_memory(), mpc_char(':'), mpc_hexdigits(), free, free, free);
}

static mpc_parser_t *gdb_arg(mpc_parser_t *par) {
	return mpc_and(2, mpcf_fst_free, par, mpc_char(','), free);
}

gdbf_fold(z, GDB_ARG_BREAK, GDBF_ARG_BREAK)

    static mpc_parser_t *gdb_packet_z(void) {
	mpc_parser_t *name, *type;

	/* TODO: parse cond_list */

	name = mpc_or(2, mpc_char('z'), mpc_char('Z'));
	type = mpc_apply(mpc_re("[0-4]"), mpcf_int);

	return mpc_and(4, gdbf_packet_z, name, gdb_arg(type), gdb_arg(gdb_address()), gdb_uhex(), free, free, free);
}

gdbf_fold(T, GDB_ARG_THREAD, GDBF_ARG_THREAD)

    static mpc_parser_t *gdb_packet_T(void) {
	return mpc_and(2, gdbf_packet_T, mpc_char('T'), gdb_thread_id(), free);
}

static mpc_parser_t *gdb_any(void) {
	return mpc_apply(mpc_many1(mpcf_strfold, mpc_any()), gdbf_ignarg);
}

static mpc_parser_t *gdb_cmd(mpc_parser_t *cmd) {
	return mpc_and(2, mpcf_fst_free, cmd, mpc_eoi(), gdb_free_cmd);
}

static mpc_parser_t *gdb_parse_stage2(void) {
	return mpc_or(8, gdb_cmd(gdb_packet_h()), gdb_cmd(gdb_packet_p()), gdb_cmd(gdb_packet_vcont()),
	              gdb_cmd(gdb_packet_m()), gdb_cmd(gdb_packet_M()), gdb_cmd(gdb_packet_z()), gdb_cmd(gdb_packet_T()),
	              gdb_any());
}

gdb_command_t *gdb_parse_cmd(gdb_packet_t *pkt) {
	mpc_result_t r;
	mpc_parser_t *par;
	char *data, *unesc;
	gdb_command_t *cmd;

	cmd = NULL;
	par = gdb_parse_stage2();

	if (pkt->kind != GDB_KIND_PACKET) {
		errno = EINVAL;
		return NULL;
	}

	if (!gdb_is_valid(pkt)) {
		errno = EILSEQ;
		return NULL;
	}

	if (!(data = gdb_decode_runlen(pkt->data))) {
		errno = EBADMSG;
		return NULL;
	}

	unesc = gdb_unescape(data);
	free(data);

	if (mpc_parse("<packet>", unesc, par, &r)) {
		cmd = (gdb_command_t *)r.output;
	} else {
#ifdef GDB_PARSER_DEBUG
		mpc_err_print(r.error);
#endif
		mpc_err_delete(r.error);
	}

	free(unesc);
	mpc_cleanup(1, par);

	if (!cmd)
		errno = EBADMSG; /* mpc_parse failed */

	return cmd;
}
