/*
 * NEVER BUILD THIS FILE DIRECTLY!!!
 *
 * NOTE RVxx.2: C-style macros
 * concrete implementatins are are derived in iss.cpp (by iss_ctemplate_handle.h)
 * see NOTE RVxx.2 in iss_ctemplate_handle.h for more details
 */

#include "iss.h"

// to save *cout* format setting, see *ISS::show*
#include <boost/format.hpp>
#include <boost/io/ios_state.hpp>
// for safe down-cast
#include <boost/lexical_cast.hpp>

using namespace rv64;

#define VExt VExtension<ISS_CT>

// GCC and clang support these types on x64 machines
// perhaps use boost::multiprecision::int128_t instead
// see: https://stackoverflow.com/questions/18439520/is-there-a-128-bit-integer-in-c
typedef __int128_t int128_t;
typedef __uint128_t uint128_t;

#define RAISE_ILLEGAL_INSTRUCTION() raise_trap(EXC_ILLEGAL_INSTR, instr.data());

#define RD instr.rd()
#define RS1 instr.rs1()
#define RS2 instr.rs2()
#define RS3 instr.rs3()

ISS_CT::ISS_CT(uxlen_t hart_id, bool use_E_base_isa) : v_ext(*this), systemc_name("Core-" + std::to_string(hart_id)) {
	csrs.mhartid.reg = hart_id;
	op = Opcode::UNDEF;
	if (use_E_base_isa)
		csrs.misa.select_E_base_isa();

	sc_core::sc_time qt = tlm::tlm_global_quantum::instance().get();
	cycle_time = sc_core::sc_time(10, sc_core::SC_NS);

	assert(qt >= cycle_time);
	assert(qt % cycle_time == sc_core::SC_ZERO_TIME);

	for (int i = 0; i < Opcode::NUMBER_OF_INSTRUCTIONS; ++i) instr_cycles[i] = cycle_time;

	const sc_core::sc_time memory_access_cycles = 4 * cycle_time;
	const sc_core::sc_time mul_div_cycles = 8 * cycle_time;

	instr_cycles[Opcode::LB] = memory_access_cycles;
	instr_cycles[Opcode::LBU] = memory_access_cycles;
	instr_cycles[Opcode::LH] = memory_access_cycles;
	instr_cycles[Opcode::LHU] = memory_access_cycles;
	instr_cycles[Opcode::LW] = memory_access_cycles;
	instr_cycles[Opcode::SB] = memory_access_cycles;
	instr_cycles[Opcode::SH] = memory_access_cycles;
	instr_cycles[Opcode::SW] = memory_access_cycles;
	instr_cycles[Opcode::MUL] = mul_div_cycles;
	instr_cycles[Opcode::MULH] = mul_div_cycles;
	instr_cycles[Opcode::MULHU] = mul_div_cycles;
	instr_cycles[Opcode::MULHSU] = mul_div_cycles;
	instr_cycles[Opcode::DIV] = mul_div_cycles;
	instr_cycles[Opcode::DIVU] = mul_div_cycles;
	instr_cycles[Opcode::REM] = mul_div_cycles;
	instr_cycles[Opcode::REMU] = mul_div_cycles;
}

