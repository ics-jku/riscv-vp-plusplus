#include <libgdb/parser2.h>
#include <stdlib.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/common/debug.h"
#include "gdb_regs.h"
#include "gdb_server.h"
#include "gdb_xml.h"

enum {
	GDB_PKTSIZ = 4096,
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
    {"qMemTags", &GDBServer::readMemoryTags},
    {"QMemTags", &GDBServer::writeMemoryTags},
    {"qIsAddressTagged", &GDBServer::isAddressTagged},
    {"qXfer:capa:read", &GDBServer::readCapability},
    {"qXfer:features:read", &GDBServer::readFeatures},
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

	std::vector<uint8_t> bytes;
	auto fn = [this, &bytes](debug_target_if *hart) {
		for (int reg : gdb_g_packet_registers(target)) {
			if (!gdb_read_register(hart, target, reg, bytes))
				throw std::out_of_range("register in 'g' packet not readable");
		}
	};

	try {
		exec_thread(fn);
	} catch (const std::out_of_range &) {  // exception raised in fn
		send_packet(conn, "E01");
		return;
	}

	send_packet(conn, gdb_hex_encode(bytes).c_str());
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

	// reply has two hex digits per byte and must fit into PacketSize
	if (mem->length > GDB_PKTSIZ / 2 || mem->length > UINT64_MAX - mem->addr) {
		send_packet(conn, "E01");
		return;
	}

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

	if (loc->length > GDB_PKTSIZ / 2 || loc->length > UINT64_MAX - loc->addr || strlen(mem->data) != 2 * loc->length) {
		send_packet(conn, "E01");
		return;
	}

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
	int reg = cmd->v.ival;

	std::vector<uint8_t> bytes;
	auto fn = [this, &bytes, reg](debug_target_if *hart) {
		if (!gdb_read_register(hart, target, reg, bytes))
			throw std::out_of_range("Unimplemented register read");
	};

	try {
		exec_thread(fn);
	} catch (const std::out_of_range &) {  // exception raised in fn
		send_packet(conn, "E01");
		return;
	}

	send_packet(conn, gdb_hex_encode(bytes).c_str());
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
	char pktsiz[32];
	snprintf(pktsiz, sizeof(pktsiz), "PacketSize=%x", GDB_PKTSIZ);
	std::string features = pktsiz;
	features += ";qXfer:features:read+";
	if (target.cheri) {
		features += ";qXfer:capa:read+";
		features += ";memory-tagging+";
	}
	features += ";vContSupported+";

	send_packet(conn, features.c_str());
}

