#include <assert.h>
#include <libgdb/parser2.h>
#include <limits.h>
#include <stdlib.h>

#include "debug.h"
#include "gdb_server.h"
#include "register_format.h"

enum {
	GDB_PKTSIZ = 4096,
	GDB_PC_REG = 32,
};

std::map<std::string, GDBServer::packet_handler> handlers{
    {"?", &GDBServer::haltReason},
    {"g", &GDBServer::getRegisters},
    {"H", &GDBServer::setThread},
    {"k", &GDBServer::killServer},
    {"m", &GDBServer::readMemory},
    {"M", &GDBServer::writeMemory},
    {"p", &GDBServer::readRegister},
    {"qAttached", &GDBServer::qAttached},
    {"qSupported", &GDBServer::qSupported},
    {"qfThreadInfo", &GDBServer::threadInfo},
    {"qsThreadInfo", &GDBServer::threadInfoEnd},
    {"T", &GDBServer::isAlive},
    {"vCont", &GDBServer::vCont},
    {"vCont?", &GDBServer::vContSupported},
    {"z", &GDBServer::removeBreakpoint},
    {"Z", &GDBServer::setBreakpoint},
};

void GDBServer::haltReason(int conn, gdb_command_t *cmd) {
	(void)cmd;

	// If n is a recognized stop reason […]. The aa should be
	// ‘05’, the trap signal. At most one stop reason should be
	// present.

	// TODO: Only send create conditionally.
	send_packet(conn, "S05");
}

void GDBServer::getRegisters(int conn, gdb_command_t *cmd) {
	(void)cmd;

	auto formatter = new RegisterFormater(arch);
	auto fn = [formatter](debug_target_if *hart) {
		for (uint64_t v : hart->get_registers()) formatter->formatRegister(v);
	};

	exec_thread(fn);
	send_packet(conn, formatter->str().c_str());
	delete formatter;
}

void GDBServer::setThread(int conn, gdb_command_t *cmd) {
	gdb_cmd_h_t *hcmd;

	hcmd = &cmd->v.hcmd;
	thread_ops[hcmd->op] = hcmd->id.tid;

	send_packet(conn, "OK");
}

void GDBServer::killServer(int conn, gdb_command_t *cmd) {
	(void)conn;
	(void)cmd;

	// TODO: Stop the System C simulation instead of
	// terminating the program. This would require interacting
	// with the GDBServerRunner directly to make it exit.
	//
	// This could be implemented by adding sys_exit to
	// debug_target_if, however, some SystemC modules, e.g. FU540_PLIC
	// also spawn threads which are not stopped at all. These
	// modules need to be fixed first.
	exit(EXIT_SUCCESS);
}

void GDBServer::readMemory(int conn, gdb_command_t *cmd) {
	gdb_memory_t *mem;

	mem = &cmd->v.mem;

	assert(mem->addr <= UINT64_MAX);
	assert(mem->length <= UINT_MAX);

	std::string retval;
	auto fn = [this, &retval, mem](debug_target_if *hart) {
		uint64_t addr = translate_addr(hart, mem->addr, LOAD);
		retval += memory->read_memory(addr, (unsigned)mem->length);
	};

	try {
		exec_thread(fn);
	} catch (const std::runtime_error &) { /* exception raised in fn */
		send_packet(conn, "E01");
		return;
	}

	send_packet(conn, retval.c_str());
}

void GDBServer::writeMemory(int conn, gdb_command_t *cmd) {
	gdb_memory_t *loc;
	gdb_memory_write_t *mem;

	mem = &cmd->v.memw;
	loc = &mem->location;

	assert(loc->addr <= UINT64_MAX);
	assert(loc->length <= UINT_MAX);

	auto fn = [this, loc, mem](debug_target_if *hart) {
		uint64_t addr = translate_addr(hart, loc->addr, STORE);
		memory->write_memory(addr, (unsigned)loc->length, mem->data);
	};

	try {
		exec_thread(fn);
	} catch (const std::runtime_error &) {
		send_packet(conn, "E01");
		return;
	}

	send_packet(conn, "OK");
}

void GDBServer::readRegister(int conn, gdb_command_t *cmd) {
	int reg;
	auto formatter = new RegisterFormater(arch);

	reg = cmd->v.ival;
	auto fn = [formatter, reg](debug_target_if *hart) {
		uint64_t regval;
		if (reg == GDB_PC_REG) {
			regval = hart->get_progam_counter();
		} else {
			regval = hart->read_register(reg);
		}

		/* TODO: handle CSRs? */

		formatter->formatRegister(regval);
	};

	try {
		exec_thread(fn);
	} catch (const std::out_of_range &) { /* exception raised in fn */
		send_packet(conn, "E01");
		goto ret;
	}

	send_packet(conn, formatter->str().c_str());
ret:
	delete formatter;
}

void GDBServer::threadInfo(int conn, gdb_command_t *cmd) {
	(void)cmd;

	std::string thrlist = "m";

	/* TODO: refactor this to make it always output hex digits,
	 * preferablly move it to the protocol code/ */
	for (size_t i = 0; i < harts.size(); i++) {
		debug_target_if *hart = harts.at(i);
		if (hart->get_status() == CoreExecStatus::Terminated)
			continue;

		thrlist += std::to_string(hart->get_hart_id() + 1);
		if (i + 1 < harts.size())
			thrlist += ",";
	}

	send_packet(conn, thrlist.c_str());
}