void ISS_CT::exec_step() {
	assert(((pc & ~pc_alignment_mask()) == 0) && "misaligned instruction");

	uint32_t mem_word;
	try {
		mem_word = instr_mem->load_instr(pc);
		instr = Instruction(mem_word);
	} catch (SimulationTrap &e) {
		op = Opcode::UNDEF;
		instr = Instruction(0);
		throw;
	}

	if (instr.is_compressed()) {
		op = instr.decode_and_expand_compressed(RV64, csrs.misa.fields.extensions);
		pc += 2;
	} else {
		op = instr.decode_normal(RV64, csrs.misa.fields.extensions);
		pc += 4;
	}

	if (trace) {
		printf("core %2lu: prv %1x: pc %16lx (%8x): %s ", csrs.mhartid.reg, prv, last_pc, mem_word,
		       Opcode::mappingStr.at(op));
		switch (Opcode::getType(op)) {
			case Opcode::Type::R:
				printf(COLORFRMT ", " COLORFRMT ", " COLORFRMT,
				       COLORPRINT(regcolors[instr.rd()], RegFile::regnames[instr.rd()]),
				       COLORPRINT(regcolors[instr.rs1()], RegFile::regnames[instr.rs1()]),
				       COLORPRINT(regcolors[instr.rs2()], RegFile::regnames[instr.rs2()]));
				break;
			case Opcode::Type::R4:
				printf(COLORFRMT ", " COLORFRMT ", " COLORFRMT ", " COLORFRMT,
				       COLORPRINT(regcolors[instr.rd()], RegFile::regnames[instr.rd()]),
				       COLORPRINT(regcolors[instr.rs1()], RegFile::regnames[instr.rs1()]),
				       COLORPRINT(regcolors[instr.rs2()], RegFile::regnames[instr.rs2()]),
				       COLORPRINT(regcolors[instr.rs3()], RegFile::regnames[instr.rs3()]));
				break;
			case Opcode::Type::I:
				printf(COLORFRMT ", " COLORFRMT ", 0x%x",
				       COLORPRINT(regcolors[instr.rd()], RegFile::regnames[instr.rd()]),
				       COLORPRINT(regcolors[instr.rs1()], RegFile::regnames[instr.rs1()]), instr.I_imm());
				break;
			case Opcode::Type::S:
				printf(COLORFRMT ", " COLORFRMT ", 0x%x",
				       COLORPRINT(regcolors[instr.rs1()], RegFile::regnames[instr.rs1()]),
				       COLORPRINT(regcolors[instr.rs2()], RegFile::regnames[instr.rs2()]), instr.S_imm());
				break;
			case Opcode::Type::B:
				printf(COLORFRMT ", " COLORFRMT ", 0x%x",
				       COLORPRINT(regcolors[instr.rs1()], RegFile::regnames[instr.rs1()]),
				       COLORPRINT(regcolors[instr.rs2()], RegFile::regnames[instr.rs2()]), instr.B_imm());
				break;
			case Opcode::Type::U:
				printf(COLORFRMT ", 0x%x", COLORPRINT(regcolors[instr.rd()], RegFile::regnames[instr.rd()]),
				       instr.U_imm());
				break;
			case Opcode::Type::J:
				printf(COLORFRMT ", 0x%x", COLORPRINT(regcolors[instr.rd()], RegFile::regnames[instr.rd()]),
				       instr.J_imm());
				break;
			default:;
		}
		puts("");
	}

	switch (op) {
		case Opcode::UNDEF:
			if (trace)
				std::cout << "[ISS] WARNING: unknown instruction '" << std::to_string(instr.data()) << "' at address '"
				          << std::to_string(last_pc) << "'" << std::endl;
			RAISE_ILLEGAL_INSTRUCTION();
			break;

		case Opcode::UNSUP:
			if (trace)
				std::cout << "[ISS] WARNING: instruction not supported (e.g. extension disabled in misa csr '"
				          << std::to_string(instr.data()) << "' at address '" << std::to_string(last_pc) << "'"
				          << std::endl;
			RAISE_ILLEGAL_INSTRUCTION();
			break;

		/*
		 * NOP instruction variants
		 * instructions decoded with rd == zero/x0 and no side effects -> nothing to do
		 */
		case Opcode::LUI_NOP:
		case Opcode::AUIPC_NOP:
		case Opcode::ADDI_NOP:
		case Opcode::SLTI_NOP:
		case Opcode::SLTIU_NOP:
		case Opcode::XORI_NOP:
		case Opcode::ORI_NOP:
		case Opcode::ANDI_NOP:
		case Opcode::SLLI_NOP:
		case Opcode::SRLI_NOP:
		case Opcode::SRAI_NOP:
		case Opcode::ADD_NOP:
		case Opcode::SUB_NOP:
		case Opcode::SLL_NOP:
		case Opcode::SLT_NOP:
		case Opcode::SLTU_NOP:
		case Opcode::XOR_NOP:
		case Opcode::SRL_NOP:
		case Opcode::SRA_NOP:
		case Opcode::OR_NOP:
		case Opcode::AND_NOP:
		case Opcode::MUL_NOP:
		case Opcode::MULH_NOP:
		case Opcode::MULHSU_NOP:
		case Opcode::MULHU_NOP:
		case Opcode::DIV_NOP:
		case Opcode::DIVU_NOP:
		case Opcode::REM_NOP:
		case Opcode::REMU_NOP:
		case Opcode::ADDIW_NOP:
		case Opcode::SLLIW_NOP:
		case Opcode::SRLIW_NOP:
		case Opcode::SRAIW_NOP:
		case Opcode::ADDW_NOP:
		case Opcode::SUBW_NOP:
		case Opcode::SLLW_NOP:
		case Opcode::SRLW_NOP:
		case Opcode::SRAW_NOP:
		case Opcode::MULW_NOP:
		case Opcode::DIVW_NOP:
		case Opcode::DIVUW_NOP:
		case Opcode::REMW_NOP:
		case Opcode::REMUW_NOP:
			break;

		/* rd != x0/zero variants */
		case Opcode::ADDI:
			regs[instr.rd()] = regs[instr.rs1()] + instr.I_imm();
			break;

		case Opcode::SLTI:
			regs[instr.rd()] = regs[instr.rs1()] < instr.I_imm();
			break;

		case Opcode::SLTIU:
			regs[instr.rd()] = ((uxlen_t)regs[instr.rs1()]) < ((uxlen_t)instr.I_imm());
			break;

		case Opcode::XORI:
			regs[instr.rd()] = regs[instr.rs1()] ^ instr.I_imm();
			break;

		case Opcode::ORI:
			regs[instr.rd()] = regs[instr.rs1()] | instr.I_imm();
			break;

		case Opcode::ANDI:
			regs[instr.rd()] = regs[instr.rs1()] & instr.I_imm();
			break;

		case Opcode::ADD:
			regs[instr.rd()] = regs[instr.rs1()] + regs[instr.rs2()];
			break;

		case Opcode::SUB:
			regs[instr.rd()] = regs[instr.rs1()] - regs[instr.rs2()];
			break;

		case Opcode::SLL:
			regs[instr.rd()] = regs[instr.rs1()] << regs.shamt(instr.rs2());
			break;

		case Opcode::SLT:
			regs[instr.rd()] = regs[instr.rs1()] < regs[instr.rs2()];
			break;

		case Opcode::SLTU:
			regs[instr.rd()] = ((uxlen_t)regs[instr.rs1()]) < ((uxlen_t)regs[instr.rs2()]);
			break;

		case Opcode::SRL:
			regs[instr.rd()] = ((uxlen_t)regs[instr.rs1()]) >> regs.shamt(instr.rs2());
			break;

		case Opcode::SRA:
			regs[instr.rd()] = regs[instr.rs1()] >> regs.shamt(instr.rs2());
			break;

		case Opcode::XOR:
			regs[instr.rd()] = regs[instr.rs1()] ^ regs[instr.rs2()];
			break;

		case Opcode::OR:
			regs[instr.rd()] = regs[instr.rs1()] | regs[instr.rs2()];
			break;

		case Opcode::AND:
			regs[instr.rd()] = regs[instr.rs1()] & regs[instr.rs2()];
			break;

		case Opcode::SLLI:
			regs[instr.rd()] = regs[instr.rs1()] << instr.shamt();
			break;

		case Opcode::SRLI:
			regs[instr.rd()] = ((uxlen_t)regs[instr.rs1()]) >> instr.shamt();
			break;

		case Opcode::SRAI:
			regs[instr.rd()] = regs[instr.rs1()] >> instr.shamt();
			break;

		case Opcode::LUI:
			regs[instr.rd()] = instr.U_imm();
			break;

		case Opcode::AUIPC:
			regs[instr.rd()] = last_pc + instr.U_imm();
			break;

		/* jal with rd == x0/zero */
		case Opcode::J: {
			pc = last_pc + instr.J_imm();
			trap_check_pc_alignment();
		} break;

		/* jal with rd != x0/zero */
		case Opcode::JAL: {
			auto link = pc;
			pc = last_pc + instr.J_imm();
			trap_check_pc_alignment();
			regs[instr.rd()] = link;
		} break;

		/* jalr with rd == x0/zero */
		case Opcode::JR: {
			pc = (regs[instr.rs1()] + instr.I_imm()) & ~1;
			trap_check_pc_alignment();
		} break;

		/* jalr with rd != x0/zero */
		case Opcode::JALR: {
			auto link = pc;
			pc = (regs[instr.rs1()] + instr.I_imm()) & ~1;
			trap_check_pc_alignment();
			regs[instr.rd()] = link;
		} break;

		case Opcode::SB: {
			uxlen_t addr = regs[instr.rs1()] + instr.S_imm();
			lscache.store_byte(addr, regs[instr.rs2()]);
		} break;

		case Opcode::SH: {
			uxlen_t addr = regs[instr.rs1()] + instr.S_imm();
			trap_check_addr_alignment<2, false>(addr);
			lscache.store_half(addr, regs[instr.rs2()]);
		} break;

		case Opcode::SW: {
			uxlen_t addr = regs[instr.rs1()] + instr.S_imm();
			trap_check_addr_alignment<4, false>(addr);
			lscache.store_word(addr, regs[instr.rs2()]);
		} break;

		case Opcode::SD: {
			uxlen_t addr = regs[instr.rs1()] + instr.S_imm();
			trap_check_addr_alignment<8, false>(addr);
			lscache.store_double(addr, regs[instr.rs2()]);
		} break;

		case Opcode::LB: {
			uxlen_t addr = regs[instr.rs1()] + instr.I_imm();
			regs[instr.rd()] = lscache.load_byte(addr);
			regs.reset_zero();
		} break;

		case Opcode::LH: {
			uxlen_t addr = regs[instr.rs1()] + instr.I_imm();
			trap_check_addr_alignment<2, true>(addr);
			regs[instr.rd()] = lscache.load_half(addr);
			regs.reset_zero();
		} break;

		case Opcode::LW: {
			uxlen_t addr = regs[instr.rs1()] + instr.I_imm();
			trap_check_addr_alignment<4, true>(addr);
			regs[instr.rd()] = lscache.load_word(addr);
			regs.reset_zero();
		} break;

		case Opcode::LD: {
			uxlen_t addr = regs[instr.rs1()] + instr.I_imm();
			trap_check_addr_alignment<8, true>(addr);
			regs[instr.rd()] = lscache.load_double(addr);
			regs.reset_zero();
		} break;

		case Opcode::LBU: {
			uxlen_t addr = regs[instr.rs1()] + instr.I_imm();
			regs[instr.rd()] = lscache.load_ubyte(addr);
			regs.reset_zero();
		} break;

		case Opcode::LHU: {
			uxlen_t addr = regs[instr.rs1()] + instr.I_imm();
			trap_check_addr_alignment<2, true>(addr);
			regs[instr.rd()] = lscache.load_uhalf(addr);
			regs.reset_zero();
		} break;

		case Opcode::LWU: {
			uxlen_t addr = regs[instr.rs1()] + instr.I_imm();
			trap_check_addr_alignment<4, true>(addr);
			regs[instr.rd()] = lscache.load_uword(addr);
			regs.reset_zero();
		} break;

		case Opcode::BEQ:
			if (regs[instr.rs1()] == regs[instr.rs2()]) {
				pc = last_pc + instr.B_imm();
				trap_check_pc_alignment();
			}
			break;

		case Opcode::BNE:
			if (regs[instr.rs1()] != regs[instr.rs2()]) {
				pc = last_pc + instr.B_imm();
				trap_check_pc_alignment();
			}
			break;

		case Opcode::BLT:
			if (regs[instr.rs1()] < regs[instr.rs2()]) {
				pc = last_pc + instr.B_imm();
				trap_check_pc_alignment();
			}
			break;

		case Opcode::BGE:
			if (regs[instr.rs1()] >= regs[instr.rs2()]) {
				pc = last_pc + instr.B_imm();
				trap_check_pc_alignment();
			}
			break;

		case Opcode::BLTU:
			if ((uxlen_t)regs[instr.rs1()] < (uxlen_t)regs[instr.rs2()]) {
				pc = last_pc + instr.B_imm();
				trap_check_pc_alignment();
			}
			break;

		case Opcode::BGEU:
			if ((uxlen_t)regs[instr.rs1()] >= (uxlen_t)regs[instr.rs2()]) {
				pc = last_pc + instr.B_imm();
				trap_check_pc_alignment();
			}
			break;

		/* rd != x0/zero variants */
		case Opcode::ADDIW:
			regs[instr.rd()] = (int32_t)regs[instr.rs1()] + (int32_t)instr.I_imm();
			break;

		case Opcode::SLLIW:
			regs[instr.rd()] = (int32_t)((uint32_t)regs[instr.rs1()] << instr.shamt_w());
			break;

		case Opcode::SRLIW:
			regs[instr.rd()] = (int32_t)(((uint32_t)regs[instr.rs1()]) >> instr.shamt_w());
			break;

		case Opcode::SRAIW:
			regs[instr.rd()] = (int32_t)((int32_t)regs[instr.rs1()] >> instr.shamt_w());
			break;

		case Opcode::ADDW:
			regs[instr.rd()] = (int32_t)regs[instr.rs1()] + (int32_t)regs[instr.rs2()];
			break;

		case Opcode::SUBW:
			regs[instr.rd()] = (int32_t)regs[instr.rs1()] - (int32_t)regs[instr.rs2()];
			break;

		case Opcode::SLLW:
			regs[instr.rd()] = (int32_t)((uint32_t)regs[instr.rs1()] << regs.shamt_w(instr.rs2()));
			break;

		case Opcode::SRLW:
			regs[instr.rd()] = (int32_t)(((uint32_t)regs[instr.rs1()]) >> regs.shamt_w(instr.rs2()));
			break;

		case Opcode::SRAW:
			regs[instr.rd()] = (int32_t)((int32_t)regs[instr.rs1()] >> regs.shamt_w(instr.rs2()));
			break;

		case Opcode::FENCE: {
			lscache.fence();
		} break;

		case Opcode::FENCE_I: {
			// not using out of order execution/caches so can be ignored
		} break;

		case Opcode::ECALL: {
			if (sys) {
				sys->execute_syscall(this);
			} else {
				switch (prv) {
					case MachineMode:
						raise_trap(EXC_ECALL_M_MODE, last_pc);
						break;
					case SupervisorMode:
						raise_trap(EXC_ECALL_S_MODE, last_pc);
						break;
					case UserMode:
						raise_trap(EXC_ECALL_U_MODE, last_pc);
						break;
					default:
						throw std::runtime_error("unknown privilege level " + std::to_string(prv));
				}
			}
		} break;

		case Opcode::EBREAK: {
			if (debug_mode) {
				status = CoreExecStatus::HitBreakpoint;
			} else {
				// TODO: also raise trap if we are in debug mode?
				raise_trap(EXC_BREAKPOINT, last_pc);
			}
		} break;

		case Opcode::CSRRW: {
			auto addr = instr.csr();
			if (is_invalid_csr_access(addr, true)) {
				RAISE_ILLEGAL_INSTRUCTION();
			} else {
				auto rd = instr.rd();
				auto rs1_val = regs[instr.rs1()];
				if (rd != RegFile::zero) {
					regs[instr.rd()] = get_csr_value(addr);
				}
				set_csr_value(addr, rs1_val);
			}
		} break;

		case Opcode::CSRRS: {
			auto addr = instr.csr();
			auto rs1 = instr.rs1();
			auto write = rs1 != RegFile::zero;
			if (is_invalid_csr_access(addr, write)) {
				RAISE_ILLEGAL_INSTRUCTION();
			} else {
				auto rd = instr.rd();
				auto rs1_val = regs[rs1];
				auto csr_val = get_csr_value(addr);
				if (rd != RegFile::zero)
					regs[rd] = csr_val;
				if (write)
					set_csr_value(addr, csr_val | rs1_val);
			}
		} break;

		case Opcode::CSRRC: {
			auto addr = instr.csr();
			auto rs1 = instr.rs1();
			auto write = rs1 != RegFile::zero;
			if (is_invalid_csr_access(addr, write)) {
				RAISE_ILLEGAL_INSTRUCTION();
			} else {
				auto rd = instr.rd();
				auto rs1_val = regs[rs1];
				auto csr_val = get_csr_value(addr);
				if (rd != RegFile::zero)
					regs[rd] = csr_val;
				if (write)
					set_csr_value(addr, csr_val & ~rs1_val);
			}
		} break;

		case Opcode::CSRRWI: {
			auto addr = instr.csr();
			if (is_invalid_csr_access(addr, true)) {
				RAISE_ILLEGAL_INSTRUCTION();
			} else {
				auto rd = instr.rd();
				if (rd != RegFile::zero) {
					regs[rd] = get_csr_value(addr);
				}
				set_csr_value(addr, instr.zimm());
			}
		} break;

		case Opcode::CSRRSI: {
			auto addr = instr.csr();
			auto zimm = instr.zimm();
			auto write = zimm != 0;
			if (is_invalid_csr_access(addr, write)) {
				RAISE_ILLEGAL_INSTRUCTION();
			} else {
				auto csr_val = get_csr_value(addr);
				auto rd = instr.rd();
				if (rd != RegFile::zero)
					regs[rd] = csr_val;
				if (write)
					set_csr_value(addr, csr_val | zimm);
			}
		} break;

		case Opcode::CSRRCI: {
			auto addr = instr.csr();
			auto zimm = instr.zimm();
			auto write = zimm != 0;
			if (is_invalid_csr_access(addr, write)) {
				RAISE_ILLEGAL_INSTRUCTION();
			} else {
				auto csr_val = get_csr_value(addr);
				auto rd = instr.rd();
				if (rd != RegFile::zero)
					regs[rd] = csr_val;
				if (write)
					set_csr_value(addr, csr_val & ~zimm);
			}
		} break;

		/* rd != x0/zero variants */
		case Opcode::MUL: {
			int128_t ans = (int128_t)regs[instr.rs1()] * (int128_t)regs[instr.rs2()];
			regs[instr.rd()] = (int64_t)ans;
		} break;

		case Opcode::MULH: {
			int128_t ans = (int128_t)regs[instr.rs1()] * (int128_t)regs[instr.rs2()];
			regs[instr.rd()] = ans >> 64;
		} break;

		case Opcode::MULHU: {
			int128_t ans = ((uint128_t)(uxlen_t)regs[instr.rs1()]) * (uint128_t)((uxlen_t)regs[instr.rs2()]);
			regs[instr.rd()] = ans >> 64;
		} break;

		case Opcode::MULHSU: {
			int128_t ans = (int128_t)regs[instr.rs1()] * (uint128_t)((uxlen_t)regs[instr.rs2()]);
			regs[instr.rd()] = ans >> 64;
		} break;

		case Opcode::DIV: {
			auto a = regs[instr.rs1()];
			auto b = regs[instr.rs2()];
			if (b == 0) {
				regs[instr.rd()] = -1;
			} else if (a == REG_MIN && b == -1) {
				regs[instr.rd()] = a;
			} else {
				regs[instr.rd()] = a / b;
			}
		} break;

		case Opcode::DIVU: {
			auto a = regs[instr.rs1()];
			auto b = regs[instr.rs2()];
			if (b == 0) {
				regs[instr.rd()] = -1;
			} else {
				regs[instr.rd()] = (uxlen_t)a / (uxlen_t)b;
			}
		} break;

		case Opcode::REM: {
			auto a = regs[instr.rs1()];
			auto b = regs[instr.rs2()];
			if (b == 0) {
				regs[instr.rd()] = a;
			} else if (a == REG_MIN && b == -1) {
				regs[instr.rd()] = 0;
			} else {
				regs[instr.rd()] = a % b;
			}
		} break;

		case Opcode::REMU: {
			auto a = regs[instr.rs1()];
			auto b = regs[instr.rs2()];
			if (b == 0) {
				regs[instr.rd()] = a;
			} else {
				regs[instr.rd()] = (uxlen_t)a % (uxlen_t)b;
			}
		} break;

		case Opcode::MULW: {
			regs[instr.rd()] = (int32_t)(regs[instr.rs1()] * regs[instr.rs2()]);
		} break;

		case Opcode::DIVW: {
			int32_t a = regs[instr.rs1()];
			int32_t b = regs[instr.rs2()];
			if (b == 0) {
				regs[instr.rd()] = -1;
			} else if (a == REG32_MIN && b == -1) {
				regs[instr.rd()] = a;
			} else {
				regs[instr.rd()] = a / b;
			}
		} break;

		case Opcode::DIVUW: {
			int32_t a = regs[instr.rs1()];
			int32_t b = regs[instr.rs2()];
			if (b == 0) {
				regs[instr.rd()] = -1;
			} else {
				regs[instr.rd()] = (int32_t)((uint32_t)a / (uint32_t)b);
			}
		} break;

		case Opcode::REMW: {
			int32_t a = regs[instr.rs1()];
			int32_t b = regs[instr.rs2()];
			if (b == 0) {
				regs[instr.rd()] = a;
			} else if (a == REG32_MIN && b == -1) {
				regs[instr.rd()] = 0;
			} else {
				regs[instr.rd()] = a % b;
			}
		} break;

		case Opcode::REMUW: {
			int32_t a = regs[instr.rs1()];
			int32_t b = regs[instr.rs2()];
			if (b == 0) {
				regs[instr.rd()] = a;
			} else {
				regs[instr.rd()] = (int32_t)((uint32_t)a % (uint32_t)b);
			}
		} break;

		case Opcode::LR_W: {
			uxlen_t addr = regs[instr.rs1()];
			trap_check_addr_alignment<4, true>(addr);
			regs[instr.rd()] = mem->atomic_load_reserved_word(addr);
			if (lr_sc_counter == 0)
				lr_sc_counter = 17;  // this instruction + 16 additional ones, (an over-approximation) to cover the
				                     // RISC-V forward progress property
			regs.reset_zero();
		} break;

		case Opcode::SC_W: {
			uxlen_t addr = regs[instr.rs1()];
			trap_check_addr_alignment<4, false>(addr);
			int32_t val = regs[instr.rs2()];
			regs[instr.rd()] = 1;  // failure by default (in case a trap is thrown)
			regs[instr.rd()] =
			    mem->atomic_store_conditional_word(addr, val) ? 0 : 1;  // overwrite result (in case no trap is thrown)
			lr_sc_counter = 0;
			regs.reset_zero();
		} break;

		/*
		 * A Extension
		 * Note: handling of x0/zero is done in execute_amo* implementations (writes to zero/x0 are ignored)
		 */
		case Opcode::AMOSWAP_W: {
			execute_amo_w(instr, [](int32_t a, int32_t b) {
				(void)a;
				return b;
			});
		} break;

		case Opcode::AMOADD_W: {
			execute_amo_w(instr, [](int32_t a, int32_t b) { return a + b; });
		} break;

		case Opcode::AMOXOR_W: {
			execute_amo_w(instr, [](int32_t a, int32_t b) { return a ^ b; });
		} break;

		case Opcode::AMOAND_W: {
			execute_amo_w(instr, [](int32_t a, int32_t b) { return a & b; });
		} break;

		case Opcode::AMOOR_W: {
			execute_amo_w(instr, [](int32_t a, int32_t b) { return a | b; });
		} break;

		case Opcode::AMOMIN_W: {
			execute_amo_w(instr, [](int32_t a, int32_t b) { return std::min(a, b); });
		} break;

		case Opcode::AMOMINU_W: {
			execute_amo_w(instr, [](int32_t a, int32_t b) { return std::min((uint32_t)a, (uint32_t)b); });
		} break;

		case Opcode::AMOMAX_W: {
			execute_amo_w(instr, [](int32_t a, int32_t b) { return std::max(a, b); });
		} break;

		case Opcode::AMOMAXU_W: {
			execute_amo_w(instr, [](int32_t a, int32_t b) { return std::max((uint32_t)a, (uint32_t)b); });
		} break;

		case Opcode::LR_D: {
			uxlen_t addr = regs[instr.rs1()];
			trap_check_addr_alignment<8, true>(addr);
			regs[instr.rd()] = mem->atomic_load_reserved_double(addr);
			if (lr_sc_counter == 0)
				lr_sc_counter = 17;  // this instruction + 16 additional ones, (an over-approximation) to cover the
				                     // RISC-V forward progress property
			regs.reset_zero();
		} break;

		case Opcode::SC_D: {
			uxlen_t addr = regs[instr.rs1()];
			trap_check_addr_alignment<8, false>(addr);
			uint64_t val = regs[instr.rs2()];
			regs[instr.rd()] = 1;  // failure by default (in case a trap is thrown)
			regs[instr.rd()] = mem->atomic_store_conditional_double(addr, val)
			                       ? 0
			                       : 1;  // overwrite result (in case no trap is thrown)
			lr_sc_counter = 0;
			regs.reset_zero();
		} break;

		case Opcode::AMOSWAP_D: {
			execute_amo_d(instr, [](int64_t a, int64_t b) {
				(void)a;
				return b;
			});
		} break;

		case Opcode::AMOADD_D: {
			execute_amo_d(instr, [](int64_t a, int64_t b) { return a + b; });
		} break;

		case Opcode::AMOXOR_D: {
			execute_amo_d(instr, [](int64_t a, int64_t b) { return a ^ b; });
		} break;

		case Opcode::AMOAND_D: {
			execute_amo_d(instr, [](int64_t a, int64_t b) { return a & b; });
		} break;

		case Opcode::AMOOR_D: {
			execute_amo_d(instr, [](int64_t a, int64_t b) { return a | b; });
		} break;

		case Opcode::AMOMIN_D: {
			execute_amo_d(instr, [](int64_t a, int64_t b) { return std::min(a, b); });
		} break;

		case Opcode::AMOMINU_D: {
			execute_amo_d(instr, [](int64_t a, int64_t b) { return std::min((uint64_t)a, (uint64_t)b); });
		} break;

		case Opcode::AMOMAX_D: {
			execute_amo_d(instr, [](int64_t a, int64_t b) { return std::max(a, b); });
		} break;

		case Opcode::AMOMAXU_D: {
			execute_amo_d(instr, [](int64_t a, int64_t b) { return std::max((uint64_t)a, (uint64_t)b); });
		} break;

			// RV64 F/D extension

		case Opcode::FLW: {
			uxlen_t addr = regs[instr.rs1()] + instr.I_imm();
			trap_check_addr_alignment<4, true>(addr);
			fp_regs.write(RD, float32_t{(uint32_t)lscache.load_uword(addr)});
		} break;

		case Opcode::FSW: {
			uxlen_t addr = regs[instr.rs1()] + instr.S_imm();
			trap_check_addr_alignment<4, false>(addr);
			lscache.store_word(addr, fp_regs.u32(RS2));
		} break;

		case Opcode::FADD_S: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, f32_add(fp_regs.f32(RS1), fp_regs.f32(RS2)));
			fp_finish_instr();
		} break;

		case Opcode::FSUB_S: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, f32_sub(fp_regs.f32(RS1), fp_regs.f32(RS2)));
			fp_finish_instr();
		} break;

		case Opcode::FMUL_S: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, f32_mul(fp_regs.f32(RS1), fp_regs.f32(RS2)));
			fp_finish_instr();
		} break;

		case Opcode::FDIV_S: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, f32_div(fp_regs.f32(RS1), fp_regs.f32(RS2)));
			fp_finish_instr();
		} break;

		case Opcode::FSQRT_S: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, f32_sqrt(fp_regs.f32(RS1)));
			fp_finish_instr();
		} break;

		case Opcode::FMIN_S: {
			fp_prepare_instr();

			bool rs1_smaller = f32_lt_quiet(fp_regs.f32(RS1), fp_regs.f32(RS2)) ||
			                   (f32_eq(fp_regs.f32(RS1), fp_regs.f32(RS2)) && f32_isNegative(fp_regs.f32(RS1)));

			if (f32_isNaN(fp_regs.f32(RS1)) && f32_isNaN(fp_regs.f32(RS2))) {
				fp_regs.write(RD, f32_defaultNaN);
			} else {
				if (rs1_smaller)
					fp_regs.write(RD, fp_regs.f32(RS1));
				else
					fp_regs.write(RD, fp_regs.f32(RS2));
			}

			fp_finish_instr();
		} break;

		case Opcode::FMAX_S: {
			fp_prepare_instr();

			bool rs1_greater = f32_lt_quiet(fp_regs.f32(RS2), fp_regs.f32(RS1)) ||
			                   (f32_eq(fp_regs.f32(RS2), fp_regs.f32(RS1)) && f32_isNegative(fp_regs.f32(RS2)));

			if (f32_isNaN(fp_regs.f32(RS1)) && f32_isNaN(fp_regs.f32(RS2))) {
				fp_regs.write(RD, f32_defaultNaN);
			} else {
				if (rs1_greater)
					fp_regs.write(RD, fp_regs.f32(RS1));
				else
					fp_regs.write(RD, fp_regs.f32(RS2));
			}

			fp_finish_instr();
		} break;

		case Opcode::FMADD_S: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, f32_mulAdd(fp_regs.f32(RS1), fp_regs.f32(RS2), fp_regs.f32(RS3)));
			fp_finish_instr();
		} break;

		case Opcode::FMSUB_S: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, f32_mulAdd(fp_regs.f32(RS1), fp_regs.f32(RS2), f32_neg(fp_regs.f32(RS3))));
			fp_finish_instr();
		} break;

		case Opcode::FNMADD_S: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, f32_mulAdd(f32_neg(fp_regs.f32(RS1)), fp_regs.f32(RS2), f32_neg(fp_regs.f32(RS3))));
			fp_finish_instr();
		} break;

		case Opcode::FNMSUB_S: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, f32_mulAdd(f32_neg(fp_regs.f32(RS1)), fp_regs.f32(RS2), fp_regs.f32(RS3)));
			fp_finish_instr();
		} break;

		case Opcode::FCVT_W_S: {
			fp_prepare_instr();
			fp_setup_rm();
			regs[RD] = f32_to_i32(fp_regs.f32(RS1), softfloat_roundingMode, true);
			fp_finish_instr();
			regs.reset_zero();
		} break;

		case Opcode::FCVT_WU_S: {
			fp_prepare_instr();
			fp_setup_rm();
			regs[RD] = (int32_t)f32_to_ui32(fp_regs.f32(RS1), softfloat_roundingMode, true);
			fp_finish_instr();
			regs.reset_zero();
		} break;

		case Opcode::FCVT_S_W: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, i32_to_f32((int32_t)regs[RS1]));
			fp_finish_instr();
		} break;

		case Opcode::FCVT_S_WU: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, ui32_to_f32((int32_t)regs[RS1]));
			fp_finish_instr();
		} break;

		case Opcode::FSGNJ_S: {
			fp_prepare_instr();
			auto f1 = fp_regs.f32(RS1);
			auto f2 = fp_regs.f32(RS2);
			fp_regs.write(RD, float32_t{(f1.v & ~F32_SIGN_BIT) | (f2.v & F32_SIGN_BIT)});
			fp_set_dirty();
		} break;

		case Opcode::FSGNJN_S: {
			fp_prepare_instr();
			auto f1 = fp_regs.f32(RS1);
			auto f2 = fp_regs.f32(RS2);
			fp_regs.write(RD, float32_t{(f1.v & ~F32_SIGN_BIT) | (~f2.v & F32_SIGN_BIT)});
			fp_set_dirty();
		} break;

		case Opcode::FSGNJX_S: {
			fp_prepare_instr();
			auto f1 = fp_regs.f32(RS1);
			auto f2 = fp_regs.f32(RS2);
			fp_regs.write(RD, float32_t{f1.v ^ (f2.v & F32_SIGN_BIT)});
			fp_set_dirty();
		} break;

		case Opcode::FMV_W_X: {
			fp_prepare_instr();
			fp_regs.write(RD, float32_t{(uint32_t)((int32_t)regs[RS1])});
			fp_set_dirty();
		} break;

		case Opcode::FMV_X_W: {
			fp_prepare_instr();
			regs[RD] = (int32_t)fp_regs.u32(RS1);
			regs.reset_zero();
		} break;

		case Opcode::FEQ_S: {
			fp_prepare_instr();
			regs[RD] = f32_eq(fp_regs.f32(RS1), fp_regs.f32(RS2));
			fp_update_exception_flags();
			regs.reset_zero();
		} break;

		case Opcode::FLT_S: {
			fp_prepare_instr();
			regs[RD] = f32_lt(fp_regs.f32(RS1), fp_regs.f32(RS2));
			fp_update_exception_flags();
			regs.reset_zero();
		} break;

		case Opcode::FLE_S: {
			fp_prepare_instr();
			regs[RD] = f32_le(fp_regs.f32(RS1), fp_regs.f32(RS2));
			fp_update_exception_flags();
			regs.reset_zero();
		} break;

		case Opcode::FCLASS_S: {
			fp_prepare_instr();
			regs[RD] = (int32_t)f32_classify(fp_regs.f32(RS1));
			regs.reset_zero();
		} break;

		case Opcode::FCVT_L_S: {
			fp_prepare_instr();
			fp_setup_rm();
			regs[RD] = f32_to_i64(fp_regs.f32(RS1), softfloat_roundingMode, true);
			fp_finish_instr();
			regs.reset_zero();
		} break;

		case Opcode::FCVT_LU_S: {
			fp_prepare_instr();
			fp_setup_rm();
			regs[RD] = f32_to_ui64(fp_regs.f32(RS1), softfloat_roundingMode, true);
			fp_finish_instr();
			regs.reset_zero();
		} break;

		case Opcode::FCVT_S_L: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, i64_to_f32(regs[RS1]));
			fp_finish_instr();
		} break;

		case Opcode::FCVT_S_LU: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, ui64_to_f32(regs[RS1]));
			fp_finish_instr();
		} break;

			// RV32D Extension

		case Opcode::FLD: {
			uxlen_t addr = regs[instr.rs1()] + instr.I_imm();
			trap_check_addr_alignment<8, true>(addr);
			fp_regs.write(RD, float64_t{(uint64_t)lscache.load_double(addr)});
		} break;

		case Opcode::FSD: {
			uxlen_t addr = regs[instr.rs1()] + instr.S_imm();
			trap_check_addr_alignment<8, false>(addr);
			lscache.store_double(addr, fp_regs.f64(RS2).v);
		} break;

		case Opcode::FADD_D: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, f64_add(fp_regs.f64(RS1), fp_regs.f64(RS2)));
			fp_finish_instr();
		} break;

		case Opcode::FSUB_D: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, f64_sub(fp_regs.f64(RS1), fp_regs.f64(RS2)));
			fp_finish_instr();
		} break;

		case Opcode::FMUL_D: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, f64_mul(fp_regs.f64(RS1), fp_regs.f64(RS2)));
			fp_finish_instr();
		} break;

		case Opcode::FDIV_D: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, f64_div(fp_regs.f64(RS1), fp_regs.f64(RS2)));
			fp_finish_instr();
		} break;

		case Opcode::FSQRT_D: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, f64_sqrt(fp_regs.f64(RS1)));
			fp_finish_instr();
		} break;

		case Opcode::FMIN_D: {
			fp_prepare_instr();

			bool rs1_smaller = f64_lt_quiet(fp_regs.f64(RS1), fp_regs.f64(RS2)) ||
			                   (f64_eq(fp_regs.f64(RS1), fp_regs.f64(RS2)) && f64_isNegative(fp_regs.f64(RS1)));

			if (f64_isNaN(fp_regs.f64(RS1)) && f64_isNaN(fp_regs.f64(RS2))) {
				fp_regs.write(RD, f64_defaultNaN);
			} else {
				if (rs1_smaller)
					fp_regs.write(RD, fp_regs.f64(RS1));
				else
					fp_regs.write(RD, fp_regs.f64(RS2));
			}

			fp_finish_instr();
		} break;

		case Opcode::FMAX_D: {
			fp_prepare_instr();

			bool rs1_greater = f64_lt_quiet(fp_regs.f64(RS2), fp_regs.f64(RS1)) ||
			                   (f64_eq(fp_regs.f64(RS2), fp_regs.f64(RS1)) && f64_isNegative(fp_regs.f64(RS2)));

			if (f64_isNaN(fp_regs.f64(RS1)) && f64_isNaN(fp_regs.f64(RS2))) {
				fp_regs.write(RD, f64_defaultNaN);
			} else {
				if (rs1_greater)
					fp_regs.write(RD, fp_regs.f64(RS1));
				else
					fp_regs.write(RD, fp_regs.f64(RS2));
			}

			fp_finish_instr();
		} break;

		case Opcode::FMADD_D: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, f64_mulAdd(fp_regs.f64(RS1), fp_regs.f64(RS2), fp_regs.f64(RS3)));
			fp_finish_instr();
		} break;

		case Opcode::FMSUB_D: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, f64_mulAdd(fp_regs.f64(RS1), fp_regs.f64(RS2), f64_neg(fp_regs.f64(RS3))));
			fp_finish_instr();
		} break;

		case Opcode::FNMADD_D: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, f64_mulAdd(f64_neg(fp_regs.f64(RS1)), fp_regs.f64(RS2), f64_neg(fp_regs.f64(RS3))));
			fp_finish_instr();
		} break;

		case Opcode::FNMSUB_D: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, f64_mulAdd(f64_neg(fp_regs.f64(RS1)), fp_regs.f64(RS2), fp_regs.f64(RS3)));
			fp_finish_instr();
		} break;

		case Opcode::FSGNJ_D: {
			fp_prepare_instr();
			auto f1 = fp_regs.f64(RS1);
			auto f2 = fp_regs.f64(RS2);
			fp_regs.write(RD, float64_t{(f1.v & ~F64_SIGN_BIT) | (f2.v & F64_SIGN_BIT)});
			fp_set_dirty();
		} break;

		case Opcode::FSGNJN_D: {
			fp_prepare_instr();
			auto f1 = fp_regs.f64(RS1);
			auto f2 = fp_regs.f64(RS2);
			fp_regs.write(RD, float64_t{(f1.v & ~F64_SIGN_BIT) | (~f2.v & F64_SIGN_BIT)});
			fp_set_dirty();
		} break;

		case Opcode::FSGNJX_D: {
			fp_prepare_instr();
			auto f1 = fp_regs.f64(RS1);
			auto f2 = fp_regs.f64(RS2);
			fp_regs.write(RD, float64_t{f1.v ^ (f2.v & F64_SIGN_BIT)});
			fp_set_dirty();
		} break;

		case Opcode::FEQ_D: {
			fp_prepare_instr();
			regs[RD] = f64_eq(fp_regs.f64(RS1), fp_regs.f64(RS2));
			fp_update_exception_flags();
			regs.reset_zero();
		} break;

		case Opcode::FLT_D: {
			fp_prepare_instr();
			regs[RD] = f64_lt(fp_regs.f64(RS1), fp_regs.f64(RS2));
			fp_update_exception_flags();
			regs.reset_zero();
		} break;

		case Opcode::FLE_D: {
			fp_prepare_instr();
			regs[RD] = f64_le(fp_regs.f64(RS1), fp_regs.f64(RS2));
			fp_update_exception_flags();
			regs.reset_zero();
		} break;

		case Opcode::FCLASS_D: {
			fp_prepare_instr();
			regs[RD] = (int64_t)f64_classify(fp_regs.f64(RS1));
			regs.reset_zero();
		} break;

		case Opcode::FMV_D_X: {
			fp_prepare_instr();
			fp_regs.write(RD, float64_t{(uint64_t)regs[RS1]});
			fp_set_dirty();
		} break;

		case Opcode::FMV_X_D: {
			fp_prepare_instr();
			regs[RD] = fp_regs.f64(RS1).v;
			regs.reset_zero();
		} break;

		case Opcode::FCVT_W_D: {
			fp_prepare_instr();
			fp_setup_rm();
			regs[RD] = f64_to_i32(fp_regs.f64(RS1), softfloat_roundingMode, true);
			fp_finish_instr();
			regs.reset_zero();
		} break;

		case Opcode::FCVT_WU_D: {
			fp_prepare_instr();
			fp_setup_rm();
			regs[RD] = (int32_t)f64_to_ui32(fp_regs.f64(RS1), softfloat_roundingMode, true);
			fp_finish_instr();
			regs.reset_zero();
		} break;

		case Opcode::FCVT_D_W: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, i32_to_f64((int32_t)regs[RS1]));
			fp_finish_instr();
		} break;

		case Opcode::FCVT_D_WU: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, ui32_to_f64((int32_t)regs[RS1]));
			fp_finish_instr();
		} break;

		case Opcode::FCVT_S_D: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, f64_to_f32(fp_regs.f64(RS1)));
			fp_finish_instr();
		} break;

		case Opcode::FCVT_D_S: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, f32_to_f64(fp_regs.f32(RS1)));
			fp_finish_instr();
		} break;

		case Opcode::FCVT_L_D: {
			fp_prepare_instr();
			fp_setup_rm();
			regs[RD] = f64_to_i64(fp_regs.f64(RS1), softfloat_roundingMode, true);
			fp_finish_instr();
			regs.reset_zero();
		} break;

		case Opcode::FCVT_LU_D: {
			fp_prepare_instr();
			fp_setup_rm();
			regs[RD] = f64_to_ui64(fp_regs.f64(RS1), softfloat_roundingMode, true);
			fp_finish_instr();
			regs.reset_zero();
		} break;

		case Opcode::FCVT_D_L: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, i64_to_f64(regs[RS1]));
			fp_finish_instr();
		} break;

		case Opcode::FCVT_D_LU: {
			fp_prepare_instr();
			fp_setup_rm();
			fp_regs.write(RD, ui64_to_f64(regs[RS1]));
			fp_finish_instr();
		} break;

		/*
		 * RV-V Extension
		 * Note: handling of x0/zero is done in v_ext implementation (writes to zero/x0 are ignored)
		 */
		case Opcode::VSETVLI: {
			v_ext.prepInstr(true, false, false);
			v_ext.v_set_operation(instr.rd(), instr.rs1(), instr.zimm_10(), 0);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSETIVLI: {
			v_ext.prepInstr(true, false, false);
			v_ext.v_set_operation(instr.rd(), 0, instr.zimm_9(), instr.rs1());
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSETVL: {
			v_ext.prepInstr(true, false, false);
			v_ext.v_set_operation(instr.rd(), instr.rs1(), regs[instr.rs2()], 0);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLM_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::masked);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSM_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::masked);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLE8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLE16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLE32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLE64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSE8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSE16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSE32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSE64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSE8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSE16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSE32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSE64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSE8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSE16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSE32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSE64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXEI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXEI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXEI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXEI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXEI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXEI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXEI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXEI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXEI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXEI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXEI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXEI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXEI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXEI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXEI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXEI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLE8FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLE16FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLE32FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLE64FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG2E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG2E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG2E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG2E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG2E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG2E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG2E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG2E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG2E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG2E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG2E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG2E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG2E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG2E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG2E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG2E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG2EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG2EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG2EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG2EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG2EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG2EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG2EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG2EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG2EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG2EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG2EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG2EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG2EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG2EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG2EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG2EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG2E8FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG2E16FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG2E32FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG2E64FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG3E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG3E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG3E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG3E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG3E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG3E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG3E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG3E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG3E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG3E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG3E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG3E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG3E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG3E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG3E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG3E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG3EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG3EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG3EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG3EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG3EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG3EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG3EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG3EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG3EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG3EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG3EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG3EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG3EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG3EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG3EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG3EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG3E8FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG3E16FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG3E32FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG3E64FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG4E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG4E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG4E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG4E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG4E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG4E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG4E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG4E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG4E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG4E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG4E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG4E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG4E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG4E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG4E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG4E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG4EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG4EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG4EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG4EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG4EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG4EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG4EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG4EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG4EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG4EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG4EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG4EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG4EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG4EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG4EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG4EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG4E8FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG4E16FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG4E32FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG4E64FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG5E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG5E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG5E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG5E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG5E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG5E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG5E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG5E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG5E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG5E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG5E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG5E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG5E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG5E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG5E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG5E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG5EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG5EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG5EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG5EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG5EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG5EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG5EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG5EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG5EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG5EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG5EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG5EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG5EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG5EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG5EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG5EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG5E8FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG5E16FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG5E32FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG5E64FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG6E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG6E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG6E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG6E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG6E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG6E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG6E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG6E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG6E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG6E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG6E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG6E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG6E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG6E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG6E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG6E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG6EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG6EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG6EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG6EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG6EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG6EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG6EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG6EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG6EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG6EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG6EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG6EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG6EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG6EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG6EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG6EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG6E8FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG6E16FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG6E32FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG6E64FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG7E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG7E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG7E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG7E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG7E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG7E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG7E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG7E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG7E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG7E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG7E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG7E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG7E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG7E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG7E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG7E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG7EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG7EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG7EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG7EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG7EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG7EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG7EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG7EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG7EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG7EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG7EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG7EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG7EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG7EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG7EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG7EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG7E8FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG7E16FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG7E32FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG7E64FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG8E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG8E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG8E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG8E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG8E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG8E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG8E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSEG8E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG8E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG8E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG8E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSSEG8E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG8E8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG8E16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG8E32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSSEG8E64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard_reg);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG8EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG8EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG8EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLUXSEG8EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG8EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG8EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG8EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLOXSEG8EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG8EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG8EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG8EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUXSEG8EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG8EI8_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG8EI16_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG8EI32_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSOXSEG8EI64_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG8E8FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG8E16FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG8E32FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VLSEG8E64FF_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::fofl);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VL1RE8_V: {
			v_ext.prepInstr(true, false, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::whole);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VL1RE16_V: {
			v_ext.prepInstr(true, false, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::whole);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VL1RE32_V: {
			v_ext.prepInstr(true, false, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::whole);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VL1RE64_V: {
			v_ext.prepInstr(true, false, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::whole);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VS1R_V: {
			v_ext.prepInstr(true, false, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::whole);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VL2RE8_V: {
			v_ext.prepInstr(true, false, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::whole);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VL2RE16_V: {
			v_ext.prepInstr(true, false, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::whole);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VL2RE32_V: {
			v_ext.prepInstr(true, false, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::whole);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VL2RE64_V: {
			v_ext.prepInstr(true, false, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::whole);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VS2R_V: {
			v_ext.prepInstr(true, false, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::whole);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VL4RE8_V: {
			v_ext.prepInstr(true, false, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::whole);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VL4RE16_V: {
			v_ext.prepInstr(true, false, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::whole);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VL4RE32_V: {
			v_ext.prepInstr(true, false, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::whole);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VL4RE64_V: {
			v_ext.prepInstr(true, false, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::whole);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VS4R_V: {
			v_ext.prepInstr(true, false, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::whole);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VL8RE8_V: {
			v_ext.prepInstr(true, false, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::whole);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VL8RE16_V: {
			v_ext.prepInstr(true, false, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::whole);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VL8RE32_V: {
			v_ext.prepInstr(true, false, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::whole);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VL8RE64_V: {
			v_ext.prepInstr(true, false, false);
			v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::whole);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VS8R_V: {
			v_ext.prepInstr(true, false, false);
			v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::whole);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VADD_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VADD_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VADD_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUB_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSub(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSUB_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSub(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VRSUB_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vRSub(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VRSUB_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vRSub(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWADD_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWADD_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWSUB_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSub(), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWSUB_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSub(), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWADDU_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWADDU_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWSUBU_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSub(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWSUBU_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSub(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWADD_WV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::wwxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWADD_WX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::wwxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWSUB_WV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSub(), VExt::elem_sel_t::wwxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWSUB_WX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSub(), VExt::elem_sel_t::wwxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWADDU_WV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::wwxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWADDU_WX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::wwxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWSUBU_WV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSub(), VExt::elem_sel_t::wwxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWSUBU_WX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSub(), VExt::elem_sel_t::wwxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VZEXT_VF2: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vExt(2), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSEXT_VF2: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vExt(2), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VZEXT_VF4: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vExt(4), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSEXT_VF4: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vExt(4), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VZEXT_VF8: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vExt(8), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSEXT_VF8: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vExt(8), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VADC_VVM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopExtCarry(v_ext.vAdc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VADC_VXM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopExtCarry(v_ext.vAdc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VADC_VIM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopExtCarry(v_ext.vAdc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMADC_VVM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidAll(v_ext.vMadc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMADC_VXM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidAll(v_ext.vMadc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMADC_VIM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidAll(v_ext.vMadc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMADC_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidAll(v_ext.vMadc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMADC_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidAll(v_ext.vMadc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMADC_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidAll(v_ext.vMadc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSBC_VVM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopExtCarry(v_ext.vSbc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSBC_VXM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopExtCarry(v_ext.vSbc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSBC_VVM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidAll(v_ext.vMsbc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSBC_VXM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidAll(v_ext.vMsbc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSBC_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidAll(v_ext.vMsbc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSBC_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidAll(v_ext.vMsbc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VAND_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAnd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VAND_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAnd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VAND_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAnd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VOR_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vOr(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VOR_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vOr(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VOR_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vOr(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VXOR_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vXor(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VXOR_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vXor(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VXOR_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vXor(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSLL_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShift(false), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSLL_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShift(false), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSLL_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShift(false), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSRL_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSRL_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSRL_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSRA_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xxxssu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSRA_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xxxssu, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSRA_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xxxssu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VNSRL_WV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VNSRL_WI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VNSRL_WX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VNSRA_WV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xwxssu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VNSRA_WI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xwxssu, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VNSRA_WX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xwxssu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSEQ_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::eq), VExt::elem_sel_t::xxxsss,
			                     VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSEQ_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::eq), VExt::elem_sel_t::xxxsss,
			                     VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSEQ_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::eq), VExt::elem_sel_t::xxxsss,
			                     VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSNE_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::ne), VExt::elem_sel_t::xxxsss,
			                     VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSNE_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::ne), VExt::elem_sel_t::xxxsss,
			                     VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSNE_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::ne), VExt::elem_sel_t::xxxsss,
			                     VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSLTU_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::lt), VExt::elem_sel_t::xxxuuu,
			                     VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSLTU_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::lt), VExt::elem_sel_t::xxxuuu,
			                     VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSLT_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::lt), VExt::elem_sel_t::xxxsss,
			                     VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSLT_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::lt), VExt::elem_sel_t::xxxsss,
			                     VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSLEU_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::le), VExt::elem_sel_t::xxxuuu,
			                     VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSLEU_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::le), VExt::elem_sel_t::xxxuuu,
			                     VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSLEU_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::le), VExt::elem_sel_t::xxxuus,
			                     VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSLE_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::le), VExt::elem_sel_t::xxxsss,
			                     VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSLE_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::le), VExt::elem_sel_t::xxxsss,
			                     VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSLE_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::le), VExt::elem_sel_t::xxxsss,
			                     VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSGTU_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::gt), VExt::elem_sel_t::xxxuuu,
			                     VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSGTU_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::gt), VExt::elem_sel_t::xxxuus,
			                     VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSGT_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::gt), VExt::elem_sel_t::xxxsss,
			                     VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSGT_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::gt), VExt::elem_sel_t::xxxsss,
			                     VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMINU_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMin(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMINU_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMin(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMIN_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMin(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMIN_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMin(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMAXU_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMax(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMAXU_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMax(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMAX_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMax(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMAX_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMax(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMUL_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMul(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMUL_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMul(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMULH_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMulh(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMULH_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMulh(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMULHU_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMulh(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMULHU_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMulh(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMULHSU_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMulh(), VExt::elem_sel_t::xxxssu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMULHSU_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMulh(), VExt::elem_sel_t::xxxssu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VDIVU_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vDiv(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VDIVU_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vDiv(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VDIV_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vDiv(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VDIV_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vDiv(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VREMU_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vRem(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VREMU_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vRem(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VREM_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vRem(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VREM_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vRem(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWMUL_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMul(), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWMUL_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMul(), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWMULU_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMul(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWMULU_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMul(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWMULSU_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMul(), VExt::elem_sel_t::wxxusu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWMULSU_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMul(), VExt::elem_sel_t::wxxusu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMACC_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExt(v_ext.vMacc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMACC_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExt(v_ext.vMacc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VNMSAC_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExt(v_ext.vNmsac(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VNMSAC_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExt(v_ext.vNmsac(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMADD_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExt(v_ext.vMadd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMADD_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExt(v_ext.vMadd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VNMSUB_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExt(v_ext.vNmsub(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VNMSUB_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExt(v_ext.vNmsub(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWMACCU_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExt(v_ext.vMacc(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWMACCU_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExt(v_ext.vMacc(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWMACC_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExt(v_ext.vMacc(), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWMACC_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExt(v_ext.vMacc(), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWMACCSU_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExt(v_ext.vMacc(), VExt::elem_sel_t::wxxuus, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWMACCSU_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExt(v_ext.vMacc(), VExt::elem_sel_t::wxxuus, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWMACCUS_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVdExt(v_ext.vMacc(), VExt::elem_sel_t::wxxusu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMERGE_VVM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopExtCarry(v_ext.vMerge(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMERGE_VXM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopExtCarry(v_ext.vMerge(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMERGE_VIM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopExtCarry(v_ext.vMerge(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMV_V_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMv(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMV_V_X: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMv(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMV_V_I: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vMv(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSADDU_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSaddu(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSADDU_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSaddu(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSADDU_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSaddu(), VExt::elem_sel_t::xxxuus, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSADD_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSadd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSADD_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSadd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSADD_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSadd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSUBU_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSsubu(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSUBU_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSsubu(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSUB_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSsub(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSUB_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSsub(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VAADDU_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAadd(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VAADDU_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAadd(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VAADD_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAadd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VAADD_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAadd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VASUBU_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAsub(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VASUBU_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAsub(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VASUB_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAsub(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VASUB_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vAsub(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSMUL_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSmul(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSMUL_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vSmul(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSRL_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShiftRight(false), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSRL_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShiftRight(false), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSRL_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShiftRight(false), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSRA_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShiftRight(false), VExt::elem_sel_t::xxxssu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSRA_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShiftRight(false), VExt::elem_sel_t::xxxssu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSSRA_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShiftRight(false), VExt::elem_sel_t::xxxssu, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VNCLIPU_WV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShiftRight(true), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VNCLIPU_WX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShiftRight(true), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VNCLIPU_WI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShiftRight(true), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VNCLIP_WV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShiftRight(true), VExt::elem_sel_t::xwxssu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VNCLIP_WX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShiftRight(true), VExt::elem_sel_t::xwxssu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VNCLIP_WI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoop(v_ext.vShiftRight(true), VExt::elem_sel_t::xwxssu, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VFADD_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfAdd(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFADD_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfAdd(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFSUB_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfSub(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFSUB_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfSub(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFRSUB_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfrSub(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWADD_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfwAdd(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWADD_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfwAdd(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWSUB_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfwSub(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWSUB_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfwSub(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWADD_WV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfwAddw(), VExt::elem_sel_t::wwxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWADD_WF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfwAddw(), VExt::elem_sel_t::wwxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWSUB_WV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfwSubw(), VExt::elem_sel_t::wwxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWSUB_WF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfwSubw(), VExt::elem_sel_t::wwxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFMUL_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfMul(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFMUL_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfMul(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFDIV_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfDiv(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFDIV_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfDiv(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFRDIV_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfrDiv(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWMUL_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfwMul(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWMUL_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfwMul(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFMACC_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfMacc(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFMACC_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfMacc(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFNMACC_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfNmacc(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFNMACC_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfNmacc(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFMSAC_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfMsac(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFMSAC_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfMsac(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFNMSAC_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfNmsac(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFNMSAC_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfNmsac(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFMADD_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfMadd(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFMADD_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfMadd(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFNMADD_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfNmadd(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFNMADD_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfNmadd(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFMSUB_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfMsub(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFMSUB_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfMsub(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFNMSUB_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfNmsub(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFNMSUB_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfNmsub(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWMACC_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfwMacc(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWMACC_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfwMacc(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWNMACC_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfwNmacc(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWNMACC_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfwNmacc(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWMSAC_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfwMsac(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWMSAC_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfwMsac(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWNMSAC_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfwNmsac(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWNMSAC_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfwNmsac(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFSQRT_V: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfSqrt(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFRSQRT7_V: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfRsqrt7(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFREC7_V: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfFrec7(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFMIN_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfMin(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFMIN_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfMin(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFMAX_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfMax(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFMAX_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfMax(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFSGNJ_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfSgnj(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFSGNJ_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfSgnj(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFSGNJN_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfSgnjn(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFSGNJN_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfSgnjn(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFSGNJX_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfSgnjx(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFSGNJX_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfSgnjx(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VMFEQ_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExtVoid(v_ext.vMfeq(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VMFEQ_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExtVoid(v_ext.vMfeq(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VMFNE_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExtVoid(v_ext.vMfneq(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VMFNE_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExtVoid(v_ext.vMfneq(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VMFLT_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExtVoid(v_ext.vMflt(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VMFLT_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExtVoid(v_ext.vMflt(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VMFLE_VV: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExtVoid(v_ext.vMfle(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VMFLE_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExtVoid(v_ext.vMfle(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VMFGT_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExtVoid(v_ext.vMfgt(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VMFGE_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExtVoid(v_ext.vMfge(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFCLASS_V: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfClass(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFMERGE_VFM: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopExtCarry(v_ext.vMerge(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFMV_V_F: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoop(v_ext.vfMv(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFCVT_XU_F_V: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfCvtXF(false), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFCVT_X_F_V: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfCvtXF(false), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFCVT_RTZ_XU_F_V: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfCvtXF(true), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFCVT_RTZ_X_F_V: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfCvtXF(true), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFCVT_F_XU_V: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoop(v_ext.vfCvtFX(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFCVT_F_X_V: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoop(v_ext.vfCvtFX(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWCVT_XU_F_V: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfCvtwXF(false), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWCVT_X_F_V: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfCvtwXF(false), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWCVT_RTZ_XU_F_V: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfCvtwXF(true), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWCVT_RTZ_X_F_V: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfCvtwXF(true), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWCVT_F_XU_V: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoop(v_ext.vfCvtFX(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWCVT_F_X_V: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoop(v_ext.vfCvtFX(), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWCVT_F_F_V: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfCvtwFF(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFNCVT_XU_F_W: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfCvtnXF(false), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFNCVT_X_F_W: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfCvtnXF(false), VExt::elem_sel_t::xwxsss, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFNCVT_RTZ_XU_F_W: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfCvtnXF(true), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFNCVT_RTZ_X_F_W: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfCvtnXF(true), VExt::elem_sel_t::xwxsss, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFNCVT_F_XU_W: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoop(v_ext.vfCvtFX(), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFNCVT_F_X_W: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoop(v_ext.vfCvtFX(), VExt::elem_sel_t::xwxsss, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFNCVT_F_F_W: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfCvtnFF(false), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFNCVT_ROD_F_F_W: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVdExt(v_ext.vfCvtnFF(true), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VREDSUM_VS: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopRed(v_ext.vRedSum(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VREDMAXU_VS: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopRed(v_ext.vRedMax(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VREDMAX_VS: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopRed(v_ext.vRedMax(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VREDMINU_VS: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopRed(v_ext.vRedMin(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VREDMIN_VS: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopRed(v_ext.vRedMin(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VREDAND_VS: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopRed(v_ext.vRedAnd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VREDOR_VS: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopRed(v_ext.vRedOr(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VREDXOR_VS: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopRed(v_ext.vRedXor(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWREDSUMU_VS: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopRed(v_ext.vRedSum(), VExt::elem_sel_t::wxwuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VWREDSUM_VS: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopRed(v_ext.vRedSum(), VExt::elem_sel_t::wxwsss, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VFREDUSUM_VS: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopRed(v_ext.vfRedSum(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFREDOSUM_VS: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopRed(v_ext.vfRedSum(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFREDMAX_VS: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopRed(v_ext.vfRedMax(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFREDMIN_VS: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopRed(v_ext.vfRedMin(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWREDUSUM_VS: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopRed(v_ext.vfwRedSum(), VExt::elem_sel_t::wxwuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFWREDOSUM_VS: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopRed(v_ext.vfwRedSum(), VExt::elem_sel_t::wxwuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VMAND_MM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidAllMask(v_ext.vMask(VExt::maskOperation::m_and));
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMNAND_MM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidAllMask(v_ext.vMask(VExt::maskOperation::m_nand));
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMANDN_MM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidAllMask(v_ext.vMask(VExt::maskOperation::m_andn));
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMXOR_MM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidAllMask(v_ext.vMask(VExt::maskOperation::m_xor));
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMOR_MM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidAllMask(v_ext.vMask(VExt::maskOperation::m_or));
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMNOR_MM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidAllMask(v_ext.vMask(VExt::maskOperation::m_nor));
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMORN_MM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidAllMask(v_ext.vMask(VExt::maskOperation::m_orn));
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMXNOR_MM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidAllMask(v_ext.vMask(VExt::maskOperation::m_xnor));
			v_ext.finishInstr(false);
		} break;
		case Opcode::VCPOP_M: {
			v_ext.prepInstr(true, true, false);
			v_ext.vCpop();
			v_ext.finishInstr(false);
		} break;
		case Opcode::VFIRST_M: {
			v_ext.prepInstr(true, true, false);
			v_ext.vFirst();
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSBF_M: {
			v_ext.prepInstr(true, true, false);
			v_ext.vMs(VExt::vms_type_t::sbf);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSIF_M: {
			v_ext.prepInstr(true, true, false);
			v_ext.vMs(VExt::vms_type_t::sif);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMSOF_M: {
			v_ext.prepInstr(true, true, false);
			v_ext.vMs(VExt::vms_type_t::sof);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VIOTA_M: {
			v_ext.prepInstr(true, true, false);
			v_ext.vIota();
			v_ext.finishInstr(false);
		} break;
		case Opcode::VID_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoid(v_ext.vId(), VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMV_X_S: {
			v_ext.prepInstr(true, true, false);
			v_ext.vMvXs();
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMV_S_X: {
			v_ext.prepInstr(true, true, false);
			v_ext.vMvSx();
			v_ext.finishInstr(false);
		} break;
		case Opcode::VFMV_F_S: {
			v_ext.prepInstr(true, true, true);
			v_ext.vMvFs();
			v_ext.finishInstr(true);
		} break;
		case Opcode::VFMV_S_F: {
			v_ext.prepInstr(true, true, true);
			v_ext.vMvSf();
			v_ext.finishInstr(true);
		} break;
		case Opcode::VSLIDEUP_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidNoOverlap(v_ext.vSlideUp(regs[instr.rs1()]), VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSLIDEUP_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidNoOverlap(v_ext.vSlideUp(instr.rs1()), VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSLIDEDOWN_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoid(v_ext.vSlideDown(regs[instr.rs1()]), VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSLIDEDOWN_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoid(v_ext.vSlideDown(instr.rs1()), VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VSLIDE1UP_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoidNoOverlap(v_ext.vSlide1Up(VExt::param_sel_t::vx), VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VFSLIDE1UP_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVoidNoOverlap(v_ext.vSlide1Up(VExt::param_sel_t::vf), VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VSLIDE1DOWN_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopVoid(v_ext.vSlide1Down(VExt::param_sel_t::vx), VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VFSLIDE1DOWN_VF: {
			v_ext.prepInstr(true, true, true);
			v_ext.vLoopVoid(v_ext.vSlide1Down(VExt::param_sel_t::vf), VExt::param_sel_t::vf);
			v_ext.finishInstr(true);
		} break;
		case Opcode::VRGATHER_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopExt(v_ext.vGather(false), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VRGATHEREI16_VV: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopExt(v_ext.vGather(true), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VRGATHER_VX: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopExt(v_ext.vGather(false), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VRGATHER_VI: {
			v_ext.prepInstr(true, true, false);
			v_ext.vLoopExt(v_ext.vGather(false), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vi);
			v_ext.finishInstr(false);
		} break;
		case Opcode::VCOMPRESS_VM: {
			v_ext.prepInstr(true, true, false);
			v_ext.vCompress();
			v_ext.finishInstr(false);
		} break;
		case Opcode::VMV_NR_R_V: {
			v_ext.prepInstr(true, true, false);
			v_ext.vMvNr();
			v_ext.finishInstr(false);
		} break;
			// RV-V Extension End -- Placeholder 6
			// privileged instructions

		case Opcode::WFI:
			// NOTE: only a hint, can be implemented as NOP
			// std::cout << "[sim:wfi] CSR mstatus.mie " << csrs.mstatus->mie << std::endl;
			release_lr_sc_reservation();

			if (s_mode() && csrs.mstatus.fields.tw)
				RAISE_ILLEGAL_INSTRUCTION();

			if (u_mode() && csrs.misa.has_supervisor_mode_extension())
				RAISE_ILLEGAL_INSTRUCTION();

			if (!ignore_wfi) {
				while (!has_local_pending_enabled_interrupts()) {
					sc_core::wait(wfi_event);
				}
			}
			break;

		case Opcode::SFENCE_VMA:
			if (s_mode() && csrs.mstatus.fields.tvm)
				RAISE_ILLEGAL_INSTRUCTION();
			lscache.fence_vma();
			break;

		case Opcode::URET:
			if (!csrs.misa.has_user_mode_extension())
				RAISE_ILLEGAL_INSTRUCTION();
			return_from_trap_handler(UserMode);
			break;

		case Opcode::SRET:
			if (!csrs.misa.has_supervisor_mode_extension() || (s_mode() && csrs.mstatus.fields.tsr))
				RAISE_ILLEGAL_INSTRUCTION();
			return_from_trap_handler(SupervisorMode);
			break;

		case Opcode::MRET:
			return_from_trap_handler(MachineMode);
			break;

		default:
			throw std::runtime_error("unknown opcode");
	}
}

uint64_t ISS_CT::_compute_and_get_current_cycles() {
	assert(cycle_counter % cycle_time == sc_core::SC_ZERO_TIME);
	assert(cycle_counter.value() % cycle_time.value() == 0);

	uint64_t num_cycles = cycle_counter.value() / cycle_time.value();

	return num_cycles;
}

bool ISS_CT::is_invalid_csr_access(uxlen_t csr_addr, bool is_write) {
	if (csr_addr == csr::FFLAGS_ADDR || csr_addr == csr::FRM_ADDR || csr_addr == csr::FCSR_ADDR) {
		fp_require_not_off();
	}
	if (csr_addr == csr::VSTART_ADDR || csr_addr == csr::VXSAT_ADDR || csr_addr == csr::VXRM_ADDR ||
	    csr_addr == csr::VCSR_ADDR || csr_addr == csr::VL_ADDR || csr_addr == csr::VTYPE_ADDR ||
	    csr_addr == csr::VLENB_ADDR) {
		v_ext.requireNotOff();
	}
	PrivilegeLevel csr_prv = (0x300 & csr_addr) >> 8;
	bool csr_readonly = ((0xC00 & csr_addr) >> 10) == 3;
	bool s_invalid = (csr_prv == SupervisorMode) && !csrs.misa.has_supervisor_mode_extension();
	bool u_invalid = (csr_prv == UserMode) && !csrs.misa.has_user_mode_extension();
	return (is_write && csr_readonly) || (prv < csr_prv) || s_invalid || u_invalid;
}

void ISS_CT::validate_csr_counter_read_access_rights(uxlen_t addr) {
	// match against counter CSR addresses, see RISC-V privileged spec for the address definitions
	if ((addr >= 0xC00 && addr <= 0xC1F)) {
		auto cnt = addr & 0x1F;  // 32 counter in total, naturally aligned with the mcounteren and scounteren CSRs

		if (s_mode() && !csr::is_bitset(csrs.mcounteren, cnt))
			RAISE_ILLEGAL_INSTRUCTION();

		if (u_mode() && (!csr::is_bitset(csrs.mcounteren, cnt) || !csr::is_bitset(csrs.scounteren, cnt)))
			RAISE_ILLEGAL_INSTRUCTION();
	}
}

uxlen_t ISS_CT::get_csr_value(uxlen_t addr) {
	validate_csr_counter_read_access_rights(addr);

	auto read = [=](auto &x, uxlen_t mask) { return x.reg & mask; };

	using namespace csr;

	switch (addr) {
		case TIME_ADDR:
		case MTIME_ADDR: {
			uint64_t mtime = clint->update_and_get_mtime();
			csrs.time.reg = mtime;
			return csrs.time.reg;
		}

		case CYCLE_ADDR:
		case MCYCLE_ADDR:
			csrs.cycle.reg = _compute_and_get_current_cycles();
			return csrs.cycle.reg;

		case MINSTRET_ADDR:
			return csrs.instret.reg;

		SWITCH_CASE_MATCH_ANY_HPMCOUNTER_RV64:  // not implemented
			return 0;

			// TODO: SD should be updated as SD=XS|FS and SD should be read-only -> update mask
		case MSTATUS_ADDR:
			return read(csrs.mstatus, MSTATUS_READ_MASK);
		case SSTATUS_ADDR:
			return read(csrs.mstatus, SSTATUS_READ_MASK);
		case USTATUS_ADDR:
			return read(csrs.mstatus, USTATUS_MASK);

		case MIP_ADDR:
			return read(csrs.mip, MIP_READ_MASK);
		case SIP_ADDR:
			return read(csrs.mip, SIP_MASK);
		case UIP_ADDR:
			return read(csrs.mip, UIP_MASK);

		case MIE_ADDR:
			return read(csrs.mie, MIE_MASK);
		case SIE_ADDR:
			return read(csrs.mie, SIE_MASK);
		case UIE_ADDR:
			return read(csrs.mie, UIE_MASK);

		case SATP_ADDR:
			if (csrs.mstatus.fields.tvm)
				RAISE_ILLEGAL_INSTRUCTION();
			break;

		case FCSR_ADDR:
			return read(csrs.fcsr, FCSR_MASK);

		case FFLAGS_ADDR:
			return csrs.fcsr.fields.fflags;

		case FRM_ADDR:
			return csrs.fcsr.fields.frm;

		case VCSR_ADDR:
			/* mirror vxrm and vxsat in vcsr */
			csrs.vcsr.fields.vxrm = csrs.vxrm.fields.vxrm;
			csrs.vcsr.fields.vxsat = csrs.vxsat.fields.vxsat;
			return csrs.vcsr.reg;
	}

	if (!csrs.is_valid_csr64_addr(addr))
		RAISE_ILLEGAL_INSTRUCTION();

	return csrs.default_read64(addr);
}

void ISS_CT::set_csr_value(uxlen_t addr, uxlen_t value) {
	auto write = [=](auto &x, uxlen_t mask) { x.reg = (x.reg & ~mask) | (value & mask); };

	using namespace csr;

	switch (addr) {
		case MISA_ADDR:                         // currently, read-only, thus cannot be changed at runtime
		SWITCH_CASE_MATCH_ANY_HPMCOUNTER_RV64:  // not implemented
			break;

		case SATP_ADDR: {
			if (csrs.mstatus.fields.tvm)
				RAISE_ILLEGAL_INSTRUCTION();
			auto mode = csrs.satp.fields.mode;
			write(csrs.satp, SATP_MASK);
			if (csrs.satp.fields.mode != SATP_MODE_BARE && csrs.satp.fields.mode != SATP_MODE_SV39 &&
			    csrs.satp.fields.mode != SATP_MODE_SV48)
				csrs.satp.fields.mode = mode;
			// std::cout << "[iss] satp=" << boost::format("%x") % csrs.satp.reg << std::endl;
		} break;

		case MTVEC_ADDR:
			write(csrs.mtvec, MTVEC_MASK);
			break;
		case STVEC_ADDR:
			write(csrs.stvec, MTVEC_MASK);
			break;
		case UTVEC_ADDR:
			write(csrs.utvec, MTVEC_MASK);
			break;

		case MEPC_ADDR:
			write(csrs.mepc, pc_alignment_mask());
			break;
		case SEPC_ADDR:
			write(csrs.sepc, pc_alignment_mask());
			break;
		case UEPC_ADDR:
			write(csrs.uepc, pc_alignment_mask());
			break;

		case MSTATUS_ADDR:
			write(csrs.mstatus, MSTATUS_WRITE_MASK);
			break;
		case SSTATUS_ADDR:
			write(csrs.mstatus, SSTATUS_WRITE_MASK);
			break;
		case USTATUS_ADDR:
			write(csrs.mstatus, USTATUS_MASK);
			break;

		case MIP_ADDR:
			write(csrs.mip, MIP_WRITE_MASK);
			break;
		case SIP_ADDR:
			write(csrs.mip, SIP_MASK);
			break;
		case UIP_ADDR:
			write(csrs.mip, UIP_MASK);
			break;

		case MIE_ADDR:
			write(csrs.mie, MIE_MASK);
			break;
		case SIE_ADDR:
			write(csrs.mie, SIE_MASK);
			break;
		case UIE_ADDR:
			write(csrs.mie, UIE_MASK);
			break;

		case MIDELEG_ADDR:
			write(csrs.mideleg, MIDELEG_MASK);
			break;

		case MEDELEG_ADDR:
			write(csrs.medeleg, MEDELEG_MASK);
			break;

		case SIDELEG_ADDR:
			write(csrs.sideleg, SIDELEG_MASK);
			break;

		case SEDELEG_ADDR:
			write(csrs.sedeleg, SEDELEG_MASK);
			break;

		case MCOUNTEREN_ADDR:
			write(csrs.mcounteren, MCOUNTEREN_MASK);
			break;

		case SCOUNTEREN_ADDR:
			write(csrs.scounteren, MCOUNTEREN_MASK);
			break;

		case MCOUNTINHIBIT_ADDR:
			write(csrs.mcountinhibit, MCOUNTINHIBIT_MASK);
			break;

		case FCSR_ADDR:
			write(csrs.fcsr, FCSR_MASK);
			break;

		case FFLAGS_ADDR:
			csrs.fcsr.fields.fflags = value;
			break;

		case FRM_ADDR:
			csrs.fcsr.fields.frm = value;
			break;

		case VXSAT_ADDR:
			write(csrs.vxsat, VXSAT_MASK);
			/* mirror vxsat in vcsr */
			csrs.vcsr.fields.vxsat = csrs.vxsat.fields.vxsat;
			break;

		case VXRM_ADDR:
			write(csrs.vxrm, VXRM_MASK);
			/* mirror vxrm in vcsr */
			csrs.vcsr.fields.vxrm = csrs.vxrm.fields.vxrm;
			break;

		case VCSR_ADDR:
			write(csrs.vcsr, VCSR_MASK);
			/* mirror vxrm and vxsat in vcsr */
			csrs.vxrm.fields.vxrm = csrs.vcsr.fields.vxrm;
			csrs.vxsat.fields.vxsat = csrs.vcsr.fields.vxsat;
			break;

		default:
			if (!csrs.is_valid_csr64_addr(addr))
				RAISE_ILLEGAL_INSTRUCTION();

			csrs.default_write64(addr, value);
	}
}

void ISS_CT::init(instr_memory_if *instr_mem, data_memory_if *data_mem, clint_if *clint, uxlen_t entrypoint,
                  uxlen_t sp) {
	this->instr_mem = instr_mem;
	this->mem = data_mem;
	this->clint = clint;
	regs[RegFile::sp] = sp;
	pc = entrypoint;

	uint64_t hartId = get_hart_id();
	lscache.init(hartId, data_mem);
}

void ISS_CT::sys_exit() {
	shall_exit = true;
}

unsigned ISS_CT::get_syscall_register_index() {
	if (csrs.misa.has_E_base_isa())
		return RegFile::a5;
	else
		return RegFile::a7;
}

uint64_t ISS_CT::read_register(unsigned idx) {
	return regs.read(idx);
}

void ISS_CT::write_register(unsigned idx, uint64_t value) {
	regs.write(idx, value);
}

uint64_t ISS_CT::get_progam_counter(void) {
	return pc;
}

void ISS_CT::block_on_wfi(bool block) {
	ignore_wfi = !block;
}

CoreExecStatus ISS_CT::get_status(void) {
	return status;
}

void ISS_CT::set_status(CoreExecStatus s) {
	status = s;
}

void ISS_CT::enable_debug(void) {
	debug_mode = true;
}

void ISS_CT::insert_breakpoint(uint64_t addr) {
	breakpoints.insert(addr);
}

void ISS_CT::remove_breakpoint(uint64_t addr) {
	breakpoints.erase(addr);
}

uint64_t ISS_CT::get_hart_id() {
	return csrs.mhartid.reg;
}

std::vector<uint64_t> ISS_CT::get_registers(void) {
	std::vector<uint64_t> regvals;

	for (auto v : regs.regs) regvals.push_back(v);

	return regvals;
}

void ISS_CT::fp_finish_instr() {
	fp_set_dirty();
	fp_update_exception_flags();
}

void ISS_CT::fp_prepare_instr() {
	assert(softfloat_exceptionFlags == 0);
	fp_require_not_off();
}

void ISS_CT::fp_set_dirty() {
	csrs.mstatus.fields.sd = 1;
	csrs.mstatus.fields.fs = FS_DIRTY;
}

void ISS_CT::fp_update_exception_flags() {
	if (softfloat_exceptionFlags) {
		fp_set_dirty();
		csrs.fcsr.fields.fflags |= softfloat_exceptionFlags;
		softfloat_exceptionFlags = 0;
	}
}

void ISS_CT::fp_setup_rm() {
	auto rm = instr.frm();
	if (rm == FRM_DYN)
		rm = csrs.fcsr.fields.frm;
	if (rm > FRM_RMM)
		RAISE_ILLEGAL_INSTRUCTION();
	softfloat_roundingMode = rm;
}

void ISS_CT::fp_require_not_off() {
	if (csrs.mstatus.fields.fs == FS_OFF)
		RAISE_ILLEGAL_INSTRUCTION();
}

void ISS_CT::return_from_trap_handler(PrivilegeLevel return_mode) {
	switch (return_mode) {
		case MachineMode:
			prv = csrs.mstatus.fields.mpp;
			csrs.mstatus.fields.mie = csrs.mstatus.fields.mpie;
			csrs.mstatus.fields.mpie = 1;
			pc = csrs.mepc.reg;
			if (csrs.misa.has_user_mode_extension())
				csrs.mstatus.fields.mpp = UserMode;
			else
				csrs.mstatus.fields.mpp = MachineMode;
			break;

		case SupervisorMode:
			prv = csrs.mstatus.fields.spp;
			csrs.mstatus.fields.sie = csrs.mstatus.fields.spie;
			csrs.mstatus.fields.spie = 1;
			pc = csrs.sepc.reg;
			if (csrs.misa.has_user_mode_extension())
				csrs.mstatus.fields.spp = UserMode;
			else
				csrs.mstatus.fields.spp = SupervisorMode;
			break;

		case UserMode:
			prv = UserMode;
			csrs.mstatus.fields.uie = csrs.mstatus.fields.upie;
			csrs.mstatus.fields.upie = 1;
			pc = csrs.uepc.reg;
			break;

		default:
			throw std::runtime_error("unknown privilege level " + std::to_string(return_mode));
	}

	if (trace)
		printf("[vp::iss] return from trap handler, time %s, pc %16lx, prv %1x\n",
		       quantum_keeper.get_current_time().to_string().c_str(), pc, prv);
}

void ISS_CT::trigger_external_interrupt(PrivilegeLevel level) {
	if (trace)
		std::cout << "[vp::iss] trigger external interrupt, " << sc_core::sc_time_stamp() << std::endl;

	switch (level) {
		case UserMode:
			csrs.mip.fields.ueip = true;
			break;
		case SupervisorMode:
			csrs.mip.fields.seip = true;
			break;
		case MachineMode:
			csrs.mip.fields.meip = true;
			break;
	}

	wfi_event.notify(sc_core::SC_ZERO_TIME);
}

void ISS_CT::clear_external_interrupt(PrivilegeLevel level) {
	if (trace)
		std::cout << "[vp::iss] clear external interrupt, " << sc_core::sc_time_stamp() << std::endl;

	switch (level) {
		case UserMode:
			csrs.mip.fields.ueip = false;
			break;
		case SupervisorMode:
			csrs.mip.fields.seip = false;
			break;
		case MachineMode:
			csrs.mip.fields.meip = false;
			break;
	}
}

void ISS_CT::trigger_timer_interrupt() {
	if (trace)
		std::cout << "[vp::iss] trigger timer interrupt, " << sc_core::sc_time_stamp() << std::endl;
	csrs.mip.fields.mtip = true;
	wfi_event.notify(sc_core::SC_ZERO_TIME);
}

void ISS_CT::clear_timer_interrupt() {
	if (trace)
		std::cout << "[vp::iss] clear timer interrupt, " << sc_core::sc_time_stamp() << std::endl;
	csrs.mip.fields.mtip = false;
}

void ISS_CT::trigger_software_interrupt() {
	if (trace)
		std::cout << "[vp::iss] trigger software interrupt, " << sc_core::sc_time_stamp() << std::endl;
	csrs.mip.fields.msip = true;
	wfi_event.notify(sc_core::SC_ZERO_TIME);
}

void ISS_CT::clear_software_interrupt() {
	if (trace)
		std::cout << "[vp::iss] clear software interrupt, " << sc_core::sc_time_stamp() << std::endl;
	csrs.mip.fields.msip = false;
}

void ISS_CT::halt() {
	if (debug_mode) {
		this->status = CoreExecStatus::HitBreakpoint;
	}
}

std::string ISS_CT::name() {
	return this->systemc_name;
}

PrivilegeLevel ISS_CT::prepare_trap(SimulationTrap &e) {
	// undo any potential pc update (for traps the pc should point to the originating instruction and not it's
	// successor)
	pc = last_pc;
	unsigned exc_bit = (1 << e.reason);

	// 1) machine mode execution takes any traps, independent of delegation setting
	// 2) non-delegated traps are processed in machine mode, independent of current execution mode
	if (prv == MachineMode || !(exc_bit & csrs.medeleg.reg)) {
		csrs.mcause.fields.interrupt = 0;
		csrs.mcause.fields.exception_code = e.reason;
		csrs.mtval.reg = e.mtval;
		return MachineMode;
	}

	// see above machine mode comment
	if (prv == SupervisorMode || !(exc_bit & csrs.sedeleg.reg)) {
		csrs.scause.fields.interrupt = 0;
		csrs.scause.fields.exception_code = e.reason;
		csrs.stval.reg = e.mtval;
		return SupervisorMode;
	}

	assert(prv == UserMode && (exc_bit & csrs.medeleg.reg) && (exc_bit & csrs.sedeleg.reg));
	csrs.ucause.fields.interrupt = 0;
	csrs.ucause.fields.exception_code = e.reason;
	csrs.utval.reg = e.mtval;
	return UserMode;
}

void ISS_CT::prepare_interrupt(const PendingInterrupts &e) {
	if (trace) {
		std::cout << "[vp::iss] prepare interrupt, pending=" << e.pending << ", target-mode=" << e.target_mode
		          << std::endl;
	}

	csr_mip x{e.pending};

	ExceptionCode exc;
	if (x.fields.meip)
		exc = EXC_M_EXTERNAL_INTERRUPT;
	else if (x.fields.msip)
		exc = EXC_M_SOFTWARE_INTERRUPT;
	else if (x.fields.mtip)
		exc = EXC_M_TIMER_INTERRUPT;
	else if (x.fields.seip)
		exc = EXC_S_EXTERNAL_INTERRUPT;
	else if (x.fields.ssip)
		exc = EXC_S_SOFTWARE_INTERRUPT;
	else if (x.fields.stip)
		exc = EXC_S_TIMER_INTERRUPT;
	else if (x.fields.ueip)
		exc = EXC_U_EXTERNAL_INTERRUPT;
	else if (x.fields.usip)
		exc = EXC_U_SOFTWARE_INTERRUPT;
	else if (x.fields.utip)
		exc = EXC_U_TIMER_INTERRUPT;
	else
		throw std::runtime_error("some pending interrupt must be available here");

	switch (e.target_mode) {
		case MachineMode:
			csrs.mcause.fields.exception_code = exc;
			csrs.mcause.fields.interrupt = 1;
			break;

		case SupervisorMode:
			csrs.scause.fields.exception_code = exc;
			csrs.scause.fields.interrupt = 1;
			break;

		case UserMode:
			csrs.ucause.fields.exception_code = exc;
			csrs.ucause.fields.interrupt = 1;
			break;

		default:
			throw std::runtime_error("unknown privilege level " + std::to_string(e.target_mode));
	}
}

PendingInterrupts ISS_CT::compute_pending_interrupts() {
	uxlen_t pending = csrs.mie.reg & csrs.mip.reg;

	if (!pending)
		return {NoneMode, 0};

	auto m_pending = pending & ~csrs.mideleg.reg;
	if (m_pending && (prv < MachineMode || (prv == MachineMode && csrs.mstatus.fields.mie))) {
		return {MachineMode, m_pending};
	}

	pending = pending & csrs.mideleg.reg;
	auto s_pending = pending & ~csrs.sideleg.reg;
	if (s_pending && (prv < SupervisorMode || (prv == SupervisorMode && csrs.mstatus.fields.sie))) {
		return {SupervisorMode, s_pending};
	}

	auto u_pending = pending & csrs.sideleg.reg;
	if (u_pending && (prv == UserMode && csrs.mstatus.fields.uie)) {
		return {UserMode, u_pending};
	}

	return {NoneMode, 0};
}

void ISS_CT::switch_to_trap_handler(PrivilegeLevel target_mode) {
	if (trace) {
		printf("[vp::iss] switch to trap handler, time %s, last_pc %16lx, pc %16lx, irq %u, t-prv %1x\n",
		       quantum_keeper.get_current_time().to_string().c_str(), last_pc, pc, csrs.mcause.fields.interrupt,
		       target_mode);
	}

	// free any potential LR/SC bus lock before processing a trap/interrupt
	release_lr_sc_reservation();

	auto pp = prv;
	prv = target_mode;

	switch (target_mode) {
		case MachineMode:
			csrs.mepc.reg = pc;

			csrs.mstatus.fields.mpie = csrs.mstatus.fields.mie;
			csrs.mstatus.fields.mie = 0;
			csrs.mstatus.fields.mpp = pp;

			pc = csrs.mtvec.get_base_address();

			if (unlikely(pc == 0)) {
				if (error_on_zero_traphandler) {
					throw std::runtime_error("[ISS] Took null trap handler in machine mode");
				} else {
					static bool once = true;
					if (once)
						std::cout
						    << "[ISS] Warn: Taking trap handler in machine mode to 0x0, this is probably an error."
						    << std::endl;
					once = false;
				}
			}

			if (csrs.mcause.fields.interrupt && csrs.mtvec.fields.mode == csr_mtvec::Mode::Vectored)
				pc += 4 * csrs.mcause.fields.exception_code;
			break;

		case SupervisorMode:
			assert(prv == SupervisorMode || prv == UserMode);

			csrs.sepc.reg = pc;

			csrs.mstatus.fields.spie = csrs.mstatus.fields.sie;
			csrs.mstatus.fields.sie = 0;
			csrs.mstatus.fields.spp = pp;

			pc = csrs.stvec.get_base_address();

			if (csrs.scause.fields.interrupt && csrs.stvec.fields.mode == csr_mtvec::Mode::Vectored)
				pc += 4 * csrs.scause.fields.exception_code;
			break;

		case UserMode:
			assert(prv == UserMode);

			csrs.uepc.reg = pc;

			csrs.mstatus.fields.upie = csrs.mstatus.fields.uie;
			csrs.mstatus.fields.uie = 0;

			pc = csrs.utvec.get_base_address();

			if (csrs.ucause.fields.interrupt && csrs.utvec.fields.mode == csr_mtvec::Mode::Vectored)
				pc += 4 * csrs.ucause.fields.exception_code;
			break;

		default:
			throw std::runtime_error("unknown privilege level " + std::to_string(target_mode));
	}

	if (last_pc == pc) {
		shall_exit = true;
	}
}

void ISS_CT::performance_and_sync_update(Opcode::Mapping executed_op) {
	if (!csrs.mcountinhibit.fields.IR)
		++csrs.instret.reg;

	if (lr_sc_counter != 0) {
		--lr_sc_counter;
		assert(lr_sc_counter >= 0);
		if (lr_sc_counter == 0)
			release_lr_sc_reservation();
	}

	auto new_cycles = instr_cycles[executed_op];

	if (!csrs.mcountinhibit.fields.CY)
		cycle_counter += new_cycles;

	quantum_keeper.inc(new_cycles);
	if (quantum_keeper.need_sync()) {
		if (lr_sc_counter == 0)  // match SystemC sync with bus unlocking in a tight LR_W/SC_W loop
			quantum_keeper.sync();
	}
}

void ISS_CT::handle_interrupt() {
	auto x = compute_pending_interrupts();
	if (x.target_mode != NoneMode) {
		prepare_interrupt(x);
		switch_to_trap_handler(x.target_mode);
	}
}

void ISS_CT::handle_trap(SimulationTrap &e) {
	if (trace) {
		std::cout << "take trap " << e.reason << ", mtval=" << boost::format("%x") % e.mtval
		          << ", pc=" << boost::format("%x") % last_pc << std::endl;
	}
	auto target_mode = prepare_trap(e);
	switch_to_trap_handler(target_mode);
}

void ISS_CT::run_step() {
	assert(regs.read(0) == 0);

	// speeds up the execution performance (non debug mode) significantly by
	// checking the additional flag first
	if (debug_mode && (breakpoints.find(pc) != breakpoints.end())) {
		status = CoreExecStatus::HitBreakpoint;
		return;
	}

	last_pc = pc;
	try {
		exec_step();
		handle_interrupt();
	} catch (SimulationTrap &e) {
		handle_trap(e);
	}

	// Do not use a check *pc == last_pc* here. The reason is that due to
	// interrupts *pc* can be set to *last_pc* accidentally (when jumping back
	// to *mepc*).
	if (shall_exit)
		status = CoreExecStatus::Terminated;

	performance_and_sync_update(op);
}

void ISS_CT::run() {
	// run a single step until either a breakpoint is hit or the execution terminates
	do {
		run_step();
	} while (status == CoreExecStatus::Runnable);

	// force sync to make sure that no action is missed
	quantum_keeper.sync();
}

void ISS_CT::show() {
	boost::io::ios_flags_saver ifs(std::cout);
	std::cout << "=[ core : " << csrs.mhartid.reg << " ]===========================" << std::endl;
	std::cout << "simulation time: " << sc_core::sc_time_stamp() << std::endl;
	regs.show();
	std::cout << "pc = " << std::hex << pc << std::endl;
	std::cout << "num-instr = " << std::dec << csrs.instret.reg << std::endl;
}
