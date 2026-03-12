#include <stdlib.h>
#include <ptest.h>
#include <libgdb/parser1.h>

#include "suite.h"
#include "util.h"

/* From internal.h from libgdb */
extern int calc_csum(const char *);
extern bool gdb_is_valid(gdb_packet_t *);
extern char *gdb_unescape(char *);
extern char *gdb_decode_runlen(char *);

#define SUITE "Suite for parser utilities"

static void
test_checksum_calculation(void)
{
	/* Checksums have been extracted from a gdb
	 * communication run with `set debug remote 1` */

	PT_ASSERT(calc_csum("qsThreadInfo") == 0xc8);
	PT_ASSERT(calc_csum("qC") == 0xb4);
	PT_ASSERT(calc_csum("qTStatus") == 0x49);
	PT_ASSERT(calc_csum("vMustReplyEmpty") == 0x3a);
	PT_ASSERT(calc_csum("qOffsets") == 0x4b);
}

static void
test_validity_check_valid(void)
{
	gdb_packet_t pkt = {
		.kind = GDB_KIND_PACKET,
		.data = "qsThreadInfo",
		.csum = { 'c', '8' },
	};

	PT_ASSERT(gdb_is_valid(&pkt));
}

static void
test_validity_check_invalid(void)
{
	gdb_packet_t pkt = {
		.kind = GDB_KIND_PACKET,
		.data = "qsThreadInfo",
		.csum = { '4', '2' },
	};

	PT_ASSERT(!gdb_is_valid(&pkt));
}

static void
test_validity_check_ack(void)
{
	gdb_packet_t pkt = {
		.kind = GDB_KIND_ACK,
		.data = NULL,
		.csum = { 0, 0 },
	};

	PT_ASSERT(gdb_is_valid(&pkt));
}

static void
test_unescape(void)
{
	/* From the GDB documentation:
	 *   For example, the byte 0x7d would be transmitted as the two bytes 0x7d 0x5d.
	 */
	char data[] = { 0x7d, 0x5d, 0x0 };
	char *unesc;

	unesc = gdb_unescape(data);
	PT_ASSERT_STR_EQ(unesc, "}");
	free(unesc);
}

static void
test_runlen_decoding(void)
{
	struct {
		const char *in;
		const char *out;
	} tests[] = {
		{ "0* ", "0000" },         /* valid run-length encoding */
		{ "230* 42", "23000042" }, /* valid run-length encoding */
		{ "*", NULL },             /* no repeat character specified */
		{ "0*\t", NULL },          /* negative repeat count */
	};

	for (size_t i = 0; i < ARRAY_LEN(tests); i++) {
		char *decoded = gdb_decode_runlen((char *)tests[i].in);
		const char *expected = tests[i].out;

		if (!decoded || !expected) {
			PT_ASSERT(decoded == expected);
			continue;
		}

		PT_ASSERT_STR_EQ(decoded, expected);
		free(decoded);
	}
}

void
suite_parser_util(void)
{
	pt_add_test(test_checksum_calculation, "Test checksum calculation", SUITE);
	pt_add_test(test_validity_check_valid, "Test validity check with valid packet", SUITE);
	pt_add_test(test_validity_check_invalid, "Test validity check with invalid packet", SUITE);
	pt_add_test(test_validity_check_ack, "Test validity check with non-packet", SUITE);
	pt_add_test(test_unescape, "Test unescape function", SUITE);
	pt_add_test(test_runlen_decoding, "Test run-length decoding", SUITE);
}
