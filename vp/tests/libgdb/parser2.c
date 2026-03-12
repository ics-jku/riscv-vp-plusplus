#include <ptest.h>
#include <libgdb/parser2.h>

#include "suite.h"
#include "util.h"

#define SUITE "Suite for second parser stage"

static void
test_set_thread_packet(void)
{
	gdb_command_t *cmd;
	gdb_cmd_h_t *hcmd;

	cmd = parse_cmd("Hc-1");
	PT_ASSERT_STR_EQ(cmd->name, "H");
	PT_ASSERT(cmd->type == GDB_ARG_H);

	hcmd = &cmd->v.hcmd;
	PT_ASSERT(hcmd->op == 'c');
	PT_ASSERT(hcmd->id.pid == GDB_THREAD_UNSET);
	PT_ASSERT(hcmd->id.tid == -1);

	gdb_free_cmd(cmd);
}

static void
test_read_register_packet(void)
{
	gdb_command_t *cmd;

	cmd = parse_cmd("p20");
	PT_ASSERT_STR_EQ(cmd->name, "p");
	PT_ASSERT(cmd->type == GDB_ARG_INT);

	PT_ASSERT(cmd->v.ival == 0x20);

	gdb_free_cmd(cmd);
}

static void
test_read_memory_packet(void)
{
	gdb_command_t *cmd;
	gdb_memory_t *mem;

	cmd = parse_cmd("m20400000,4");
	PT_ASSERT_STR_EQ(cmd->name, "m");
	PT_ASSERT(cmd->type == GDB_ARG_MEMORY);

	mem = &cmd->v.mem;
	PT_ASSERT(mem->addr == 0x20400000);
	PT_ASSERT(mem->length == 4);

	gdb_free_cmd(cmd);
}

static void
test_write_memory_packet(void)
{
	gdb_command_t *cmd;
	gdb_memory_write_t *mem;
	gdb_memory_t *loc;

	cmd = parse_cmd("M20400000,4:ffffffff");
	PT_ASSERT_STR_EQ(cmd->name, "M");
	PT_ASSERT(cmd->type == GDB_ARG_MEMORYW);

	mem = &cmd->v.memw;
	PT_ASSERT_STR_EQ(mem->data, "ffffffff");

	loc = &mem->location;
	PT_ASSERT(loc->addr == 0x20400000);
	PT_ASSERT(loc->length == 4);

	gdb_free_cmd(cmd);
}

static void
test_liveness_check_packet(void)
{
	gdb_command_t *cmd;
	gdb_thread_t *thr;

	cmd = parse_cmd("Tp-1.0");
	PT_ASSERT_STR_EQ(cmd->name, "T");
	PT_ASSERT(cmd->type == GDB_ARG_THREAD);

	thr = &cmd->v.tval;
	PT_ASSERT(thr->pid == GDB_THREAD_ALL);
	PT_ASSERT(thr->tid == GDB_THREAD_ANY);

	gdb_free_cmd(cmd);
}

static void
test_insert_soft_breakpoint_packet(void)
{
	gdb_command_t *cmd;
	gdb_breakpoint_t *brk;

	cmd = parse_cmd("Z0,20400326,2");
	PT_ASSERT_STR_EQ(cmd->name, "Z");
	PT_ASSERT(cmd->type == GDB_ARG_BREAK);

	brk = &cmd->v.bval;
	PT_ASSERT(brk->type == GDB_ZKIND_SOFT);
	PT_ASSERT(brk->address == 0x20400326);
	PT_ASSERT(brk->kind == 2);

	gdb_free_cmd(cmd);
}

static void
test_vcont_single_action(void)
{
	gdb_command_t *cmd;
	gdb_vcont_t *vcont;

	cmd = parse_cmd("vCont;s:4");
	PT_ASSERT_STR_EQ(cmd->name, "vCont");
	PT_ASSERT(cmd->type == GDB_ARG_VCONT);

	vcont = cmd->v.vval;
	PT_ASSERT(vcont->action == 's');
	PT_ASSERT(vcont->sig == -1);
	PT_ASSERT(vcont->thread.pid == GDB_THREAD_UNSET);
	PT_ASSERT(vcont->thread.tid == 4);
	PT_ASSERT(vcont->next == NULL);

	gdb_free_cmd(cmd);
}

static void
test_vcont_multiple_actions(void)
{
	gdb_command_t *cmd;
	gdb_vcont_t *vcont;

	cmd = parse_cmd("vCont;s:1;c");
	PT_ASSERT_STR_EQ(cmd->name, "vCont");
	PT_ASSERT(cmd->type == GDB_ARG_VCONT);

	vcont = cmd->v.vval;
	PT_ASSERT(vcont->action == 's');
	PT_ASSERT(vcont->sig == -1);
	PT_ASSERT(vcont->thread.pid == GDB_THREAD_UNSET);
	PT_ASSERT(vcont->thread.tid == 1);
	PT_ASSERT(vcont->next != NULL);

	vcont = cmd->v.vval->next;
	PT_ASSERT(vcont->action == 'c');
	PT_ASSERT(vcont->sig == -1);
	PT_ASSERT(vcont->thread.pid == GDB_THREAD_UNSET);
	PT_ASSERT(vcont->thread.tid == GDB_THREAD_ALL);
	PT_ASSERT(vcont->next == NULL);

	gdb_free_cmd(cmd);
}

void
suite_parser2(void)
{
	pt_add_test(test_set_thread_packet, "Test parser for 'H' packet", SUITE);
	pt_add_test(test_read_register_packet, "Test parser for 'P' packet", SUITE);
	pt_add_test(test_read_memory_packet, "Test parser for 'm' packet", SUITE);
	pt_add_test(test_write_memory_packet, "Test parser for 'M' packet", SUITE);
	pt_add_test(test_liveness_check_packet, "Test parser for 'T' packet", SUITE);
	pt_add_test(test_insert_soft_breakpoint_packet, "Test parser for 'Z' packet", SUITE);
	pt_add_test(test_vcont_single_action, "Test parser for single 'vCont' action", SUITE);
	pt_add_test(test_vcont_multiple_actions, "Test parser for multiple 'vCont' actions", SUITE);
	return;
}