void GDBServer::isAlive(int conn, gdb_command_t *cmd) {
	gdb_thread_t *thr;

	thr = &cmd->v.tval;
	try {
		get_harts(thr->tid);
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
	std::vector<debug_target_if *> all_harts;

	/* This handler attempts to implement the all-stop mode.
	 * See: https://sourceware.org/gdb/onlinedocs/gdb/All_002dStop-Mode.html */

	vcont = cmd->v.vval;
	for (vcont = cmd->v.vval; vcont; vcont = vcont->next) {
		bool single = false;
		if (vcont->action == 's')
			single = true;
		else if (vcont->action != 'c')
			throw std::invalid_argument("Unimplemented vCont action"); /* TODO */

		std::vector<debug_target_if *> current_harts;
		try {
			current_harts = get_harts(vcont->thread.tid);
			for (auto i = current_harts.begin(); i != current_harts.end();) {
				debug_target_if *hart = *i;
				if (std::find(all_harts.begin(), all_harts.end(), hart) != all_harts.end())
					i = current_harts.erase(i); /* already matched */
				else
					i++;
			}
		} catch (const std::out_of_range &) {
			send_packet(conn, "E01");
			return;
		}

		for (debug_target_if *hart : current_harts) {
			set_single_run(hart, single);
			all_harts.push_back(hart);
		}
	}

	run_all_harts(all_harts);

	for (debug_target_if *hart : all_harts) {
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

	if (!stop_reason) {
		stop_reason = "02";
	}

	/* This sets the current thread for various follow-up
	 * operations, most importantly readRegister. Without this
	 * change gdb reads the registers of the previously selected
	 * thread and doesn't switch threads properly if a thread other
	 * than the currently selected hits a breakpoint.
	 *
	 * XXX: No idea if the stub is really required to do this. */
	if (stopped_thread >= 1) {
		thread_ops['g'] = stopped_thread;
	}

	std::string msg = "T" + std::string(stop_reason);
	if (stopped_thread >= 1) {
		msg += "thread:" + std::to_string(stopped_thread) + ";";
	}
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

void GDBServer::readMemoryTags(int conn, gdb_command_t *cmd) {
	// malformed packets fall through to the generic parser (GDB_ARG_NONE), cmd->v is not initialized then
	if (cmd->type != GDB_ARG_MEMTAGS || cmd->v.memtags.type != 0) {
		send_packet(conn, "E01");
		return;
	}

	uint64_t addr = cmd->v.memtags.location.addr;
	uint64_t len = cmd->v.memtags.location.length;
	std::string retval = "m";

	if (!target.cheri) {
		send_packet(conn, "E01");
		return;
	}

	// at most len / GDB_CAP_BYTES + 2 granules, two hex digits each, must fit into PacketSize
	if (len == 0 || len / GDB_CAP_BYTES + 2 > (GDB_PKTSIZ - 1) / 2) {
		send_packet(conn, "E01");
		return;
	}

	auto fn = [this, &retval, addr, len](debug_target_if *hart) {
		uint64_t paddr = translate_addr(hart, addr, LOAD);
		std::vector<uint8_t> tags = memory->read_memory_tags(paddr, len);
		// one tag per granule touched by [paddr, paddr + len)
		uint64_t ngranules = (paddr + len + GDB_CAP_BYTES - 1) / GDB_CAP_BYTES - paddr / GDB_CAP_BYTES;
		if (tags.size() != ngranules)
			throw std::runtime_error("short tag read");
		for (uint8_t tag : tags) {
			retval += (tag ? "01" : "00");
		}
	};

	try {
		exec_thread(fn);
	} catch (const std::runtime_error &) {
		send_packet(conn, "E14");
		return;
	}

	send_packet(conn, retval.c_str());
}

void GDBServer::writeMemoryTags(int conn, gdb_command_t *cmd) {
	(void)cmd;

	std::cout << "writeMemoryTags not implemented" << std::endl;
	send_packet(conn, "");
}

void GDBServer::isAddressTagged(int conn, gdb_command_t *cmd) {
	(void)cmd;

	std::cout << "isAddressTagged not implemented" << std::endl;
	send_packet(conn, "");
}

void GDBServer::readCapability(int conn, gdb_command_t *cmd) {
	if (!target.cheri) {
		send_packet(conn, "E01");
		return;
	}

	uint64_t addr = cmd->v.xfer_capa_read.addr;
	uint64_t offset = cmd->v.xfer_capa_read.offset;
	uint64_t length = cmd->v.xfer_capa_read.length;
	const uint64_t cap_size = GDB_CAP_BYTES + 1;  // tag byte followed by the capability
	if (addr % GDB_CAP_BYTES != 0) {
		send_packet(conn, "E22");
		return;
	}
	if (offset > cap_size) {
		send_packet(conn, "E22");
		return;
	}
	if (offset == cap_size) {
		send_packet(conn, "l");
		return;
	}

	uint64_t remaining = cap_size - offset;
	uint64_t reply_len = length < remaining ? length : remaining;
	uint8_t prefix = (offset + reply_len < cap_size) ? 'm' : 'l';

	std::vector<uint8_t> cap;
	auto fn = [this, &cap, addr](debug_target_if *hart) {
		uint64_t paddr = translate_addr(hart, addr, LOAD);
		std::vector<uint8_t> tag = memory->read_memory_tags(paddr, GDB_CAP_BYTES);
		std::vector<uint8_t> bytes = memory->read_memory_bytes(paddr, GDB_CAP_BYTES);
		if (tag.size() != 1 || bytes.size() != GDB_CAP_BYTES)
			throw std::runtime_error("short capability read");
		cap.push_back(tag.at(0));
		cap.insert(cap.end(), bytes.begin(), bytes.end());
	};

	try {
		exec_thread(fn);
	} catch (const std::runtime_error &) {
		send_packet(conn, "E14");
		return;
	}

	size_t start = offset;
	size_t end = start + reply_len;
	if (start >= cap.size()) {
		send_packet(conn, "E22");
		return;
	}
	if (end > cap.size()) {
		send_packet(conn, "E22");
		return;
	}

	std::vector<uint8_t> retval;
	retval.reserve(reply_len + 1);
	retval.push_back(prefix);
	retval.insert(retval.end(), cap.begin() + start, cap.begin() + end);
	send_binary_packet(conn, retval);
}

void GDBServer::readFeatures(int conn, gdb_command_t *cmd) {
	std::string_view xml = gdb_xml::lookup(target, cmd->v.xfer_features_read.annex);

	uint64_t offset = cmd->v.xfer_features_read.offset;
	uint64_t length = cmd->v.xfer_features_read.length;

	if (xml.empty()) {
		send_packet(conn, "E01");
		return;
	}

	size_t xml_len = xml.size();
	if (offset > xml_len) {
		send_packet(conn, "E01");
		return;
	}

	size_t remaining = xml_len - offset;
	size_t reply_len = length < remaining ? length : remaining;
	uint8_t prefix = (offset + reply_len < xml_len) ? 'm' : 'l';
	std::vector<uint8_t> retval(reply_len + 1);
	retval[0] = prefix;
	std::copy_n(xml.begin() + offset, reply_len, retval.begin() + 1);
	send_binary_packet(conn, retval);
}
