#ifndef GDB_REGS_H
#define GDB_REGS_H

#include <stdint.h>

#include <string>
#include <vector>

#include "core/common/debug.h"
#include "gdb_xml.h"

/*
 * GDB register numbering, identical for all targets (registers not present in a target are simply not described).
 * Must match the register numbers in the target descriptions in gdb-mc/xml/.
 *
 * The numbering matches (CHERI-)QEMU for a RV64 core with the D extension (QEMU assigns the numbers dynamically in
 * the order cpu, fpu, virtual, cheri, csr, ycsr).
 */
enum GdbRegNum {
	GDB_REG_X_FIRST = 0,  // x0-x31 (x0-x15 for the E base ISA)
	GDB_REG_PC = 32,
	GDB_REG_FP_FIRST = 33,   // f0-f31
	GDB_REG_PRIV = 65,       // virtual privilege level register
	GDB_REG_CAP_FIRST = 66,  // RVY: cnull, followed by the capability views of x1-x31
	GDB_REG_PCC = 98,        // RVY
	GDB_REG_DDC = 99,        // RVY: reported as regular register instead of a YCSR to match QEMU
	// GDB number = offset + CSR address (CSR addresses are 12 bit)
	GDB_REG_CSR_OFFSET = 100,
	// RVY: GDB number = offset + YCSR address, directly after the CSR range
	GDB_REG_YCSR_OFFSET = GDB_REG_CSR_OFFSET + 4096,
};

// Capability size in bytes without tag, also the tag granule size. TODO Only true for RV64
constexpr unsigned GDB_CAP_BYTES = 16;

// Append value to out in target (RISC-V little-endian) byte order, independent of the host byte order
template <typename T>
inline void gdb_append_le(std::vector<uint8_t> &out, T value) {
	for (size_t i = 0; i < sizeof(T); i++) out.push_back(static_cast<uint8_t>(value >> (8 * i)));
}

// Append the GDB representation of register regnum of hart to out.
// Returns false if the register is not part of the target description of cfg or cannot be read.
bool gdb_read_register(debug_target_if *hart, const gdb_xml::TargetConfig &cfg, int regnum, std::vector<uint8_t> &out);

// Registers sent in the 'g' packet, in order. GDB fetches all other registers individually with 'p'.
std::vector<int> gdb_g_packet_registers(const gdb_xml::TargetConfig &cfg);

// Encode bytes as hex string (two lower case digits per byte)
std::string gdb_hex_encode(const std::vector<uint8_t> &bytes);

#endif
