#include <ptest.h>
#include <libgdb/parser1.h>

#include "suite.h"
#include "util.h"

#define SUITE "Suite for first parser stage"

static void
test_packet_ack(void)
{
	gdb_packet_t *pkt;

	pkt = parse_pkt("testdata/ack.dat");

	PT_ASSERT(pkt->kind == GDB_KIND_ACK);
	PT_ASSERT(pkt->data == NULL);
	PT_ASSERT(pkt->csum[0] == 0);
	PT_ASSERT(pkt->csum[0] == 0);

	gdb_free_packet(pkt);
}

static void
test_packet_nack(void)
{
	gdb_packet_t *pkt;

	pkt = parse_pkt("testdata/nack.dat");

	PT_ASSERT(pkt->kind == GDB_KIND_NACK);
	PT_ASSERT(pkt->data == NULL);
	PT_ASSERT(pkt->csum[0] == 0);
	PT_ASSERT(pkt->csum[0] == 0);

	gdb_free_packet(pkt);
}

static void
test_packet_simple(void)
{
	gdb_packet_t *pkt;

	pkt = parse_pkt("testdata/simple.dat");

	PT_ASSERT(pkt->kind == GDB_KIND_PACKET);
	PT_ASSERT_STR_EQ(pkt->data, "qTStatus");
	PT_ASSERT(pkt->csum[0] == '4');
	PT_ASSERT(pkt->csum[1] == '9');

	gdb_free_packet(pkt);
}

static void
test_packet_notify(void)
{
	gdb_packet_t *pkt;

	pkt = parse_pkt("testdata/notify.dat");

	PT_ASSERT(pkt->kind == GDB_KIND_NOTIFY);
	PT_ASSERT_STR_EQ(pkt->data, "?");
	PT_ASSERT(pkt->csum[0] == '3');
	PT_ASSERT(pkt->csum[1] == 'f');

	gdb_free_packet(pkt);
}

void
suite_parser1(void)
{
	pt_add_test(test_packet_ack, "Test parser for acknowledgment", SUITE);
	pt_add_test(test_packet_nack, "Test parser for negative acknowledgment", SUITE);
	pt_add_test(test_packet_simple, "Test parser for simple GDB packet", SUITE);
	pt_add_test(test_packet_notify, "Test parser for simple notify packet", SUITE);
}
