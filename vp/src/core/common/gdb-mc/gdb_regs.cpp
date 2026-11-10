#include "gdb_regs.h"

namespace {

void append_xlen(std::vector<uint8_t> &out, const gdb_xml::TargetConfig &cfg, uint64_t value) {
	if (cfg.xlen == 32)
		gdb_append_le(out, static_cast<uint32_t>(value));
	else
		gdb_append_le(out, value);
}

// Sent as tag byte followed by the capability (as done by QEMU), CHERI GDB expects tagged register types with an
// additional leading tag byte
void append_capability(std::vector<uint8_t> &out, const debug_capability &cap) {
	out.push_back(cap.tag ? 1 : 0);
	gdb_append_le(out, cap.bits);
}

unsigned num_x_regs(const gdb_xml::TargetConfig &cfg) {
	return cfg.embedded ? 16 : 32;
}

}  // namespace

bool gdb_read_register(debug_target_if *hart, const gdb_xml::TargetConfig &cfg, int regnum, std::vector<uint8_t> &out) {
	if (regnum < 0)
		return false;

	if (regnum < GDB_REG_PC) {
		if (static_cast<unsigned>(regnum) >= num_x_regs(cfg))
			return false;
		append_xlen(out, cfg, hart->read_register(regnum));
		return true;
	}

	if (regnum == GDB_REG_PC) {
		append_xlen(out, cfg, hart->get_progam_counter());
		return true;
	}

	if (regnum >= GDB_REG_FP_FIRST && regnum < GDB_REG_FP_FIRST + 32) {
		// FLEN is always 64 (D extension)
		gdb_append_le(out, hart->read_fp_register(regnum - GDB_REG_FP_FIRST));
		return true;
	}

	if (regnum == GDB_REG_PRIV) {
		append_xlen(out, cfg, hart->get_privilege());
		return true;
	}

	if (regnum >= GDB_REG_CSR_OFFSET && regnum < GDB_REG_YCSR_OFFSET) {
		uint64_t value;
		if (!hart->read_csr_gdb(regnum - GDB_REG_CSR_OFFSET, value))
			return false;
		append_xlen(out, cfg, value);
		return true;
	}

	// all remaining registers are RVY specific
	debug_target_cheri_if *cheri = cfg.cheri ? hart->get_cheri_debug_if() : nullptr;
	if (!cheri)
		return false;

	if (regnum >= GDB_REG_CAP_FIRST && regnum < GDB_REG_PCC) {
		append_capability(out, cheri->read_capability_register(regnum - GDB_REG_CAP_FIRST));
		return true;
	}
	if (regnum == GDB_REG_PCC) {
		append_capability(out, cheri->read_pcc());
		return true;
	}
	if (regnum == GDB_REG_DDC) {
		append_capability(out, cheri->read_ddc());
		return true;
	}
	if (regnum >= GDB_REG_YCSR_OFFSET) {
		debug_capability cap;
		if (!cheri->read_ycsr_gdb(regnum - GDB_REG_YCSR_OFFSET, cap))
			return false;
		append_capability(out, cap);
		return true;
	}

	return false;
}

std::vector<int> gdb_g_packet_registers(const gdb_xml::TargetConfig &cfg) {
	// GDB orders the 'g' packet by register number, so it must be a prefix of the described registers
	std::vector<int> regs;
	for (unsigned i = 0; i < num_x_regs(cfg); i++) regs.push_back(GDB_REG_X_FIRST + i);
	regs.push_back(GDB_REG_PC);
	for (int i = 0; i < 32; i++) regs.push_back(GDB_REG_FP_FIRST + i);
	regs.push_back(GDB_REG_PRIV);
	if (cfg.cheri) {
		for (int i = GDB_REG_CAP_FIRST; i <= GDB_REG_DDC; i++) regs.push_back(i);
	}
	return regs;
}

std::string gdb_hex_encode(const std::vector<uint8_t> &bytes) {
	static const char digits[] = "0123456789abcdef";
	std::string s;
	s.reserve(bytes.size() * 2);
	for (uint8_t b : bytes) {
		s.push_back(digits[b >> 4]);
		s.push_back(digits[b & 0xf]);
	}
	return s;
}