void GDBServer::threadInfoEnd(int conn, gdb_command_t *cmd) {
	(void)cmd;

	// GDB will respond to each reply with a request for more thread
	// ids (using the ‘qs’ form of the query), until the target
	// responds with ‘l’ (lower-case ell, for last).
	//
	// Since the GDBServer::threadInfo sends all threads in one
	// response we always terminate the list here.
	send_packet(conn, "l");
}

void GDBServer::qAttached(int conn, gdb_command_t *cmd) {
	(void)cmd;

	// 0 process started, 1 attached to process
	send_packet(conn, "0");
}

void GDBServer::qSupported(int conn, gdb_command_t *cmd) {
	(void)cmd;

	send_packet(conn, ("vContSupported+;PacketSize=" + std::to_string(GDB_PKTSIZ)).c_str());
}

void GDBServer::isAlive(int conn, gdb_command_t *cmd) {
	gdb_thread_t *thr;

	thr = &cmd->v.tval;
	try {
		get_threads(thr->tid);
	} catch (const std::out_of_range &) {
		send_packet(conn, "E01");
		return;
	}

	send_packet(conn, "OK");
}

void GDBServer::vCont(int conn, gdb_command_t *cmd) {
	gdb_vcont_t *vcont;
	int stopped_thread = -1;
	const char *stop_reason = NULL;
	std::map<debug_target_if *, bool> matched;

	/* This handler attempts to implement the all-stop mode.
	 * See: https://sourceware.org/gdb/onlinedocs/gdb/All_002dStop-Mode.html */

	vcont = cmd->v.vval;
	for (vcont = cmd->v.vval; vcont; vcont = vcont->next) {
		bool single = false;
		if (vcont->action == 's')
			single = true;
		else if (vcont->action != 'c')
			throw std::invalid_argument("Unimplemented vCont action"); /* TODO */

		std::vector<debug_target_if *> selected_harts;
		try {
			auto run = get_threads(vcont->thread.tid);
			for (auto i = run.begin(); i != run.end();) {
				debug_target_if *hart = *i;
				if (matched.count(hart))
					i = run.erase(i); /* already matched */
				else
					i++;
			}

			selected_harts = run_threads(run, single);
		} catch (const std::out_of_range &) {
			send_packet(conn, "E01");
			return;
		}

		for (debug_target_if *hart : selected_harts) {
			switch (hart->get_status()) {
				case CoreExecStatus::HitBreakpoint:
					stop_reason = "05";
					stopped_thread = hart->get_hart_id() + 1;

					/* mark runnable again */
					hart->set_status(CoreExecStatus::Runnable);

					break;
				case CoreExecStatus::Terminated:
					stop_reason = "03";
					stopped_thread = hart->get_hart_id() + 1;
					break;
				case CoreExecStatus::Runnable:
					continue;
			}
		}

		/* The vCont specification mandates that only the leftmost action with
		 * a matching thread-id is applied. Unfortunately, the specification
		 * is a bit unclear in regards to handling two actions with no thread
		 * id (i.e. GDB_THREAD_ALL). */
		if (vcont->thread.tid > 0) {
			auto threads = get_threads(vcont->thread.tid);
			assert(threads.size() == 1);
			matched[threads.front()] = true;
		}
	}

	assert(stop_reason && stopped_thread >= 1);

	/* This sets the current thread for various follow-up
	 * operations, most importantly readRegister. Without this
	 * change gdb reads the registers of the previously selected
	 * thread and doesn't switch threads properly if a thread other
	 * than the currently selected hits a breakpoint.
	 *
	 * XXX: No idea if the stub is really required to do this. */
	thread_ops['g'] = stopped_thread;

	const std::string msg = std::string("T") + stop_reason + "thread:" + std::to_string(stopped_thread) + ";";
	send_packet(conn, msg.c_str());
}

void GDBServer::vContSupported(int conn, gdb_command_t *cmd) {
	(void)cmd;

	// We need to support both c and C otherwise GDB doesn't use vCont
	// This is documented in the remote_vcont_probe function in the GDB source.
	send_packet(conn, "vCont;c;C");
}

void GDBServer::removeBreakpoint(int conn, gdb_command_t *cmd) {
	gdb_breakpoint_t *bpoint;

	bpoint = &cmd->v.bval;
	if (bpoint->type != GDB_ZKIND_SOFT) {
		send_packet(conn, ""); /* not supported */
		return;
	}

	for (debug_target_if *hart : harts) hart->remove_breakpoint(bpoint->address);
	send_packet(conn, "OK");
}

void GDBServer::setBreakpoint(int conn, gdb_command_t *cmd) {
	gdb_breakpoint_t *bpoint;

	bpoint = &cmd->v.bval;
	if (bpoint->type != GDB_ZKIND_SOFT) {
		send_packet(conn, ""); /* not supported */
		return;
	}

	for (debug_target_if *hart : harts) hart->insert_breakpoint(bpoint->address);
	send_packet(conn, "OK");
}
