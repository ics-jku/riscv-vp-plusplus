/*
 * NEVER BUILD THIS FILE DIRECTLY!!!
 *
 * NOTE RVxx.2: C-style macros
 * concrete implementatins are are derived in iss.cpp (by iss_ctemplate_handle.h)
 * see NOTE RVxx.2 in iss_ctemplate_handle.h for more details
 */

#include "iss.h"

// std::replace
#include <algorithm>
// to save *cout* format setting, see *ISS_CT::show*
#include <boost/format.hpp>
#include <boost/io/ios_state.hpp>
// for safe down-cast
#include <boost/lexical_cast.hpp>

#include "util/propertytree.h"

namespace cheriv9::rv64 {

#define VExt VExtension<ISS_CT>

// GCC and clang support these types on x64 machines
// perhaps use boost::multiprecision::int128_t instead
// see: https://stackoverflow.com/questions/18439520/is-there-a-128-bit-integer-in-c
typedef __int128_t int128_t;
typedef __uint128_t uint128_t;

#define RAISE_ILLEGAL_INSTRUCTION() raise_trap(EXC_ILLEGAL_INSTR, instr.data(), &rvfi_dii_output);

#define RD instr.rd()
#define RS1 instr.rs1()
#define RS2 instr.rs2()
#define RS3 instr.rs3()

ISS_CT::ISS_CT(RV_ISA_Config *isa_config, uxlen_t hart_id)
    : isa_config(isa_config), stats(hart_id), v_ext(*this), systemc_name("Core-" + std::to_string(hart_id)) {
	ISS_CT::init_cheri_regs();
	csrs.mhartid.reg.val = hart_id;
	csrs.misa.reg.fields.extensions = isa_config->get_misa_extensions();

	/* get config properties from global property tree (or use default) */
	VPPP_PROPERTY_GET("ISS." + name(), "clock_cycle_period", sc_core::sc_time, prop_clock_cycle_period);

	sc_core::sc_time qt = tlm::tlm_global_quantum::instance().get();

	assert(qt >= prop_clock_cycle_period);
	assert(qt % prop_clock_cycle_period == sc_core::SC_ZERO_TIME);

	/*
	 * NOTE: The cycle model below is a static cycle model -> Value changes at
	 * runtime may have no effect (since cycles may be cached)
	 * If you want to add a dynamic cycle model, you have add this in the
	 * operation implementations below (OPCASE)
	 */

	/* Enable the use of the legacy, hard-coded instruction clock cycle model
	 * If use_legacy_instr_clock_cycle_model is not set (default = 1) or set to >0 explicitly, then the
	 * instr_clock_cycle values of the legacy, hard-coded model are used as default values for instructions
	 * not explicitly in the PropertyTree.
	 */
	uint64_t use_legacy_cycle_model = 1;
	VPPP_PROPERTY_GET("ISS." + name(), "use_legacy_instr_clock_cycle_model", uint64_t, use_legacy_cycle_model);

	/* Default instruction clock cycles
	 * If the legacy model is disabled (use_legacy_cycle_model = 0), this value is used as the default for
	 * instructions where instr_clock_cycles is not explicitly specified in the property tree.
	 */
	uint64_t default_instr_clock_cycles = 1;
	VPPP_PROPERTY_GET("ISS." + name(), "default_instr_clock_cycles", uint64_t, default_instr_clock_cycles);

	/*
	 * Example 1 (override values of legacy model):
	 * ```
	 *     "vppp.ISS.Core-0.LW_instr_clock_cycles": "0x6",
	 *     "vppp.ISS.Core-0.XOR_instr_clock_cycles": "0x1",
	 * ```
	 *  * "use_legacy_instr_clock_cycle_model" is not explicitly set -> defaults
	 *    to 1 (enabled) -> use legacy model values as default
	 *  * LW explicitly set to 6 cycles
	 *  * XOR explicitly set to 1 cycle
	 *  * cycles for all other instructions set according to legacy model (e.g. LB to 4 cycles)
	 *
	 * Example 2 (override values of legacy model):
	 * ```
	 *     "vppp.ISS.Core-0.use_legacy_instr_clock_cycle_model": "0x1",
	 *     "vppp.ISS.Core-0.LW_instr_clock_cycles": "0x6",
	 *     "vppp.ISS.Core-0.XOR_instr_clock_cycles": "0x1",
	 *     "vppp.ISS.Core-0.default_instr_clock_cycles": "0x1",
	 * ```
	 *  * "use_legacy_instr_clock_cycle_model" is explicitly set to 1 -> use
	 *    legacy model values as default -> use legacy model values as default
	 *  * LW explicitly set to 6 cycles (same behavior as in Example 1)
	 *  * XOR explicitly set to 1 cycle (same behavior as in Example 1)
	 *  * cycles for all other instructions set by legacy model (e.g. LB to 4
	 *    cycles) (same behavior as in Example 1)
	 *  * "default_instr_clock_cycles" is ignored
	 *
	 * Example 3 (describe full custom model)
	 * ```
	 *     "vppp.ISS.Core-0.use_legacy_instr_clock_cycle_model": "0x0",
	 *     "vppp.ISS.Core-0.LW_instr_clock_cycles": "0x6",
	 *     "vppp.ISS.Core-0.XOR_instr_clock_cycles": "0x1",
	 *     "vppp.ISS.Core-0.default_instr_clock_cycles": "0x1",
	 * ```
	 *  * "use_legacy_instr_clock_cycle_model" is explicitly set to 0 -> legacy model
	 *    is ignored
	 *  * LW explicitly set to 6 cycles
	 *  * XOR explicitly set to 1 cycle
	 *  * cycles for all other instructions set to value of
	 *    "default_instr_clock_cycles", i.e. to 1
	 */

	/* initialize opMap including timing model */
	for (unsigned int opId = 0; opId < Operation::OpId::NUMBER_OF_OPERATIONS; ++opId) {
		uint64_t instr_clock_cycles = default_instr_clock_cycles;

		/* set instruction id and reset the jump label */
		opMap[opId].opId = (Operation::OpId)opId;
		opMap[opId].labelPtr = nullptr;

		/* use legacy model (see above) -- DEPRECATED! */
		if (use_legacy_cycle_model) {
			/* TODO: This model is incomplete for RV64_CHERIV9 (e.g. LD/SD missing), however we don't fix this yet
			 * because we want to keep compatibility with older VP version and the legacy model is depricated anyways.
			 */
			switch (opId) {
				case Operation::OpId::LB:
				case Operation::OpId::LBU:
				case Operation::OpId::LH:
				case Operation::OpId::LHU:
				case Operation::OpId::LW:
				case Operation::OpId::SB:
				case Operation::OpId::SH:
				case Operation::OpId::SW:
					instr_clock_cycles = 4;
					break;

				case Operation::OpId::MUL:
				case Operation::OpId::MULH:
				case Operation::OpId::MULHU:
				case Operation::OpId::MULHSU:
				case Operation::OpId::DIV:
				case Operation::OpId::DIVU:
				case Operation::OpId::REM:
				case Operation::OpId::REMU:
					instr_clock_cycles = 8;
					break;

				default:
					instr_clock_cycles = 1;
			}
		}

		/* try to load cycle model from the global property map (or use default) */
		std::string desc = std::string(Operation::opIdStr[opId]);
		std::replace(desc.begin(), desc.end(), '.', '_');
		std::replace(desc.begin(), desc.end(), ' ', '_');
		VPPP_PROPERTY_GET("ISS." + name(), desc + "_instr_clock_cycles", uint64_t, instr_clock_cycles);

		/* set instruction time */
		opMap[opId].instr_time = instr_clock_cycles * prop_clock_cycle_period.value(); /* ps */
	}
}

void ISS_CT::print_trace() {
	uint32_t mem_word = dbbcache.get_mem_word();

	/*
	 * NOTE:
	 *  * we decode here again, but efficiency does not matter much for this kind of tracing.
	 *    -> TODO: optimize (e.g. table from jump label to op)
	 *  * every compressed instruction was already converted to a normal instruction.
	 *    -> it is safe to use decode_normal here
	 */
	Operation::OpId opId = instr.decode_normal(ARCH, *isa_config);

	ProgramCounterCapability last_pc = dbbcache.get_last_pc_before_callback();
	printf("core %2lu: prv %1x: pcap: %d pc %16lx (%8x): %s ", csrs.mhartid.reg.val, prv,
	       last_pc->cap.fields.flag_cap_mode, last_pc->cap.fields.address, mem_word, Operation::opIdStr.at(opId));
	switch (Operation::getType(opId)) {
		case Operation::Type::R:
			if (opId == Operation::OpId::C_SPECIAL_R_W) {
				printf(COLORFRMT ", " COLORFRMT, COLORPRINT(regcolors[instr.rd()], RegFile::regnames[instr.rd()]),
				       COLORPRINT(regcolors[instr.rs1()], RegFile::regnames[instr.rs1()]));
				printf(", %s(0x%x)", csrs.scr_name_map(instr.rs2()), instr.rs2());
				break;
			}
			// TODO Special cases for most Capability instructions...
			if (opId == Operation::OpId::C_MOVE) {
				printf(COLORFRMT ", " COLORFRMT, COLORPRINT(regcolors[instr.rd()], RegFile::regnames[instr.rd()]),
				       COLORPRINT(regcolors[instr.rs1()], RegFile::regnames[instr.rs1()]));
				break;
			}
			printf(COLORFRMT ", " COLORFRMT ", " COLORFRMT,
			       COLORPRINT(regcolors[instr.rd()], RegFile::regnames[instr.rd()]),
			       COLORPRINT(regcolors[instr.rs1()], RegFile::regnames[instr.rs1()]),
			       COLORPRINT(regcolors[instr.rs2()], RegFile::regnames[instr.rs2()]));
			break;
		case Operation::Type::R4:
			printf(COLORFRMT ", " COLORFRMT ", " COLORFRMT ", " COLORFRMT,
			       COLORPRINT(regcolors[instr.rd()], RegFile::regnames[instr.rd()]),
			       COLORPRINT(regcolors[instr.rs1()], RegFile::regnames[instr.rs1()]),
			       COLORPRINT(regcolors[instr.rs2()], RegFile::regnames[instr.rs2()]),
			       COLORPRINT(regcolors[instr.rs3()], RegFile::regnames[instr.rs3()]));
			break;
		case Operation::Type::R_Q_M: {
			uint8_t q = (instr.rs1() & 0b11000) >> 3;                 // TODO: Check mask, make constant
			uint8_t m = ((instr.rs1() & 0b00111) << 5) | instr.rd();  // TODO: Check mask, make constant
			printf("q: %u, m: %u", q, m);
			break;
		}
		case Operation::Type::I:
			printf(COLORFRMT ", " COLORFRMT ", 0x%x", COLORPRINT(regcolors[instr.rd()], RegFile::regnames[instr.rd()]),
			       COLORPRINT(regcolors[instr.rs1()], RegFile::regnames[instr.rs1()]), instr.I_imm());
			break;
		case Operation::Type::S:
			printf(COLORFRMT ", " COLORFRMT ", 0x%x",
			       COLORPRINT(regcolors[instr.rs1()], RegFile::regnames[instr.rs1()]),
			       COLORPRINT(regcolors[instr.rs2()], RegFile::regnames[instr.rs2()]), instr.S_imm());
			break;
		case Operation::Type::B:
			printf(COLORFRMT ", " COLORFRMT ", 0x%x",
			       COLORPRINT(regcolors[instr.rs1()], RegFile::regnames[instr.rs1()]),
			       COLORPRINT(regcolors[instr.rs2()], RegFile::regnames[instr.rs2()]), instr.B_imm());
			break;
		case Operation::Type::U:
			printf(COLORFRMT ", 0x%x", COLORPRINT(regcolors[instr.rd()], RegFile::regnames[instr.rd()]), instr.U_imm());
			break;
		case Operation::Type::J:
			printf(COLORFRMT ", 0x%x", COLORPRINT(regcolors[instr.rd()], RegFile::regnames[instr.rd()]), instr.J_imm());
			break;
		default:;
	}
	puts("");
}
} /* namespace cheriv9::rv64 */

/*
 * label generation
 * Fetch, Decode and Dispatch (FDD) variants
 */

/* helpers for ctemplate label handling */
/* prefix for all static variables, sections and labels: "<ARCH>_<TEMPLATE_INSTANCE_NAME>_" (ensure uniqueness) */
#define OP_PREFIX M_JOIN(ISS_CT_ARCH, M_JOIN(_, M_JOIN(ISS_CT, _)))
/* op code entry section handling */
#define OP_LABEL_ENTRIES_SECNAME M_JOIN(OP_PREFIX, op_label_entries)
#define OP_LABLE_ENTRIES_SEC_STR M_DEFINE2STR(OP_LABEL_ENTRIES_SECNAME)
#define OP_LABEL_ENTIRES_SEC_START M_JOIN(__start_, OP_LABEL_ENTRIES_SECNAME)
#define OP_LABEL_ENTIRES_SEC_STOP M_JOIN(__stop_, OP_LABEL_ENTRIES_SECNAME)
/* fast_abort_and_fdd_labelPtr handling */
#define OP_GLOBAL_FAST_ABORT_AND_FDD_LABEL_NAME M_JOIN(OP_PREFIX, op_global_fast_abort_and_fdd_labelPtr)
#define OP_GLOBAL_FAST_ABORT_AND_FDD_LABEL_STR M_DEFINE2STR(OP_GLOBAL_FAST_ABORT_AND_FDD_LABEL_NAME)
#define OP_GLOBAL_FAST_ABORT_AND_FDD_LABEL_START M_JOIN(__start_, OP_GLOBAL_FAST_ABORT_AND_FDD_LABEL_NAME)
/* explicitly defined labels */
#define OP_LABEL(_name) M_JOIN(OP_PREFIX, M_JOIN(op_label_, _name))
#define OP_LABEL_ENTRY(_name) M_JOIN(OP_PREFIX, M_JOIN(op_label_, _name))
/* for labels generated in OP_CASE */
#define OP_LABEL_OP(_op) M_JOIN(OP_LABEL(op_), _op)
#define OP_LABEL_ENTRY_OP(_op) M_JOIN(OP_LABEL(entry_), _op)

extern void *const OP_GLOBAL_FAST_ABORT_AND_FDD_LABEL_START;
extern const struct op_label_entry OP_LABEL_ENTIRES_SEC_START;
extern const struct op_label_entry OP_LABEL_ENTIRES_SEC_STOP;

namespace cheriv9::rv64 {
void *ISS_CT::genOpMap() {
	bool error = false;
	struct op_label_entry *entry = (struct op_label_entry *)&OP_LABEL_ENTIRES_SEC_START;
	struct op_label_entry *end = (struct op_label_entry *)&OP_LABEL_ENTIRES_SEC_STOP;

	// fill op labels (all others are already initialized with nullptr)
	while (entry < end) {
		if ((unsigned int)entry->opId >= Operation::OpId::NUMBER_OF_OPERATIONS) {
			std::cerr << "[ISS] Error: Invalid operation (" << entry->opId << ") in op_lable_entry section at 0x"
			          << std::hex << entry << std::dec << std::endl;
			error = true;
			break;
		}
		if (opMap[entry->opId].labelPtr != nullptr) {
			std::cerr << "[ISS] Error: Multiple implementations for operation " << entry->opId << " ("
			          << Operation::opIdStr.at(entry->opId) << ")" << std::endl;
			error = true;
		}
		opMap[entry->opId].labelPtr = entry->labelPtr;
		entry++;
	}
	if (error) {
		throw std::runtime_error("[ISS] Multiple implementations for operation(s) (see above)");
	}

	// check for unimplemented opcodes
	for (unsigned int opId = 0; opId < Operation::OpId::NUMBER_OF_OPERATIONS; opId++) {
		if (opMap[opId].labelPtr == nullptr) {
			std::cerr << "[ISS] Error: Unimplemented operation " << opId << " (" << Operation::opIdStr.at(opId) << ")"
			          << std::endl;
			error = true;
		}
	}
	if (error) {
		throw std::runtime_error("[ISS] Unimplemented operation(s) (see above)");
	}

	return OP_GLOBAL_FAST_ABORT_AND_FDD_LABEL_START;
}

#define OP_SLOW_FDD()                                                                                           \
	assert(((pc & ~pc_alignment_mask()) == 0) && "misaligned instruction");                                     \
	stats.inc_cnt();                                                                                            \
	stats.inc_slow_fdd();                                                                                       \
	void *opLabelPtr;                                                                                           \
	uint8_t decoded_instr_length;                                                                               \
	if (unlikely(rvfi_dii)) {                                                                                   \
		opLabelPtr = dbbcache.fetch_rvfi_decode(pc, instr, rvfi_dii_input.rvfi_dii_insn, decoded_instr_length); \
	} else {                                                                                                    \
		opLabelPtr = dbbcache.fetch_decode(pc, instr);                                                          \
	}                                                                                                           \
	if (trace) {                                                                                                \
		print_trace();                                                                                          \
		/* always stay in slow path if trace enabled */                                                         \
		force_slow_path();                                                                                      \
	}                                                                                                           \
	if (unlikely(rvfi_dii)) {                                                                                   \
		rvfi_dii_output.rvfi_dii_insn = rvfi_dii_input.rvfi_dii_insn;                                           \
		rvfi_dii_output.rvfi_dii_pc_wdata = pc;                                                                 \
		rvfi_dii_output.rvfi_dii_order = csrs.instret.reg.val;                                                  \
		rvfi_dii_output.rvfi_dii_pc_rdata = pc;                                                                 \
		rvfi_dii_output.rvfi_dii_rs1_data = regs[instr.rs1()];                                                  \
		rvfi_dii_output.rvfi_dii_rs2_data = regs[instr.rs2()];                                                  \
		rvfi_dii_output.rvfi_dii_rs1_addr = instr.rs1();                                                        \
		rvfi_dii_output.rvfi_dii_rs2_addr = instr.rs2();                                                        \
	}                                                                                                           \
	goto *opLabelPtr;

#define OP_MED_FDD()     \
	stats.inc_cnt();     \
	stats.inc_med_fdd(); \
	goto *dbbcache.fetch_decode(pc, instr);

#define OP_FAST_FDD()     \
	stats.inc_cnt();      \
	stats.inc_fast_fdd(); \
	goto *dbbcache.fetch_decode_fast(instr);

/* fast operation finalization and fdd (TODO: move ninstr check to control flow ops?) */
#define OP_FAST_FINALIZE_AND_FDD() \
	ninstr++;                      \
	OP_FAST_FDD();

#define OP_GLOBAL_FDD() OP_LABEL(op_global_fdd) :

/* switch / case structure emulation */

#define OP_SWITCH_BEGIN()
#define OP_SWITCH_END()                                                            \
	OP_LABEL(op_global_fast_abort_and_fdd)                                         \
	    : static void * OP_GLOBAL_FAST_ABORT_AND_FDD_LABEL_NAME                    \
	      __attribute__((used, section(OP_GLOBAL_FAST_ABORT_AND_FDD_LABEL_STR))) = \
	    &&OP_LABEL(op_global_fast_abort_and_fdd);                                  \
	dbbcache.abort_fetch_decode_fast();                                            \
	stats.dec_cnt();                                                               \
	stats.inc_fast_fdd_abort();                                                    \
	goto OP_LABEL(op_global_fdd);                                                  \
	OP_LABEL(op_global_fast_finalize_and_fdd) : __attribute__((unused));           \
	{OP_FAST_FINALIZE_AND_FDD()}

#define OP_CASE(_op)                                                                                                 \
	OP_LABEL_OP(_op)                                                                                                 \
	    : static struct op_label_entry OP_LABEL_ENTRY_OP(_op)                                                        \
	          __attribute__((used, section(OP_LABLE_ENTRIES_SEC_STR))) = {Operation::OpId::_op, &&OP_LABEL_OP(_op)}; \
	stats.inc_op(Operation::OpId::_op);

#define OP_INVALID_END()                                                                                             \
	if (trace) {                                                                                                     \
		std::cout << "[ISS] WARNING: RV64 instruction not supported on RV32 " << std::to_string(instr.data())        \
		          << "' at address '" << std::to_string(dbbcache.get_last_pc_before_callback()) << "'" << std::endl; \
	}                                                                                                                \
	RAISE_ILLEGAL_INSTRUCTION();

#undef OP_CASE_NOP
#undef OP_CASE_INVALID
#ifdef ISS_CT_STATS_ENABLED
/* ensure correct counting of opIds (see OP_CASE) -> treat each nop as dedicated case with its own OP_END (no
 * fallthough, but higher overhead (cache)) */
#define OP_CASE_NOP(_op) \
	OP_CASE(_op)         \
	stats.inc_nops();    \
	OP_END();
#define OP_CASE_INVALID(_op) \
	OP_CASE(_op)             \
	OP_INVALID_END();

#else /* ISS_CT_STATS_ENABLED */
/* disabled stats -> fall through to common OP_END (less overhead -> more efficient (cache)) */
#define OP_CASE_NOP(_op) OP_CASE(_op)
#define OP_CASE_INVALID(_op) OP_CASE(_op)
#endif /* ISS_CT_STATS_ENABLED */

#ifdef ISS_CT_OP_TAIL_FAST_FDD_ENABLED
#define OP_END() OP_FAST_FINALIZE_AND_FDD()
#else
#define OP_END() goto OP_LABEL(op_global_fast_finalize_and_fdd)
#endif

/*
 * supress "ISO C++ forbids computed gotos [-Wpedantic]" warings by disabling -Wpedantic for exec_steps
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
void ISS_CT::exec_steps(const bool debug_single_step) {
	/* keep track if step was done */
	bool debug_single_step_done = false;

	// TODO: remove?
	assert(regs.read(0) == 0);

	/*
	 * Check quantum in fast path roughly every tenth of a quantum (heuristic)
	 * Uncertainties: time is also increasing outside of ISS; not every instruction has cycle_time
	 */
	const uint64_t fast_quantum_ins_granularity =
	    quantum_keeper.get_global_quantum().value() / prop_clock_cycle_period.value() / 10;
	uint64_t ninstr = 0;

	// TODO: remove?
	assert(((pc & ~pc_alignment_mask()) == 0) && "misaligned instruction");

	/* start in slow_path */
	force_slow_path();

	do {
		try {
			/* global (slow and med) operation fetch, decode and dispatch (FDD) */
			OP_GLOBAL_FDD() {
				pc = dbbcache.get_pc_maybe_after_callback();

				if (unlikely(iss_slow_path)) {
					iss_slow_path = false;

					/* update counters by local fast counters */
					commit_instructions(ninstr);
					commit_cycles();

					/* call interrupt handling */
					handle_interrupt();

					// TODO: CHECK!
					/* Do not use a check *pc == last_pc* here. The reason is that due to */
					/* interrupts *pc* can be set to *last_pc* accidentally (when jumping back */
					/* to *mepc*). */
					if (shall_exit) {
						set_status(CoreExecStatus::Terminated);
					}
					/* run a single step until either a breakpoint is hit or the execution terminates */
					if (status != CoreExecStatus::Runnable) {
						break;
					}

					if (lr_sc_counter != 0) {
						stats.inc_lr_sc();
						--lr_sc_counter;
						assert(lr_sc_counter >= 0);
						if (lr_sc_counter == 0) {
							release_lr_sc_reservation();
						} else {
							// again
							force_slow_path();
						}
					} else {
						// match SystemC sync with bus unlocking in a tight LR_W/SC_W loop
						stats.inc_qk_need_sync();
						if (quantum_keeper.need_sync()) {
							stats.inc_qk_sync();
							quantum_keeper.sync();
						}
					}

					/* speeds up the execution performance (non debug mode) significantly by */
					/* checking the additional flag first */
					if (debug_mode) {
						/* always stay in slow path if debug enabled */
						force_slow_path();

						/* stop after single step */
						if (debug_single_step && debug_single_step_done) {
							if (unlikely(rvfi_dii)) {
								// x0 must be reset here, as it must not change if written to it
								// This is done later on in run_step, but for reporting via rvfi_dii it must be done
								// now
								regs.regs[regs.zero] = cNullCap;
								// Trace manipulated registers
								Operation::OpId op = instr.decode_normal(
								    ARCH, *isa_config);  // TODO This can be optimizes, see print_trace
								if (op == Operation::OpId::C_CLEAR || op == Operation::OpId::FP_CLEAR) {
									// Trace is done inside this functions instead
								} else if (op != Operation::OpId::C_INVOKE) {
									// C Invoke has its own format, it does not fit in any of the classic opcode
									// types, it is handled directly in C_INVOKE opcode switch
									switch (Operation::getType(op)) {
										case Operation::Type::R:
											rvfi_dii_output.rvfi_dii_rd_wdata = regs[instr.rd()];
											rvfi_dii_output.rvfi_dii_rd_addr = instr.rd();
											rvfi_dii_output.rvfi_dii_pc_wdata = pc;
											break;
										case Operation::Type::B:
											rvfi_dii_output.rvfi_dii_pc_wdata = pc;
											break;
										case Operation::Type::I:
											if (op == Operation::OpId::ADDI_NOP) {
												break;
											}
											rvfi_dii_output.rvfi_dii_rd_wdata = regs[instr.rd()];
											rvfi_dii_output.rvfi_dii_rd_addr = instr.rd();
											rvfi_dii_output.rvfi_dii_pc_wdata = pc;
											break;
										case Operation::Type::S:
											rvfi_dii_output.rvfi_dii_pc_wdata = pc;
											break;
										case Operation::Type::U:
											rvfi_dii_output.rvfi_dii_rd_wdata = regs[instr.rd()];
											rvfi_dii_output.rvfi_dii_rd_addr = instr.rd();
											rvfi_dii_output.rvfi_dii_pc_wdata = pc;
											break;
										case Operation::Type::J:
											rvfi_dii_output.rvfi_dii_rd_wdata = regs[instr.rd()];
											rvfi_dii_output.rvfi_dii_rd_addr = instr.rd();
											rvfi_dii_output.rvfi_dii_pc_wdata = pc;
											break;
										default:
											rvfi_dii_output.rvfi_dii_rd_wdata = regs[instr.rd()];
											rvfi_dii_output.rvfi_dii_rd_addr = instr.rd();
											rvfi_dii_output.rvfi_dii_pc_wdata = pc;
											break;
									}
								}
							}
							break;
						}

						/* stop on breakpoint */
						if (breakpoints.find(pc) != breakpoints.end()) {
							set_status(CoreExecStatus::HitBreakpoint);
							break;
						}
						if (unlikely(rvfi_dii)) {
							rvfi_dii_output = {};
						}

						debug_single_step_done = true;
					}

					OP_SLOW_FDD();
					// UNREACHABLE
					assert(false);
				}

				if (unlikely(ninstr > fast_quantum_ins_granularity)) {
					///* perform slow path next time -> update instr/cyclc counters and check if quantum needs sync */
					// iss_slow_path = true;
					commit_instructions(ninstr);
					commit_cycles();
					stats.inc_qk_need_sync();
					if (quantum_keeper.need_sync()) {
						// TODO: must also be done for transactions (keeper in common/mem.h) ?!
						/* sync pc member variable before potential SysC context switch */
						// pc = dbbcache.get_pc_maybe_after_callback();
						stats.inc_qk_sync();
						quantum_keeper.sync();
					}
				}

				OP_MED_FDD();
				// UNREACHABLE
				assert(false);
			}

			/* operation implementations and implicit fast operation fetch, decode and dispatch (FFD) */
			OP_SWITCH_BEGIN() {
				OP_CASE(UNDEF) {
					if (trace)
						std::cout << "[ISS] WARNING: unknown instruction '" << std::to_string(instr.data())
						          << "' at address '" << std::to_string(dbbcache.get_last_pc_before_callback()) << "'"
						          << std::endl;
					RAISE_ILLEGAL_INSTRUCTION();
				}
				OP_END();

				OP_CASE(UNSUP) {
					if (trace)
						std::cout << "[ISS] WARNING: instruction not supported (e.g. extension disabled in misa csr) '"
						          << std::to_string(instr.data()) << "' at address '"
						          << std::to_string(dbbcache.get_last_pc_before_callback()) << "'" << std::endl;
					RAISE_ILLEGAL_INSTRUCTION();
				}
				OP_END();

				/*
				 * NOP instruction variants
				 * instructions decoded with rd == zero/x0 and no side effects -> nothing to do
				 */
				OP_CASE_NOP(LUI_NOP)
				OP_CASE_NOP(AUIPC_NOP)
				OP_CASE_NOP(ADDI_NOP)
				OP_CASE_NOP(SLTI_NOP)
				OP_CASE_NOP(SLTIU_NOP)
				OP_CASE_NOP(XORI_NOP)
				OP_CASE_NOP(ORI_NOP)
				OP_CASE_NOP(ANDI_NOP)
				OP_CASE_NOP(SLLI_NOP)
				OP_CASE_NOP(SRLI_NOP)
				OP_CASE_NOP(SRAI_NOP)
				OP_CASE_NOP(ADD_NOP)
				OP_CASE_NOP(SUB_NOP)
				OP_CASE_NOP(SLL_NOP)
				OP_CASE_NOP(SLT_NOP)
				OP_CASE_NOP(SLTU_NOP)
				OP_CASE_NOP(XOR_NOP)
				OP_CASE_NOP(SRL_NOP)
				OP_CASE_NOP(SRA_NOP)
				OP_CASE_NOP(OR_NOP)
				OP_CASE_NOP(AND_NOP)
				OP_CASE_NOP(MUL_NOP)
				OP_CASE_NOP(MULH_NOP)
				OP_CASE_NOP(MULHSU_NOP)
				OP_CASE_NOP(MULHU_NOP)
				OP_CASE_NOP(DIV_NOP)
				OP_CASE_NOP(DIVU_NOP)
				OP_CASE_NOP(REM_NOP)
				OP_CASE_NOP(REMU_NOP)
				OP_CASE_NOP(ADDIW_NOP)
				OP_CASE_NOP(SLLIW_NOP)
				OP_CASE_NOP(SRLIW_NOP)
				OP_CASE_NOP(SRAIW_NOP)
				OP_CASE_NOP(ADDW_NOP)
				OP_CASE_NOP(SUBW_NOP)
				OP_CASE_NOP(SLLW_NOP)
				OP_CASE_NOP(SRLW_NOP)
				OP_CASE_NOP(SRAW_NOP)
				OP_CASE_NOP(MULW_NOP)
				OP_CASE_NOP(DIVW_NOP)
				OP_CASE_NOP(DIVUW_NOP)
				OP_CASE_NOP(REMW_NOP)
				OP_CASE_NOP(REMUW_NOP)
				OP_END(); /* needed for fallthrough (see definition of OP_CASE_NOP above) */

				OP_CASE(ADDI) {
					regs[instr.rd()] = regs[instr.rs1()] + instr.I_imm();
				}
				OP_END();

				OP_CASE(SLTI) {
					regs[instr.rd()] = static_cast<int64_t>(regs[instr.rs1()]) < instr.I_imm();
				}
				OP_END();

				OP_CASE(SLTIU) {
					regs[instr.rd()] = ((uxlen_t)regs[instr.rs1()]) < ((uxlen_t)instr.I_imm());
				}
				OP_END();

				OP_CASE(XORI) {
					regs[instr.rd()] = regs[instr.rs1()] ^ instr.I_imm();
				}
				OP_END();

				OP_CASE(ORI) {
					regs[instr.rd()] = regs[instr.rs1()] | instr.I_imm();
				}
				OP_END();

				OP_CASE(ANDI) {
					regs[instr.rd()] = regs[instr.rs1()] & instr.I_imm();
				}
				OP_END();

				OP_CASE(ADD) {
					regs[instr.rd()] = regs[instr.rs1()] + regs[instr.rs2()];
				}
				OP_END();

				OP_CASE(SUB) {
					regs[instr.rd()] = regs[instr.rs1()] - regs[instr.rs2()];
				}
				OP_END();

				OP_CASE(SLL) {
					regs[instr.rd()] = regs[instr.rs1()] << regs.shamt(instr.rs2());
				}
				OP_END();

				OP_CASE(SLT) {
					regs[instr.rd()] = regs[instr.rs1()] < regs[instr.rs2()];
				}
				OP_END();

				OP_CASE(SLTU) {
					regs[instr.rd()] = ((uxlen_t)regs[instr.rs1()]) < ((uxlen_t)regs[instr.rs2()]);
				}
				OP_END();

				OP_CASE(SRL) {
					regs[instr.rd()] = ((uxlen_t)regs[instr.rs1()]) >> regs.shamt(instr.rs2());
				}
				OP_END();

				OP_CASE(SRA) {
					regs[instr.rd()] = regs[instr.rs1()] >> regs.shamt(instr.rs2());
				}
				OP_END();

				OP_CASE(XOR) {
					regs[instr.rd()] = regs[instr.rs1()] ^ regs[instr.rs2()];
				}
				OP_END();

				OP_CASE(OR) {
					regs[instr.rd()] = regs[instr.rs1()] | regs[instr.rs2()];
				}
				OP_END();

				OP_CASE(AND) {
					regs[instr.rd()] = regs[instr.rs1()] & regs[instr.rs2()];
				}
				OP_END();

				OP_CASE(SLLI) {
					regs[instr.rd()] = regs[instr.rs1()] << instr.shamt();
				}
				OP_END();

				OP_CASE(SRLI) {
					regs[instr.rd()] = ((uxlen_t)regs[instr.rs1()]) >> instr.shamt();
				}
				OP_END();

				OP_CASE(SRAI) {
					regs[instr.rd()] = regs[instr.rs1()] >> instr.shamt();
				}
				OP_END();

				OP_CASE(LUI) {
					regs[instr.rd()] = instr.U_imm();
				}
				OP_END();

				OP_CASE(AUIPC) {
					if (pc->cap.fields.flag_cap_mode) {
						uint64_t off = instr.U_imm();
						Capability newCap = pc;
						bool representable = newCap.setCapAddr(dbbcache.get_last_pc_before_callback() + off);
						newCap.clearTagIf(!representable);
						regs[instr.rd()] = newCap;
						OP_END();
					}
					// Integer Pointer mode
					regs[instr.rd()] = dbbcache.get_last_pc_before_callback().pcc + instr.U_imm();
					regs[instr.rd()].clearMetadata();
				}
				OP_END();

				OP_CASE(J) {
					// TODO: Check if this requires cheri

					stats.inc_j();
					try {
						dbbcache.jump(instr.J_imm());
					} catch (SimulationTrap t) {
						raise_trap(t.reason, instr.rd(), &rvfi_dii_output);
					}

					if (unlikely(ninstr > fast_quantum_ins_granularity)) {
						ninstr++;
						goto OP_LABEL(op_global_fdd);
					}
				}
				OP_END();

				OP_CASE(JAL) {
					stats.inc_jal();
					try {
						regs[instr.rd()] = dbbcache.jump_and_link(instr.J_imm()).pcc;
					} catch (SimulationTrap t) {
						raise_trap(t.reason, instr.rd(), &rvfi_dii_output);
					}
					if (unlikely(ninstr > fast_quantum_ins_granularity)) {
						ninstr++;
						goto OP_LABEL(op_global_fdd);
					}
				}
				OP_END();

				OP_CASE(JR) {
					stats.inc_jr();
					if (pc->cap.fields.flag_cap_mode) {
						// TODO, does work as rd = 0, but could also use an optimized version
						execute_c_jalr(instr.I_imm());
					} else {
						uxlen_t new_pc = (regs[instr.rs1()] + instr.I_imm()) & ~1;
						cheriControlCheckAddr(new_pc, pc.pcc, &rvfi_dii_output, min_instruction_bytes());
						if (unlikely((new_pc & 0x3) && (!csrs.misa.has_C_extension()))) {
							// NOTE: misaligned instruction address not possible on machines supporting compressed
							// instructions
							raise_trap(EXC_INSTR_ADDR_MISALIGNED, new_pc, &rvfi_dii_output);
						}
						try {
							dbbcache.jump_dyn(new_pc);
						} catch (SimulationTrap t) {
							raise_trap(t.reason, instr.rd(), &rvfi_dii_output);
						}

						if (unlikely(ninstr > fast_quantum_ins_granularity)) {
							ninstr++;
							goto OP_LABEL(op_global_fdd);
						}
					}
				}
				OP_END();

				OP_CASE(JALR) {
					stats.inc_jalr();
					if (pc->cap.fields.flag_cap_mode) {
						execute_c_jalr(instr.I_imm());
					} else {
						uxlen_t new_pc = (regs[instr.rs1()] + instr.I_imm()) & ~1;
						cheriControlCheckAddr(new_pc, pc.pcc, &rvfi_dii_output, min_instruction_bytes());
						if (unlikely((new_pc & 0x3) && (!csrs.misa.has_C_extension()))) {
							// NOTE: misaligned instruction address not possible on machines supporting compressed
							// instructions
							raise_trap(EXC_INSTR_ADDR_MISALIGNED, new_pc, &rvfi_dii_output);
						}
						try {
							regs[instr.rd()] = dbbcache.jump_dyn_and_link(new_pc, false).pcc;
						} catch (SimulationTrap t) {
							raise_trap(t.reason, instr.rd(), &rvfi_dii_output);
						}
					}

					if (unlikely(ninstr > fast_quantum_ins_granularity)) {
						ninstr++;
						goto OP_LABEL(op_global_fdd);
					}
				}
				OP_END();

				OP_CASE(SB) {
					Capability auth_val;
					uint64_t vaddr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), instr.S_imm(), &auth_val, &vaddr);
					mem->handle_store_data_via_cap(regs[instr.rs2()], auth_idx, auth_val, vaddr, 1);
				}
				OP_END();

				OP_CASE(SH) {
					Capability auth_val;
					uint64_t vaddr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), instr.S_imm(), &auth_val, &vaddr);
					mem->handle_store_data_via_cap(regs[instr.rs2()], auth_idx, auth_val, vaddr, 2);
				}
				OP_END();

				OP_CASE(SW) {
					Capability auth_val;
					uint64_t vaddr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), instr.S_imm(), &auth_val, &vaddr);
					mem->handle_store_data_via_cap(regs[instr.rs2()], auth_idx, auth_val, vaddr, 4);
				}
				OP_END();

				OP_CASE(SD) {
					Capability auth_val;
					uint64_t vaddr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), instr.S_imm(), &auth_val, &vaddr);
					mem->handle_store_data_via_cap(regs[instr.rs2()], auth_idx, auth_val, vaddr, 8);
				}
				OP_END();

				OP_CASE(LB) {
					stats.inc_loadstore();
					Capability auth_val;
					uint64_t vaddr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), instr.I_imm(), &auth_val, &vaddr);
					regs[instr.rd()] = mem->handle_load_data_via_cap(auth_idx, auth_val, vaddr, false, 1);
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(LH) {
					stats.inc_loadstore();
					Capability auth_val;
					uint64_t vaddr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), instr.I_imm(), &auth_val, &vaddr);
					regs[instr.rd()] = mem->handle_load_data_via_cap(auth_idx, auth_val, vaddr, false, 2);
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(LW) {
					stats.inc_loadstore();
					Capability auth_val;
					uint64_t vaddr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), instr.I_imm(), &auth_val, &vaddr);
					regs[instr.rd()] = mem->handle_load_data_via_cap(auth_idx, auth_val, vaddr, false, 4);
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(LD) {
					stats.inc_loadstore();
					Capability auth_val;
					uint64_t vaddr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), instr.I_imm(), &auth_val, &vaddr);
					regs[instr.rd()] = mem->handle_load_data_via_cap(auth_idx, auth_val, vaddr, false, 8);
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(LBU) {
					stats.inc_loadstore();
					Capability auth_val;
					uint64_t vaddr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), instr.I_imm(), &auth_val, &vaddr);
					regs[instr.rd()] = mem->handle_load_data_via_cap(auth_idx, auth_val, vaddr, true, 1);
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(LHU) {
					stats.inc_loadstore();
					Capability auth_val;
					uint64_t vaddr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), instr.I_imm(), &auth_val, &vaddr);
					regs[instr.rd()] = mem->handle_load_data_via_cap(auth_idx, auth_val, vaddr, true, 2);
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(LWU) {
					stats.inc_loadstore();
					Capability auth_val;
					uint64_t vaddr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), instr.I_imm(), &auth_val, &vaddr);
					regs[instr.rd()] = mem->handle_load_data_via_cap(auth_idx, auth_val, vaddr, true, 4);
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(BEQ) {
					if (regs[instr.rs1()].cap.fields.address == regs[instr.rs2()].cap.fields.address) {
						dbbcache.branch_taken(instr.B_imm());
						if (unlikely(ninstr > fast_quantum_ins_granularity)) {
							ninstr++;
							goto OP_LABEL(op_global_fdd);
						}
					} else {
						dbbcache.branch_not_taken(pc);
					}
				}
				OP_END();

				OP_CASE(BNE) {
					if (regs[instr.rs1()].cap.fields.address != regs[instr.rs2()].cap.fields.address) {
						dbbcache.branch_taken(instr.B_imm());
						if (unlikely(ninstr > fast_quantum_ins_granularity)) {
							ninstr++;
							goto OP_LABEL(op_global_fdd);
						}
					} else {
						dbbcache.branch_not_taken(pc);
					}
				}
				OP_END();

				OP_CASE(BLT) {
					if (regs[instr.rs1()] < regs[instr.rs2()]) {
						dbbcache.branch_taken(instr.B_imm());
						if (unlikely(ninstr > fast_quantum_ins_granularity)) {
							ninstr++;
							goto OP_LABEL(op_global_fdd);
						}
					} else {
						dbbcache.branch_not_taken(pc);
					}
				}
				OP_END();

				OP_CASE(BGE) {
					if (regs[instr.rs1()] >= regs[instr.rs2()]) {
						dbbcache.branch_taken(instr.B_imm());
						if (unlikely(ninstr > fast_quantum_ins_granularity)) {
							ninstr++;
							goto OP_LABEL(op_global_fdd);
						}
					} else {
						dbbcache.branch_not_taken(pc);
					}
				}
				OP_END();

				OP_CASE(BLTU) {
					if ((uxlen_t)regs[instr.rs1()].cap.fields.address < (uxlen_t)regs[instr.rs2()].cap.fields.address) {
						dbbcache.branch_taken(instr.B_imm());
						if (unlikely(ninstr > fast_quantum_ins_granularity)) {
							ninstr++;
							goto OP_LABEL(op_global_fdd);
						}
					} else {
						dbbcache.branch_not_taken(pc);
					}
				}
				OP_END();

				OP_CASE(BGEU) {
					if ((uxlen_t)regs[instr.rs1()].cap.fields.address >=
					    (uxlen_t)regs[instr.rs2()].cap.fields.address) {
						dbbcache.branch_taken(instr.B_imm());
						if (unlikely(ninstr > fast_quantum_ins_granularity)) {
							ninstr++;
							goto OP_LABEL(op_global_fdd);
						}
					} else {
						dbbcache.branch_not_taken(pc);
					}
				}
				OP_END();

				/* rd != x0/zero variants */
				OP_CASE(ADDIW) {
					regs[instr.rd()] = (int32_t)regs[instr.rs1()] + (int32_t)instr.I_imm();
				}
				OP_END();

				OP_CASE(SLLIW) {
					regs[instr.rd()] = (int32_t)((uint32_t)regs[instr.rs1()] << instr.shamt_w());
				}
				OP_END();

				OP_CASE(SRLIW) {
					regs[instr.rd()] = (int32_t)(((uint32_t)regs[instr.rs1()]) >> instr.shamt_w());
				}
				OP_END();

				OP_CASE(SRAIW) {
					regs[instr.rd()] = (int32_t)((int32_t)regs[instr.rs1()] >> instr.shamt_w());
				}
				OP_END();

				OP_CASE(ADDW) {
					regs[instr.rd()] = (int32_t)regs[instr.rs1()] + (int32_t)regs[instr.rs2()];
				}
				OP_END();

				OP_CASE(SUBW) {
					regs[instr.rd()] = (int32_t)regs[instr.rs1()] - (int32_t)regs[instr.rs2()];
				}
				OP_END();

				OP_CASE(SLLW) {
					regs[instr.rd()] = (int32_t)((uint32_t)regs[instr.rs1()] << regs.shamt_w(instr.rs2()));
				}
				OP_END();

				OP_CASE(SRLW) {
					regs[instr.rd()] = (int32_t)(((uint32_t)regs[instr.rs1()]) >> regs.shamt_w(instr.rs2()));
				}
				OP_END();

				OP_CASE(SRAW) {
					regs[instr.rd()] = (int32_t)((int32_t)regs[instr.rs1()] >> regs.shamt_w(instr.rs2()));
				}
				OP_END();

				OP_CASE(FENCE) {
					lscache.fence();
				}
				OP_END();

				OP_CASE(FENCE_I) {
					dbbcache.fence_i(pc);
					stats.inc_fence_i();
				}
				OP_END();

				OP_CASE(ECALL) {
					if (sys) {
						sys->execute_syscall(this);
					} else {
						uxlen_t last_pc = dbbcache.get_last_pc_before_callback();
						switch (prv) {
							case MachineMode:
								raise_trap(EXC_ECALL_M_MODE, last_pc, &rvfi_dii_output);
								break;
							case SupervisorMode:
								raise_trap(EXC_ECALL_S_MODE, last_pc, &rvfi_dii_output);
								break;
							case UserMode:
								raise_trap(EXC_ECALL_U_MODE, last_pc, &rvfi_dii_output);
								break;
							default:
								throw std::runtime_error("unknown privilege level " + std::to_string(prv));
						}
					}
				}
				OP_END();

				OP_CASE(EBREAK) {
					if (debug_mode && !rvfi_dii) {
						// set_status(CoreExecStatus::HitBreakpoint);
						raise_trap(EXC_BREAKPOINT, dbbcache.get_last_pc_before_callback(), &rvfi_dii_output);
					} else {
						// TODO: also raise trap if we are in debug mode?
						raise_trap(EXC_BREAKPOINT, dbbcache.get_last_pc_before_callback(), &rvfi_dii_output);
					}
				}
				OP_END();

				OP_CASE(CSRRW) {
					stats.inc_csr();
					auto addr = instr.csr();
					if (is_invalid_csr_access(addr, true)) {
						RAISE_ILLEGAL_INSTRUCTION();
					} else {
						auto rd = instr.rd();
						auto rs1_val = regs[instr.rs1()];
						if (rd != RegFile::zero) {
							commit_instructions(ninstr);
							regs[instr.rd()] = get_csr_value(addr);
						}
						set_csr_value(addr, rs1_val);
					}
				}
				OP_END();

				OP_CASE(CSRRS) {
					stats.inc_csr();
					auto addr = instr.csr();
					auto rs1 = instr.rs1();
					auto write = rs1 != RegFile::zero;
					if (is_invalid_csr_access(addr, write)) {
						RAISE_ILLEGAL_INSTRUCTION();
					} else {
						auto rd = instr.rd();
						auto rs1_val = regs[rs1];
						commit_instructions(ninstr);
						auto csr_val = get_csr_value(addr);
						if (rd != RegFile::zero)
							regs[rd] = csr_val;
						if (write)
							set_csr_value(addr, csr_val | rs1_val);
					}
				}
				OP_END();

				OP_CASE(CSRRC) {
					stats.inc_csr();
					auto addr = instr.csr();
					auto rs1 = instr.rs1();
					auto write = rs1 != RegFile::zero;
					if (is_invalid_csr_access(addr, write)) {
						RAISE_ILLEGAL_INSTRUCTION();
					} else {
						auto rd = instr.rd();
						auto rs1_val = regs[rs1];
						commit_instructions(ninstr);
						auto csr_val = get_csr_value(addr);
						if (rd != RegFile::zero)
							regs[rd] = csr_val;
						if (write)
							set_csr_value(addr, csr_val & ~rs1_val);
					}
				}
				OP_END();

				OP_CASE(CSRRWI) {
					stats.inc_csr();
					auto addr = instr.csr();
					if (is_invalid_csr_access(addr, true)) {
						RAISE_ILLEGAL_INSTRUCTION();
					} else {
						auto rd = instr.rd();
						if (rd != RegFile::zero) {
							commit_instructions(ninstr);
							regs[rd] = get_csr_value(addr);
						}
						set_csr_value(addr, instr.zimm());
					}
				}
				OP_END();

				OP_CASE(CSRRSI) {
					stats.inc_csr();
					auto addr = instr.csr();
					auto zimm = instr.zimm();
					auto write = zimm != 0;
					if (is_invalid_csr_access(addr, write)) {
						RAISE_ILLEGAL_INSTRUCTION();
					} else {
						commit_instructions(ninstr);
						auto csr_val = get_csr_value(addr);
						auto rd = instr.rd();
						if (rd != RegFile::zero)
							regs[rd] = csr_val;
						if (write)
							set_csr_value(addr, csr_val | zimm);
					}
				}
				OP_END();

				OP_CASE(CSRRCI) {
					stats.inc_csr();
					auto addr = instr.csr();
					auto zimm = instr.zimm();
					auto write = zimm != 0;
					if (is_invalid_csr_access(addr, write)) {
						RAISE_ILLEGAL_INSTRUCTION();
					} else {
						commit_instructions(ninstr);
						auto csr_val = get_csr_value(addr);
						auto rd = instr.rd();
						if (rd != RegFile::zero)
							regs[rd] = csr_val;
						if (write)
							set_csr_value(addr, csr_val & ~((uint64_t)zimm));
					}
				}
				OP_END();

				/* rd != x0/zero variants */
				OP_CASE(MUL) {
					int128_t ans = (int128_t)regs[instr.rs1()] * (int128_t)regs[instr.rs2()];
					regs[instr.rd()] = (int64_t)ans;
				}
				OP_END();

				OP_CASE(MULH) {
					int128_t ans = (int128_t)regs[instr.rs1()] * (int128_t)regs[instr.rs2()];
					regs[instr.rd()] = ans >> 64;
				}
				OP_END();

				OP_CASE(MULHU) {
					int128_t ans = ((uint128_t)(uxlen_t)regs[instr.rs1()]) * (uint128_t)((uxlen_t)regs[instr.rs2()]);
					regs[instr.rd()] = ans >> 64;
				}
				OP_END();

				OP_CASE(MULHSU) {
					int128_t ans = (int128_t)regs[instr.rs1()] * (uint128_t)((uxlen_t)regs[instr.rs2()]);
					regs[instr.rd()] = ans >> 64;
				}
				OP_END();

				OP_CASE(DIV) {
					auto a = regs[instr.rs1()];
					auto b = regs[instr.rs2()];
					if (b == 0) {
						regs[instr.rd()] = -1;
					} else if (a == REG_MIN && b == -1) {
						regs[instr.rd()] = a;
					} else {
						regs[instr.rd()] = a / b;
					}
				}
				OP_END();

				OP_CASE(DIVU) {
					auto a = regs[instr.rs1()];
					auto b = regs[instr.rs2()];
					if (b == 0) {
						regs[instr.rd()] = -1;
					} else {
						regs[instr.rd()] = (uxlen_t)a / (uxlen_t)b;
					}
				}
				OP_END();

				OP_CASE(REM) {
					auto a = regs[instr.rs1()];
					auto b = regs[instr.rs2()];
					if (b == 0) {
						regs[instr.rd()] = a;
					} else if (a == REG_MIN && b == -1) {
						regs[instr.rd()] = 0;
					} else {
						regs[instr.rd()] = a % b;
					}
				}
				OP_END();

				OP_CASE(REMU) {
					auto a = regs[instr.rs1()];
					auto b = regs[instr.rs2()];
					if (b == 0) {
						regs[instr.rd()] = a;
					} else {
						regs[instr.rd()] = (uxlen_t)a % (uxlen_t)b;
					}
				}
				OP_END();

				OP_CASE(MULW) {
					regs[instr.rd()] = (int32_t)(regs[instr.rs1()] * regs[instr.rs2()]);
				}
				OP_END();

				OP_CASE(DIVW) {
					int32_t a = regs[instr.rs1()];
					int32_t b = regs[instr.rs2()];
					if (b == 0) {
						regs[instr.rd()] = -1;
					} else if (a == REG32_MIN && b == -1) {
						regs[instr.rd()] = a;
					} else {
						regs[instr.rd()] = a / b;
					}
				}
				OP_END();

				OP_CASE(DIVUW) {
					int32_t a = regs[instr.rs1()];
					int32_t b = regs[instr.rs2()];
					if (b == 0) {
						regs[instr.rd()] = -1;
					} else {
						regs[instr.rd()] = (int32_t)((uint32_t)a / (uint32_t)b);
					}
				}
				OP_END();

				OP_CASE(REMW) {
					int32_t a = regs[instr.rs1()];
					int32_t b = regs[instr.rs2()];
					if (b == 0) {
						regs[instr.rd()] = a;
					} else if (a == REG32_MIN && b == -1) {
						regs[instr.rd()] = 0;
					} else {
						regs[instr.rd()] = a % b;
					}
				}
				OP_END();

				OP_CASE(REMUW) {
					int32_t a = regs[instr.rs1()];
					int32_t b = regs[instr.rs2()];
					if (b == 0) {
						regs[instr.rd()] = a;
					} else {
						regs[instr.rd()] = (int32_t)((uint32_t)a % (uint32_t)b);
					}
				}
				OP_END();

				/*
				 * A Extension
				 * Note: handling of x0/zero
				 *  * for AMO instructoins is done in execute_amo* implementations -> writes to zero/x0 are ignored
				 *  * for lr/sc instructions is done within case -> zero/x0 is reset to zero after op
				 */
				OP_CASE(LR_W) {
					stats.inc_loadstore();
					uxlen_t addr;
					Capability auth_val;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), 0, &auth_val, &addr);
					trap_check_addr_alignment<4, true>(addr);
					regs[instr.rd()] = mem->atomic_load_reserved_data_via_cap(auth_idx, auth_val, addr, 4);
					if (lr_sc_counter == 0) {
						lr_sc_counter = 17;  // this instruction + 16 additional ones, (an over-approximation) to
						// cover the RISC-V forward progress property
						force_slow_path();
					}
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(SC_W) {
					stats.inc_loadstore();
					uxlen_t addr;
					Capability auth_val;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), 0, &auth_val, &addr);
					trap_check_addr_alignment<4, false>(addr);
					regs[instr.rd()] = 1;  // failure by default (in case a trap is thrown)
					regs[instr.rd()] =
					    mem->atomic_store_conditional_data_via_cap(regs[instr.rs2()], auth_idx, auth_val, addr, 4)
					        ? 0
					        : 1;  // overwrite result (in case no trap is thrown)
					lr_sc_counter = 0;
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(AMOSWAP_W) {
					execute_amo_w(instr, [](int32_t a, int32_t b) {
						(void)a;
						return b;
					});
				}
				OP_END();

				OP_CASE(AMOADD_W) {
					execute_amo_w(instr, [](int32_t a, int32_t b) { return a + b; });
				}
				OP_END();

				OP_CASE(AMOXOR_W) {
					execute_amo_w(instr, [](int32_t a, int32_t b) { return a ^ b; });
				}
				OP_END();

				OP_CASE(AMOAND_W) {
					execute_amo_w(instr, [](int32_t a, int32_t b) { return a & b; });
				}
				OP_END();

				OP_CASE(AMOOR_W) {
					execute_amo_w(instr, [](int32_t a, int32_t b) { return a | b; });
				}
				OP_END();

				OP_CASE(AMOMIN_W) {
					execute_amo_w(instr, [](int32_t a, int32_t b) { return std::min(a, b); });
				}
				OP_END();

				OP_CASE(AMOMINU_W) {
					execute_amo_w(instr, [](int32_t a, int32_t b) { return std::min((uint32_t)a, (uint32_t)b); });
				}
				OP_END();

				OP_CASE(AMOMAX_W) {
					execute_amo_w(instr, [](int32_t a, int32_t b) { return std::max(a, b); });
				}
				OP_END();

				OP_CASE(AMOMAXU_W) {
					execute_amo_w(instr, [](int32_t a, int32_t b) { return std::max((uint32_t)a, (uint32_t)b); });
				}
				OP_END();

				OP_CASE(LR_D) {
					stats.inc_loadstore();
					uxlen_t addr;
					Capability auth_val;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), 0, &auth_val, &addr);
					trap_check_addr_alignment<8, true>(addr);
					regs[instr.rd()] = mem->atomic_load_reserved_data_via_cap(auth_idx, auth_val, addr, 8);
					if (lr_sc_counter == 0) {
						lr_sc_counter = 17;  // this instruction + 16 additional ones, (an over-approximation) to
						// cover the RISC-V forward progress property
						force_slow_path();
					}
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(SC_D) {
					stats.inc_loadstore();
					Capability auth_val;
					uxlen_t addr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), 0, &auth_val, &addr);
					trap_check_addr_alignment<8, false>(addr);
					regs[instr.rd()] = 1;  // failure by default (in case a trap is thrown)
					regs[instr.rd()] =
					    mem->atomic_store_conditional_data_via_cap(regs[instr.rs2()], auth_idx, auth_val, addr, 8)
					        ? 0
					        : 1;  // overwrite result (in case no trap is thrown)
					lr_sc_counter = 0;
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(AMOSWAP_D) {
					execute_amo_d(instr, [](int64_t a, int64_t b) {
						(void)a;
						return b;
					});
				}
				OP_END();

				OP_CASE(AMOADD_D) {
					execute_amo_d(instr, [](int64_t a, int64_t b) { return a + b; });
				}
				OP_END();

				OP_CASE(AMOXOR_D) {
					execute_amo_d(instr, [](int64_t a, int64_t b) { return a ^ b; });
				}
				OP_END();

				OP_CASE(AMOAND_D) {
					execute_amo_d(instr, [](int64_t a, int64_t b) { return a & b; });
				}
				OP_END();

				OP_CASE(AMOOR_D) {
					execute_amo_d(instr, [](int64_t a, int64_t b) { return a | b; });
				}
				OP_END();

				OP_CASE(AMOMIN_D) {
					execute_amo_d(instr, [](int64_t a, int64_t b) { return std::min(a, b); });
				}
				OP_END();

				OP_CASE(AMOMINU_D) {
					execute_amo_d(instr, [](int64_t a, int64_t b) { return std::min((uint64_t)a, (uint64_t)b); });
				}
				OP_END();

				OP_CASE(AMOMAX_D) {
					execute_amo_d(instr, [](int64_t a, int64_t b) { return std::max(a, b); });
				}
				OP_END();

				OP_CASE(AMOMAXU_D) {
					execute_amo_d(instr, [](int64_t a, int64_t b) { return std::max((uint64_t)a, (uint64_t)b); });
				}
				OP_END();

				// RV Zfh extension

				OP_CASE(FLH) {
					uint64_t addr = regs[instr.rs1()] + instr.I_imm();
					trap_check_addr_alignment<2, true>(addr);
					fp_regs.write(RD, float16_t{(uint16_t)lscache.load_uhalf(addr)});
				}
				OP_END();

				OP_CASE(FSH) {
					uint64_t addr = regs[instr.rs1()] + instr.S_imm();
					trap_check_addr_alignment<2, false>(addr);
					lscache.store_half(addr, fp_regs.f16(RS2).v);
				}
				OP_END();

				OP_CASE(FADD_H) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f16_add(fp_regs.f16(RS1), fp_regs.f16(RS2)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FSUB_H) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f16_sub(fp_regs.f16(RS1), fp_regs.f16(RS2)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FMUL_H) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f16_mul(fp_regs.f16(RS1), fp_regs.f16(RS2)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FDIV_H) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f16_div(fp_regs.f16(RS1), fp_regs.f16(RS2)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FSQRT_H) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f16_sqrt(fp_regs.f16(RS1)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FMIN_H) {
					fp_prepare_instr();

					bool rs1_smaller = f16_lt_quiet(fp_regs.f16(RS1), fp_regs.f16(RS2)) ||
					                   (f16_eq(fp_regs.f16(RS1), fp_regs.f16(RS2)) && f16_isNegative(fp_regs.f16(RS1)));

					if (f16_isNaN(fp_regs.f16(RS1)) && f16_isNaN(fp_regs.f16(RS2))) {
						fp_regs.write(RD, f16_defaultNaN);
					} else {
						if (rs1_smaller)
							fp_regs.write(RD, fp_regs.f16(RS1));
						else
							fp_regs.write(RD, fp_regs.f16(RS2));
					}

					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FMAX_H) {
					fp_prepare_instr();

					bool rs1_greater = f16_lt_quiet(fp_regs.f16(RS2), fp_regs.f16(RS1)) ||
					                   (f16_eq(fp_regs.f16(RS2), fp_regs.f16(RS1)) && f16_isNegative(fp_regs.f16(RS2)));

					if (f16_isNaN(fp_regs.f16(RS1)) && f16_isNaN(fp_regs.f16(RS2))) {
						fp_regs.write(RD, f16_defaultNaN);
					} else {
						if (rs1_greater)
							fp_regs.write(RD, fp_regs.f16(RS1));
						else
							fp_regs.write(RD, fp_regs.f16(RS2));
					}

					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FMADD_H) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f16_mulAdd(fp_regs.f16(RS1), fp_regs.f16(RS2), fp_regs.f16(RS3)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FMSUB_H) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f16_mulAdd(fp_regs.f16(RS1), fp_regs.f16(RS2), f16_neg(fp_regs.f16(RS3))));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FNMADD_H) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD,
					              f16_mulAdd(f16_neg(fp_regs.f16(RS1)), fp_regs.f16(RS2), f16_neg(fp_regs.f16(RS3))));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FNMSUB_H) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f16_mulAdd(f16_neg(fp_regs.f16(RS1)), fp_regs.f16(RS2), fp_regs.f16(RS3)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FSGNJ_H) {
					fp_prepare_instr();
					auto f1 = fp_regs.f16(RS1);
					auto f2 = fp_regs.f16(RS2);
					uint16_t a = (f1.v & ~F16_SIGN_BIT) | (f2.v & F16_SIGN_BIT);
					fp_regs.write(RD, float16_t{a});
					fp_set_dirty();
				}
				OP_END();

				OP_CASE(FSGNJN_H) {
					fp_prepare_instr();
					auto f1 = fp_regs.f16(RS1);
					auto f2 = fp_regs.f16(RS2);
					uint16_t a = (f1.v & ~F16_SIGN_BIT) | (~f2.v & F16_SIGN_BIT);
					fp_regs.write(RD, float16_t{a});
					fp_set_dirty();
				}
				OP_END();

				OP_CASE(FSGNJX_H) {
					fp_prepare_instr();
					auto f1 = fp_regs.f16(RS1);
					auto f2 = fp_regs.f16(RS2);
					uint16_t a = f1.v ^ (f2.v & F16_SIGN_BIT);
					fp_regs.write(RD, float16_t{a});
					fp_set_dirty();
				}
				OP_END();

				OP_CASE(FEQ_H) {
					fp_prepare_instr();
					regs[RD] = f16_eq(fp_regs.f16(RS1), fp_regs.f16(RS2));
					fp_update_exception_flags();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FLT_H) {
					fp_prepare_instr();
					regs[RD] = f16_lt(fp_regs.f16(RS1), fp_regs.f16(RS2));
					fp_update_exception_flags();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FLE_H) {
					fp_prepare_instr();
					regs[RD] = f16_le(fp_regs.f16(RS1), fp_regs.f16(RS2));
					fp_update_exception_flags();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FCLASS_H) {
					fp_prepare_instr();
					regs[RD] = (int16_t)f16_classify(fp_regs.f16(RS1));
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FMV_H_X) {
					fp_prepare_instr();
					fp_regs.write(RD, float16_t{(uint16_t)regs[RS1]});
					fp_set_dirty();
				}
				OP_END();

				OP_CASE(FMV_X_H) {
					fp_prepare_instr();
					regs[RD] = fp_regs.f16(RS1).v;
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FCVT_W_H) {
					fp_prepare_instr();
					fp_setup_rm();
					regs[RD] = f16_to_i32(fp_regs.f16(RS1), softfloat_roundingMode, true);
					fp_finish_instr();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FCVT_WU_H) {
					fp_prepare_instr();
					fp_setup_rm();
					regs[RD] = (int32_t)f16_to_ui32(fp_regs.f16(RS1), softfloat_roundingMode, true);
					fp_finish_instr();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FCVT_H_W) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, i32_to_f16((int32_t)regs[RS1]));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FCVT_H_WU) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, ui32_to_f16((int32_t)regs[RS1]));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FCVT_S_H) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f16_to_f32(fp_regs.f16(RS1)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FCVT_H_S) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f32_to_f16(fp_regs.f32(RS1)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FCVT_H_D) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f64_to_f16(fp_regs.f64(RS1)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FCVT_D_H) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f16_to_f64(fp_regs.f16(RS1)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FCVT_L_H) {
					fp_prepare_instr();
					fp_setup_rm();
					regs[RD] = f16_to_i64(fp_regs.f16(RS1), softfloat_roundingMode, true);
					fp_finish_instr();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FCVT_LU_H) {
					fp_prepare_instr();
					fp_setup_rm();
					regs[RD] = f16_to_ui64(fp_regs.f16(RS1), softfloat_roundingMode, true);
					fp_finish_instr();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FCVT_H_L) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, i64_to_f16(regs[RS1]));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FCVT_H_LU) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, ui64_to_f16(regs[RS1]));
					fp_finish_instr();
				}
				OP_END();

				// RV64 F/D extension

				OP_CASE(FLW) {
					stats.inc_loadstore();
					uxlen_t addr = regs[instr.rs1()] + instr.I_imm();
					trap_check_addr_alignment<4, true>(addr);
					fp_regs.write(RD, float32_t{(uint32_t)lscache.load_uword(addr)});
				}
				OP_END();

				OP_CASE(FSW) {
					stats.inc_loadstore();
					uxlen_t addr = regs[instr.rs1()] + instr.S_imm();
					trap_check_addr_alignment<4, false>(addr);
					lscache.store_word(addr, fp_regs.u32(RS2));
				}
				OP_END();

				OP_CASE(FADD_S) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f32_add(fp_regs.f32(RS1), fp_regs.f32(RS2)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FSUB_S) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f32_sub(fp_regs.f32(RS1), fp_regs.f32(RS2)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FMUL_S) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f32_mul(fp_regs.f32(RS1), fp_regs.f32(RS2)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FDIV_S) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f32_div(fp_regs.f32(RS1), fp_regs.f32(RS2)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FSQRT_S) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f32_sqrt(fp_regs.f32(RS1)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FMIN_S) {
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
				}
				OP_END();

				OP_CASE(FMAX_S) {
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
				}
				OP_END();

				OP_CASE(FMADD_S) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f32_mulAdd(fp_regs.f32(RS1), fp_regs.f32(RS2), fp_regs.f32(RS3)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FMSUB_S) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f32_mulAdd(fp_regs.f32(RS1), fp_regs.f32(RS2), f32_neg(fp_regs.f32(RS3))));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FNMADD_S) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD,
					              f32_mulAdd(f32_neg(fp_regs.f32(RS1)), fp_regs.f32(RS2), f32_neg(fp_regs.f32(RS3))));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FNMSUB_S) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f32_mulAdd(f32_neg(fp_regs.f32(RS1)), fp_regs.f32(RS2), fp_regs.f32(RS3)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FCVT_W_S) {
					fp_prepare_instr();
					fp_setup_rm();
					regs[RD] = f32_to_i32(fp_regs.f32(RS1), softfloat_roundingMode, true);
					fp_finish_instr();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FCVT_WU_S) {
					fp_prepare_instr();
					fp_setup_rm();
					regs[RD] = (int32_t)f32_to_ui32(fp_regs.f32(RS1), softfloat_roundingMode, true);
					fp_finish_instr();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FCVT_S_W) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, i32_to_f32((int32_t)regs[RS1]));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FCVT_S_WU) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, ui32_to_f32((int32_t)regs[RS1]));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FSGNJ_S) {
					fp_prepare_instr();
					auto f1 = fp_regs.f32(RS1);
					auto f2 = fp_regs.f32(RS2);
					fp_regs.write(RD, float32_t{(f1.v & ~F32_SIGN_BIT) | (f2.v & F32_SIGN_BIT)});
					fp_set_dirty();
				}
				OP_END();

				OP_CASE(FSGNJN_S) {
					fp_prepare_instr();
					auto f1 = fp_regs.f32(RS1);
					auto f2 = fp_regs.f32(RS2);
					fp_regs.write(RD, float32_t{(f1.v & ~F32_SIGN_BIT) | (~f2.v & F32_SIGN_BIT)});
					fp_set_dirty();
				}
				OP_END();

				OP_CASE(FSGNJX_S) {
					fp_prepare_instr();
					auto f1 = fp_regs.f32(RS1);
					auto f2 = fp_regs.f32(RS2);
					fp_regs.write(RD, float32_t{f1.v ^ (f2.v & F32_SIGN_BIT)});
					fp_set_dirty();
				}
				OP_END();

				OP_CASE(FMV_W_X) {
					fp_prepare_instr();
					fp_regs.write(RD, float32_t{(uint32_t)((int32_t)regs[RS1])});
					fp_set_dirty();
				}
				OP_END();

				OP_CASE(FMV_X_W) {
					fp_prepare_instr();
					regs[RD] = (int32_t)fp_regs.u32(RS1);
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FEQ_S) {
					fp_prepare_instr();
					regs[RD] = f32_eq(fp_regs.f32(RS1), fp_regs.f32(RS2));
					fp_update_exception_flags();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FLT_S) {
					fp_prepare_instr();
					regs[RD] = f32_lt(fp_regs.f32(RS1), fp_regs.f32(RS2));
					fp_update_exception_flags();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FLE_S) {
					fp_prepare_instr();
					regs[RD] = f32_le(fp_regs.f32(RS1), fp_regs.f32(RS2));
					fp_update_exception_flags();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FCLASS_S) {
					fp_prepare_instr();
					regs[RD] = (int32_t)f32_classify(fp_regs.f32(RS1));
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FCVT_L_S) {
					fp_prepare_instr();
					fp_setup_rm();
					regs[RD] = f32_to_i64(fp_regs.f32(RS1), softfloat_roundingMode, true);
					fp_finish_instr();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FCVT_LU_S) {
					fp_prepare_instr();
					fp_setup_rm();
					regs[RD] = f32_to_ui64(fp_regs.f32(RS1), softfloat_roundingMode, true);
					fp_finish_instr();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FCVT_S_L) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, i64_to_f32(regs[RS1]));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FCVT_S_LU) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, ui64_to_f32(regs[RS1]));
					fp_finish_instr();
				}
				OP_END();

				// RV32D Extension

				OP_CASE(FLD) {
					stats.inc_loadstore();
					uxlen_t addr = regs[instr.rs1()] + instr.I_imm();
					trap_check_addr_alignment<8, true>(addr);
					fp_regs.write(RD, float64_t{(uint64_t)lscache.load_double(addr)});
				}
				OP_END();

				OP_CASE(FSD) {
					stats.inc_loadstore();
					uxlen_t addr = regs[instr.rs1()] + instr.S_imm();
					trap_check_addr_alignment<8, false>(addr);
					lscache.store_double(addr, fp_regs.f64(RS2).v);
				}
				OP_END();

				OP_CASE(FADD_D) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f64_add(fp_regs.f64(RS1), fp_regs.f64(RS2)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FSUB_D) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f64_sub(fp_regs.f64(RS1), fp_regs.f64(RS2)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FMUL_D) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f64_mul(fp_regs.f64(RS1), fp_regs.f64(RS2)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FDIV_D) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f64_div(fp_regs.f64(RS1), fp_regs.f64(RS2)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FSQRT_D) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f64_sqrt(fp_regs.f64(RS1)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FMIN_D) {
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
				}
				OP_END();

				OP_CASE(FMAX_D) {
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
				}
				OP_END();

				OP_CASE(FMADD_D) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f64_mulAdd(fp_regs.f64(RS1), fp_regs.f64(RS2), fp_regs.f64(RS3)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FMSUB_D) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f64_mulAdd(fp_regs.f64(RS1), fp_regs.f64(RS2), f64_neg(fp_regs.f64(RS3))));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FNMADD_D) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD,
					              f64_mulAdd(f64_neg(fp_regs.f64(RS1)), fp_regs.f64(RS2), f64_neg(fp_regs.f64(RS3))));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FNMSUB_D) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f64_mulAdd(f64_neg(fp_regs.f64(RS1)), fp_regs.f64(RS2), fp_regs.f64(RS3)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FSGNJ_D) {
					fp_prepare_instr();
					auto f1 = fp_regs.f64(RS1);
					auto f2 = fp_regs.f64(RS2);
					fp_regs.write(RD, float64_t{(f1.v & ~F64_SIGN_BIT) | (f2.v & F64_SIGN_BIT)});
					fp_set_dirty();
				}
				OP_END();

				OP_CASE(FSGNJN_D) {
					fp_prepare_instr();
					auto f1 = fp_regs.f64(RS1);
					auto f2 = fp_regs.f64(RS2);
					fp_regs.write(RD, float64_t{(f1.v & ~F64_SIGN_BIT) | (~f2.v & F64_SIGN_BIT)});
					fp_set_dirty();
				}
				OP_END();

				OP_CASE(FSGNJX_D) {
					fp_prepare_instr();
					auto f1 = fp_regs.f64(RS1);
					auto f2 = fp_regs.f64(RS2);
					fp_regs.write(RD, float64_t{f1.v ^ (f2.v & F64_SIGN_BIT)});
					fp_set_dirty();
				}
				OP_END();

				OP_CASE(FEQ_D) {
					fp_prepare_instr();
					regs[RD] = f64_eq(fp_regs.f64(RS1), fp_regs.f64(RS2));
					fp_update_exception_flags();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FLT_D) {
					fp_prepare_instr();
					regs[RD] = f64_lt(fp_regs.f64(RS1), fp_regs.f64(RS2));
					fp_update_exception_flags();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FLE_D) {
					fp_prepare_instr();
					regs[RD] = f64_le(fp_regs.f64(RS1), fp_regs.f64(RS2));
					fp_update_exception_flags();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FCLASS_D) {
					fp_prepare_instr();
					regs[RD] = (int64_t)f64_classify(fp_regs.f64(RS1));
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FMV_D_X) {
					fp_prepare_instr();
					fp_regs.write(RD, float64_t{(uint64_t)regs[RS1]});
					fp_set_dirty();
				}
				OP_END();

				OP_CASE(FMV_X_D) {
					fp_prepare_instr();
					regs[RD] = fp_regs.f64(RS1).v;
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FCVT_W_D) {
					fp_prepare_instr();
					fp_setup_rm();
					regs[RD] = f64_to_i32(fp_regs.f64(RS1), softfloat_roundingMode, true);
					fp_finish_instr();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FCVT_WU_D) {
					fp_prepare_instr();
					fp_setup_rm();
					regs[RD] = (int32_t)f64_to_ui32(fp_regs.f64(RS1), softfloat_roundingMode, true);
					fp_finish_instr();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FCVT_D_W) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, i32_to_f64((int32_t)regs[RS1]));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FCVT_D_WU) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, ui32_to_f64((int32_t)regs[RS1]));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FCVT_S_D) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f64_to_f32(fp_regs.f64(RS1)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FCVT_D_S) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, f32_to_f64(fp_regs.f32(RS1)));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FCVT_L_D) {
					fp_prepare_instr();
					fp_setup_rm();
					regs[RD] = f64_to_i64(fp_regs.f64(RS1), softfloat_roundingMode, true);
					fp_finish_instr();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FCVT_LU_D) {
					fp_prepare_instr();
					fp_setup_rm();
					regs[RD] = f64_to_ui64(fp_regs.f64(RS1), softfloat_roundingMode, true);
					fp_finish_instr();
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(FCVT_D_L) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, i64_to_f64(regs[RS1]));
					fp_finish_instr();
				}
				OP_END();

				OP_CASE(FCVT_D_LU) {
					fp_prepare_instr();
					fp_setup_rm();
					fp_regs.write(RD, ui64_to_f64(regs[RS1]));
					fp_finish_instr();
				}
				OP_END();

				/*
				 * RV-V Extension
				 * Note: handling of x0/zero is done in v_ext implementation (writes to zero/x0 are ignored)
				 */
				OP_CASE(VSETVLI) {
					v_ext.prepInstr(true, false, false);
					v_ext.v_set_operation(instr.rd(), instr.rs1(), instr.zimm_10(), 0);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSETIVLI) {
					v_ext.prepInstr(true, false, false);
					v_ext.v_set_operation(instr.rd(), 0, instr.zimm_9(), instr.rs1());
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSETVL) {
					v_ext.prepInstr(true, false, false);
					v_ext.v_set_operation(instr.rd(), instr.rs1(), regs[instr.rs2()], 0);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLM_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::masked);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSM_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::masked);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLE8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLE16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLE32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLE64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSE8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSE16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSE32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSE64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSE8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSE16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSE32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSE64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSE8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSE16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSE32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSE64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXEI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXEI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXEI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXEI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXEI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXEI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXEI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXEI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXEI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXEI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXEI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXEI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXEI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXEI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXEI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXEI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLE8FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLE16FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLE32FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLE64FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG2E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG2E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG2E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG2E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG2E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG2E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG2E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG2E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG2E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG2E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG2E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG2E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG2E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG2E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG2E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG2E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG2EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG2EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG2EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG2EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG2EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG2EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG2EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG2EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG2EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG2EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG2EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG2EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG2EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG2EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG2EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG2EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG2E8FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG2E16FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG2E32FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG2E64FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG3E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG3E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG3E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG3E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG3E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG3E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG3E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG3E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG3E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG3E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG3E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG3E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG3E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG3E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG3E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG3E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG3EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG3EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG3EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG3EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG3EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG3EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG3EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG3EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG3EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG3EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG3EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG3EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG3EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG3EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG3EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG3EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG3E8FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG3E16FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG3E32FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG3E64FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG4E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG4E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG4E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG4E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG4E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG4E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG4E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG4E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG4E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG4E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG4E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG4E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG4E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG4E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG4E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG4E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG4EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG4EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG4EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG4EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG4EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG4EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG4EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG4EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG4EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG4EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG4EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG4EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG4EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG4EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG4EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG4EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG4E8FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG4E16FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG4E32FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG4E64FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG5E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG5E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG5E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG5E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG5E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG5E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG5E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG5E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG5E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG5E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG5E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG5E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG5E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG5E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG5E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG5E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG5EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG5EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG5EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG5EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG5EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG5EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG5EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG5EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG5EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG5EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG5EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG5EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG5EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG5EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG5EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG5EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG5E8FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG5E16FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG5E32FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG5E64FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG6E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG6E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG6E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG6E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG6E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG6E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG6E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG6E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG6E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG6E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG6E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG6E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG6E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG6E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG6E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG6E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG6EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG6EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG6EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG6EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG6EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG6EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG6EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG6EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG6EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG6EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG6EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG6EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG6EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG6EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG6EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG6EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG6E8FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG6E16FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG6E32FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG6E64FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG7E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG7E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG7E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG7E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG7E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG7E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG7E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG7E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG7E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG7E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG7E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG7E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG7E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG7E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG7E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG7E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG7EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG7EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG7EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG7EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG7EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG7EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG7EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG7EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG7EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG7EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG7EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG7EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG7EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG7EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG7EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG7EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG7E8FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG7E16FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG7E32FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG7E64FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG8E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG8E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG8E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG8E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG8E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG8E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG8E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSEG8E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG8E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG8E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG8E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSSEG8E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG8E8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG8E16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG8E32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSSEG8E64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::standard_reg);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG8EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG8EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG8EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLUXSEG8EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG8EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG8EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG8EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLOXSEG8EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG8EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG8EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG8EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUXSEG8EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG8EI8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG8EI16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 16, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG8EI32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 32, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSOXSEG8EI64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 64, VExt::load_store_type_t::indexed);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG8E8FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG8E16FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG8E32FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VLSEG8E64FF_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, true, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::fofl);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VL1RE8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, false, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::whole);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VL1RE16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, false, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::whole);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VL1RE32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, false, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::whole);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VL1RE64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, false, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::whole);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VS1R_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, false, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::whole);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VL2RE8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, false, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::whole);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VL2RE16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, false, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::whole);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VL2RE32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, false, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::whole);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VL2RE64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, false, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::whole);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VS2R_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, false, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::whole);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VL4RE8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, false, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::whole);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VL4RE16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, false, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::whole);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VL4RE32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, false, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::whole);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VL4RE64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, false, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::whole);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VS4R_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, false, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::whole);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VL8RE8_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, false, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 8, VExt::load_store_type_t::whole);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VL8RE16_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, false, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 16, VExt::load_store_type_t::whole);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VL8RE32_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, false, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 32, VExt::load_store_type_t::whole);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VL8RE64_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, false, false);
					v_ext.vLoadStore(VExt::load_store_t::load, 64, VExt::load_store_type_t::whole);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VS8R_V) {
					stats.inc_loadstore();
					v_ext.prepInstr(true, false, false);
					v_ext.vLoadStore(VExt::load_store_t::store, 8, VExt::load_store_type_t::whole);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VADD_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VADD_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VADD_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUB_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSub(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSUB_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSub(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VRSUB_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vRSub(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VRSUB_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vRSub(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWADD_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWADD_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWSUB_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSub(), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWSUB_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSub(), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWADDU_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWADDU_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWSUBU_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSub(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWSUBU_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSub(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWADD_WV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::wwxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWADD_WX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::wwxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWSUB_WV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSub(), VExt::elem_sel_t::wwxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWSUB_WX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSub(), VExt::elem_sel_t::wwxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWADDU_WV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::wwxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWADDU_WX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAdd(), VExt::elem_sel_t::wwxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWSUBU_WV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSub(), VExt::elem_sel_t::wwxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWSUBU_WX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSub(), VExt::elem_sel_t::wwxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VZEXT_VF2) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vExt(2), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSEXT_VF2) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vExt(2), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VZEXT_VF4) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vExt(4), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSEXT_VF4) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vExt(4), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VZEXT_VF8) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vExt(8), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSEXT_VF8) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vExt(8), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VADC_VVM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopExtCarry(v_ext.vAdc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VADC_VXM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopExtCarry(v_ext.vAdc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VADC_VIM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopExtCarry(v_ext.vAdc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMADC_VVM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidAll(v_ext.vMadc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMADC_VXM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidAll(v_ext.vMadc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMADC_VIM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidAll(v_ext.vMadc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMADC_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidAll(v_ext.vMadc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMADC_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidAll(v_ext.vMadc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMADC_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidAll(v_ext.vMadc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSBC_VVM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopExtCarry(v_ext.vSbc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSBC_VXM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopExtCarry(v_ext.vSbc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSBC_VVM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidAll(v_ext.vMsbc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSBC_VXM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidAll(v_ext.vMsbc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSBC_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidAll(v_ext.vMsbc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSBC_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidAll(v_ext.vMsbc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VAND_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAnd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VAND_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAnd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VAND_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAnd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VOR_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vOr(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VOR_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vOr(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VOR_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vOr(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VXOR_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vXor(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VXOR_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vXor(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VXOR_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vXor(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSLL_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShift(false), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSLL_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShift(false), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSLL_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShift(false), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSRL_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSRL_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSRL_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSRA_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xxxssu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSRA_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xxxssu, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSRA_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xxxssu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VNSRL_WV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VNSRL_WI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VNSRL_WX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VNSRA_WV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xwxssu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VNSRA_WI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xwxssu, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VNSRA_WX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShift(true), VExt::elem_sel_t::xwxssu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSEQ_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::eq), VExt::elem_sel_t::xxxsss,
					                     VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSEQ_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::eq), VExt::elem_sel_t::xxxsss,
					                     VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSEQ_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::eq), VExt::elem_sel_t::xxxsss,
					                     VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSNE_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::ne), VExt::elem_sel_t::xxxsss,
					                     VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSNE_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::ne), VExt::elem_sel_t::xxxsss,
					                     VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSNE_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::ne), VExt::elem_sel_t::xxxsss,
					                     VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSLTU_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::lt), VExt::elem_sel_t::xxxuuu,
					                     VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSLTU_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::lt), VExt::elem_sel_t::xxxuuu,
					                     VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSLT_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::lt), VExt::elem_sel_t::xxxsss,
					                     VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSLT_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::lt), VExt::elem_sel_t::xxxsss,
					                     VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSLEU_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::le), VExt::elem_sel_t::xxxuuu,
					                     VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSLEU_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::le), VExt::elem_sel_t::xxxuuu,
					                     VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSLEU_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::le), VExt::elem_sel_t::xxxuus,
					                     VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSLE_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::le), VExt::elem_sel_t::xxxsss,
					                     VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSLE_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::le), VExt::elem_sel_t::xxxsss,
					                     VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSLE_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::le), VExt::elem_sel_t::xxxsss,
					                     VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSGTU_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::gt), VExt::elem_sel_t::xxxuuu,
					                     VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSGTU_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::gt), VExt::elem_sel_t::xxxuus,
					                     VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSGT_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::gt), VExt::elem_sel_t::xxxsss,
					                     VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSGT_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExtVoid(v_ext.vCompInt(VExt::int_compare_t::gt), VExt::elem_sel_t::xxxsss,
					                     VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMINU_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMin(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMINU_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMin(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMIN_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMin(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMIN_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMin(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMAXU_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMax(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMAXU_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMax(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMAX_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMax(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMAX_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMax(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMUL_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMul(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMUL_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMul(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMULH_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMulh(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMULH_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMulh(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMULHU_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMulh(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMULHU_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMulh(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMULHSU_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMulh(), VExt::elem_sel_t::xxxssu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMULHSU_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMulh(), VExt::elem_sel_t::xxxssu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VDIVU_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vDiv(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VDIVU_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vDiv(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VDIV_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vDiv(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VDIV_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vDiv(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VREMU_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vRem(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VREMU_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vRem(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VREM_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vRem(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VREM_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vRem(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWMUL_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMul(), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWMUL_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMul(), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWMULU_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMul(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWMULU_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMul(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWMULSU_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMul(), VExt::elem_sel_t::wxxusu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWMULSU_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMul(), VExt::elem_sel_t::wxxusu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMACC_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExt(v_ext.vMacc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMACC_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExt(v_ext.vMacc(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VNMSAC_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExt(v_ext.vNmsac(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VNMSAC_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExt(v_ext.vNmsac(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMADD_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExt(v_ext.vMadd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMADD_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExt(v_ext.vMadd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VNMSUB_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExt(v_ext.vNmsub(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VNMSUB_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExt(v_ext.vNmsub(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWMACCU_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExt(v_ext.vMacc(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWMACCU_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExt(v_ext.vMacc(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWMACC_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExt(v_ext.vMacc(), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWMACC_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExt(v_ext.vMacc(), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWMACCSU_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExt(v_ext.vMacc(), VExt::elem_sel_t::wxxuus, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWMACCSU_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExt(v_ext.vMacc(), VExt::elem_sel_t::wxxuus, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWMACCUS_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVdExt(v_ext.vMacc(), VExt::elem_sel_t::wxxusu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMERGE_VVM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopExtCarry(v_ext.vMerge(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMERGE_VXM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopExtCarry(v_ext.vMerge(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMERGE_VIM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopExtCarry(v_ext.vMerge(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMV_V_V) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMv(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMV_V_X) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMv(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMV_V_I) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vMv(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSADDU_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSaddu(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSADDU_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSaddu(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSADDU_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSaddu(), VExt::elem_sel_t::xxxuus, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSADD_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSadd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSADD_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSadd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSADD_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSadd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSUBU_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSsubu(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSUBU_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSsubu(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSUB_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSsub(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSUB_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSsub(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VAADDU_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAadd(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VAADDU_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAadd(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VAADD_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAadd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VAADD_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAadd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VASUBU_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAsub(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VASUBU_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAsub(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VASUB_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAsub(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VASUB_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vAsub(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSMUL_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSmul(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSMUL_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vSmul(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSRL_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShiftRight(false), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSRL_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShiftRight(false), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSRL_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShiftRight(false), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSRA_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShiftRight(false), VExt::elem_sel_t::xxxssu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSRA_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShiftRight(false), VExt::elem_sel_t::xxxssu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSSRA_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShiftRight(false), VExt::elem_sel_t::xxxssu, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VNCLIPU_WV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShiftRight(true), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VNCLIPU_WX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShiftRight(true), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VNCLIPU_WI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShiftRight(true), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VNCLIP_WV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShiftRight(true), VExt::elem_sel_t::xwxssu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VNCLIP_WX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShiftRight(true), VExt::elem_sel_t::xwxssu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VNCLIP_WI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoop(v_ext.vShiftRight(true), VExt::elem_sel_t::xwxssu, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VFADD_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfAdd(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFADD_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfAdd(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFSUB_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfSub(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFSUB_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfSub(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFRSUB_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfrSub(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWADD_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfwAdd(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWADD_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfwAdd(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWSUB_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfwSub(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWSUB_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfwSub(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWADD_WV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfwAddw(), VExt::elem_sel_t::wwxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWADD_WF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfwAddw(), VExt::elem_sel_t::wwxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWSUB_WV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfwSubw(), VExt::elem_sel_t::wwxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWSUB_WF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfwSubw(), VExt::elem_sel_t::wwxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFMUL_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfMul(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFMUL_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfMul(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFDIV_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfDiv(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFDIV_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfDiv(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFRDIV_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfrDiv(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWMUL_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfwMul(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWMUL_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfwMul(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFMACC_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfMacc(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFMACC_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfMacc(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFNMACC_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfNmacc(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFNMACC_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfNmacc(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFMSAC_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfMsac(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFMSAC_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfMsac(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFNMSAC_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfNmsac(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFNMSAC_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfNmsac(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFMADD_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfMadd(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFMADD_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfMadd(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFNMADD_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfNmadd(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFNMADD_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfNmadd(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFMSUB_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfMsub(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFMSUB_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfMsub(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFNMSUB_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfNmsub(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFNMSUB_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfNmsub(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWMACC_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfwMacc(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWMACC_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfwMacc(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWNMACC_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfwNmacc(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWNMACC_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfwNmacc(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWMSAC_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfwMsac(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWMSAC_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfwMsac(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWNMSAC_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfwNmsac(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWNMSAC_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfwNmsac(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFSQRT_V) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfSqrt(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFRSQRT7_V) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfRsqrt7(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFREC7_V) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfFrec7(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFMIN_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfMin(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFMIN_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfMin(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFMAX_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfMax(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFMAX_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfMax(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFSGNJ_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfSgnj(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFSGNJ_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfSgnj(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFSGNJN_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfSgnjn(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFSGNJN_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfSgnjn(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFSGNJX_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfSgnjx(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFSGNJX_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfSgnjx(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VMFEQ_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExtVoid(v_ext.vMfeq(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VMFEQ_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExtVoid(v_ext.vMfeq(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VMFNE_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExtVoid(v_ext.vMfneq(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VMFNE_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExtVoid(v_ext.vMfneq(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VMFLT_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExtVoid(v_ext.vMflt(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VMFLT_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExtVoid(v_ext.vMflt(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VMFLE_VV) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExtVoid(v_ext.vMfle(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VMFLE_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExtVoid(v_ext.vMfle(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VMFGT_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExtVoid(v_ext.vMfgt(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VMFGE_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExtVoid(v_ext.vMfge(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFCLASS_V) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfClass(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFMERGE_VFM) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopExtCarry(v_ext.vMerge(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFMV_V_F) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoop(v_ext.vfMv(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFCVT_XU_F_V) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfCvtXF(false), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFCVT_X_F_V) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfCvtXF(false), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFCVT_RTZ_XU_F_V) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfCvtXF(true), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFCVT_RTZ_X_F_V) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfCvtXF(true), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFCVT_F_XU_V) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoop(v_ext.vfCvtFX(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFCVT_F_X_V) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoop(v_ext.vfCvtFX(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWCVT_XU_F_V) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfCvtwXF(false), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWCVT_X_F_V) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfCvtwXF(false), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWCVT_RTZ_XU_F_V) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfCvtwXF(true), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWCVT_RTZ_X_F_V) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfCvtwXF(true), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWCVT_F_XU_V) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoop(v_ext.vfCvtFX(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWCVT_F_X_V) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoop(v_ext.vfCvtFX(), VExt::elem_sel_t::wxxsss, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWCVT_F_F_V) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfCvtwFF(), VExt::elem_sel_t::wxxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFNCVT_XU_F_W) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfCvtnXF(false), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFNCVT_X_F_W) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfCvtnXF(false), VExt::elem_sel_t::xwxsss, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFNCVT_RTZ_XU_F_W) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfCvtnXF(true), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFNCVT_RTZ_X_F_W) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfCvtnXF(true), VExt::elem_sel_t::xwxsss, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFNCVT_F_XU_W) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoop(v_ext.vfCvtFX(), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFNCVT_F_X_W) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoop(v_ext.vfCvtFX(), VExt::elem_sel_t::xwxsss, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFNCVT_F_F_W) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfCvtnFF(false), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFNCVT_ROD_F_F_W) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVdExt(v_ext.vfCvtnFF(true), VExt::elem_sel_t::xwxuuu, VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VREDSUM_VS) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopRed(v_ext.vRedSum(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VREDMAXU_VS) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopRed(v_ext.vRedMax(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VREDMAX_VS) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopRed(v_ext.vRedMax(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VREDMINU_VS) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopRed(v_ext.vRedMin(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VREDMIN_VS) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopRed(v_ext.vRedMin(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VREDAND_VS) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopRed(v_ext.vRedAnd(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VREDOR_VS) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopRed(v_ext.vRedOr(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VREDXOR_VS) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopRed(v_ext.vRedXor(), VExt::elem_sel_t::xxxsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWREDSUMU_VS) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopRed(v_ext.vRedSum(), VExt::elem_sel_t::wxwuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VWREDSUM_VS) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopRed(v_ext.vRedSum(), VExt::elem_sel_t::wxwsss, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VFREDUSUM_VS) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopRed(v_ext.vfRedSum(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFREDOSUM_VS) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopRed(v_ext.vfRedSum(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFREDMAX_VS) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopRed(v_ext.vfRedMax(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFREDMIN_VS) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopRed(v_ext.vfRedMin(), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWREDUSUM_VS) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopRed(v_ext.vfwRedSum(), VExt::elem_sel_t::wxwuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFWREDOSUM_VS) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopRed(v_ext.vfwRedSum(), VExt::elem_sel_t::wxwuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VMAND_MM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidAllMask(v_ext.vMask(VExt::maskOperation::m_and));
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMNAND_MM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidAllMask(v_ext.vMask(VExt::maskOperation::m_nand));
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMANDN_MM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidAllMask(v_ext.vMask(VExt::maskOperation::m_andn));
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMXOR_MM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidAllMask(v_ext.vMask(VExt::maskOperation::m_xor));
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMOR_MM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidAllMask(v_ext.vMask(VExt::maskOperation::m_or));
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMNOR_MM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidAllMask(v_ext.vMask(VExt::maskOperation::m_nor));
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMORN_MM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidAllMask(v_ext.vMask(VExt::maskOperation::m_orn));
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMXNOR_MM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidAllMask(v_ext.vMask(VExt::maskOperation::m_xnor));
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VCPOP_M) {
					v_ext.prepInstr(true, true, false);
					v_ext.vCpop();
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VFIRST_M) {
					v_ext.prepInstr(true, true, false);
					v_ext.vFirst();
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSBF_M) {
					v_ext.prepInstr(true, true, false);
					v_ext.vMs(VExt::vms_type_t::sbf);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSIF_M) {
					v_ext.prepInstr(true, true, false);
					v_ext.vMs(VExt::vms_type_t::sif);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMSOF_M) {
					v_ext.prepInstr(true, true, false);
					v_ext.vMs(VExt::vms_type_t::sof);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VIOTA_M) {
					v_ext.prepInstr(true, true, false);
					v_ext.vIota();
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VID_V) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoid(v_ext.vId(), VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMV_X_S) {
					v_ext.prepInstr(true, true, false);
					v_ext.vMvXs();
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMV_S_X) {
					v_ext.prepInstr(true, true, false);
					v_ext.vMvSx();
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VFMV_F_S) {
					v_ext.prepInstr(true, true, true);
					v_ext.vMvFs();
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VFMV_S_F) {
					v_ext.prepInstr(true, true, true);
					v_ext.vMvSf();
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VSLIDEUP_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidNoOverlap(v_ext.vSlideUp(regs[instr.rs1()]), VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSLIDEUP_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidNoOverlap(v_ext.vSlideUp(instr.rs1()), VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSLIDEDOWN_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoid(v_ext.vSlideDown(regs[instr.rs1()]), VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSLIDEDOWN_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoid(v_ext.vSlideDown(instr.rs1()), VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VSLIDE1UP_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoidNoOverlap(v_ext.vSlide1Up(VExt::param_sel_t::vx), VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VFSLIDE1UP_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVoidNoOverlap(v_ext.vSlide1Up(VExt::param_sel_t::vf), VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VSLIDE1DOWN_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopVoid(v_ext.vSlide1Down(VExt::param_sel_t::vx), VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VFSLIDE1DOWN_VF) {
					v_ext.prepInstr(true, true, true);
					v_ext.vLoopVoid(v_ext.vSlide1Down(VExt::param_sel_t::vf), VExt::param_sel_t::vf);
					v_ext.finishInstr(true);
				}
				OP_END();

				OP_CASE(VRGATHER_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopExt(v_ext.vGather(false), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VRGATHEREI16_VV) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopExt(v_ext.vGather(true), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vv);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VRGATHER_VX) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopExt(v_ext.vGather(false), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vx);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VRGATHER_VI) {
					v_ext.prepInstr(true, true, false);
					v_ext.vLoopExt(v_ext.vGather(false), VExt::elem_sel_t::xxxuuu, VExt::param_sel_t::vi);
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VCOMPRESS_VM) {
					v_ext.prepInstr(true, true, false);
					v_ext.vCompress();
					v_ext.finishInstr(false);
				}
				OP_END();

				OP_CASE(VMV_NR_R_V) {
					v_ext.prepInstr(true, true, false);
					v_ext.vMvNr();
					v_ext.finishInstr(false);
				}
				OP_END();
				// RV-V Extension End -- Placeholder 6
				// privileged instructions

				OP_CASE(WFI) {
					// NOTE: only a hint, can be implemented as NOP
					// std::cout << "[sim:wfi] CSR mstatus.mie " << csrs.mstatus->mie << std::endl;
					release_lr_sc_reservation();

					if (s_mode() && csrs.mstatus.reg.fields.tw)
						RAISE_ILLEGAL_INSTRUCTION();

					if (u_mode() && csrs.misa.has_supervisor_mode_extension())
						RAISE_ILLEGAL_INSTRUCTION();

					stats.inc_wfi();
					if (!ignore_wfi) {
						while (!has_local_pending_enabled_interrupts()) {
							sc_core::wait(wfi_event);
						}
					}
				}
				OP_END();

				OP_CASE(SFENCE_VMA) {
					if (s_mode() && csrs.mstatus.reg.fields.tvm)
						RAISE_ILLEGAL_INSTRUCTION();
					dbbcache.fence_vma(pc);
					lscache.fence_vma();
					stats.inc_fence_vma();
				}
				OP_END();

				OP_CASE(URET) {
					if (!csrs.misa.has_user_mode_extension())
						RAISE_ILLEGAL_INSTRUCTION();
					if (!csrs.misa.has_N_extension())
						RAISE_ILLEGAL_INSTRUCTION();
					if (!pc->cap.fields.access_system_regs)
						handle_cheri_cap_exception(CapEx_AccessSystemRegsViolation, cPccIdx, &rvfi_dii_output);
					return_from_trap_handler(UserMode);
					stats.inc_uret();
				}
				OP_END();

				OP_CASE(SRET) {
					if (u_mode())
						RAISE_ILLEGAL_INSTRUCTION();  // sret is always invalid in User mode
					if (!csrs.misa.has_supervisor_mode_extension() || (s_mode() && csrs.mstatus.reg.fields.tsr))
						RAISE_ILLEGAL_INSTRUCTION();
					if (!pc->cap.fields.access_system_regs)
						handle_cheri_cap_exception(CapEx_AccessSystemRegsViolation, cPccIdx, &rvfi_dii_output);
					return_from_trap_handler(SupervisorMode);
					stats.inc_sret();
				}
				OP_END();

				OP_CASE(MRET) {
					if (!m_mode())
						RAISE_ILLEGAL_INSTRUCTION();  // mret only valid in machine mode
					if (!pc->cap.fields.access_system_regs)
						handle_cheri_cap_exception(CapEx_AccessSystemRegsViolation, cPccIdx, &rvfi_dii_output);
					return_from_trap_handler(MachineMode);
					stats.inc_mret();
				}
				OP_END();
				OP_CASE(C_GET_PERM) {
					Capability cs1_val = regs[instr.rs1()];
					uint32_t perms = cs1_val.getCapPerms();
					regs[instr.rd()] = perms;
				}
				OP_END();
				OP_CASE(C_GET_TYPE) {
					Capability cs1_val = regs[instr.rs1()];
					if (cs1_val.hasReservedOType()) {
						// Sign extension of an otype field (18 bits wide)
						if (cs1_val.cap.fields.otype & (1 << (cCapOTypeWidth - 1))) {
							regs[instr.rd()] =
							    static_cast<int64_t>(cs1_val.cap.fields.otype | ~((1 << cCapOTypeWidth) - 1));
						} else {
							regs[instr.rd()] =
							    static_cast<int64_t>(cs1_val.cap.fields.otype & ((1 << cCapOTypeWidth) - 1));
						}
					} else {
						regs[instr.rd()] = static_cast<uint64_t>(cs1_val.cap.fields.otype);
					}
				}
				OP_END();
				OP_CASE(C_GET_BASE) {
					{
						Capability cs1_val = regs[instr.rs1()];
						regs[instr.rd()] = cs1_val.getBase();
					}
				}
				OP_END();
				OP_CASE(C_GET_LEN) {
					Capability cs1_val = regs[instr.rs1()];
					CapLen_t len = cs1_val.getLength();
					if (len > cCapMaxAddr) {
						len = cCapMaxAddr;
					}
					regs[instr.rd()] = len;
				}
				OP_END();
				OP_CASE(C_GET_TAG) {
					Capability cs1_val = regs[instr.rs1()];
					regs[instr.rd()] = cs1_val.cap.fields.tag;
				}
				OP_END();
				OP_CASE(C_GET_SEALED) {
					Capability cs1_val = regs[instr.rs1()];
					regs[instr.rd()] = cs1_val.isSealed();
				}
				OP_END();
				OP_CASE(C_GET_OFFSET) {
					Capability cs1_val = regs[instr.rs1()];
					regs[instr.rd()] = cs1_val.getOffset();
				}
				OP_END();
				OP_CASE(C_GET_FLAGS) {
					Capability cs1_val = regs[instr.rs1()];
					regs[instr.rd()] = cs1_val.getFlags();
				}
				OP_END();
				OP_CASE(C_GET_HIGH) {
					Capability cs1_val = regs[instr.rs1()];
					regs[instr.rd()] = cs1_val.toUint128() >> xlen;
				}
				OP_END();
				OP_CASE(C_GET_TOP) {
					Capability cs1_val = regs[instr.rs1()];
					CapLen_t top = cs1_val.getTop();
					if (top > cCapMaxAddr) {
						top = cCapMaxAddr;
					}
					regs[instr.rd()] = top;
				}
				OP_END();
				OP_CASE(C_SEAL) {
					Capability cs1_val = regs[instr.rs1()];
					Capability cs2_val = regs[instr.rs2()];
					uint64_t cs2_cursor = cs2_val.cap.fields.address;
					CapAddr_t cs2_base;
					CapLen_t cs2_top;
					cs2_val.getCapBounds(&cs2_base, &cs2_top);
					bool permitted = cs2_val.cap.fields.tag && !cs2_val.isSealed() && cs2_val.cap.fields.permit_seal &&
					                 (cs2_cursor >= cs2_base) && (cs2_top > cs2_cursor) && (cs2_cursor <= cCapMaxOType);
					cs1_val.clearTagIfSealed();
					cs1_val.seal(cs2_cursor);
					cs1_val.clearTagIf(!permitted);
					regs[instr.rd()] = cs1_val;
				}
				OP_END();
				OP_CASE(C_UNSEAL) {
					Capability cs1_val = regs[instr.rs1()];
					Capability cs2_val = regs[instr.rs2()];
					uint64_t cs2_cursor = cs2_val.cap.fields.address;
					CapAddr_t cs2_base;
					CapLen_t cs2_top;
					cs2_val.getCapBounds(&cs2_base, &cs2_top);
					bool permitted = cs2_val.cap.fields.tag && cs1_val.isSealed() && !cs2_val.isSealed() &&
					                 !cs1_val.hasReservedOType() &&
					                 (cs2_cursor == static_cast<uint64_t>(cs1_val.cap.fields.otype)) &&
					                 cs2_val.cap.fields.permit_unseal && (cs2_cursor >= cs2_base) &&
					                 (cs2_top >= cs2_cursor);
					bool new_global = cs1_val.cap.fields.global & cs2_val.cap.fields.global;
					cs1_val.unseal();
					cs1_val.cap.fields.global = new_global;
					cs1_val.clearTagIf(!permitted);
					regs[instr.rd()] = cs1_val;
				}
				OP_END();
				OP_CASE(C_AND_PERM) {
					Capability cs1_val = regs[instr.rs1()];
					uint64_t rs2_val = regs[instr.rs2()];
					uint32_t perms = cs1_val.getCapPerms();
					uint32_t mask = rs2_val & ((1 << cCapPermsWidth) - 1);
					cs1_val.clearTagIfSealed();
					cs1_val.setCapPerms(perms & mask);
					regs[instr.rd()] = cs1_val;
				}
				OP_END();
				OP_CASE(C_SET_FLAGS) {
					Capability cs1_val = regs[instr.rs1()];
					uint64_t rs2_val = regs[instr.rs2()];
					cs1_val.clearTagIfSealed();
					cs1_val.setFlags(rs2_val & ((1 << cCapFlagsWidth) - 1));
					regs[instr.rd()] = cs1_val;
				}
				OP_END();
				OP_CASE(C_SET_OFFSET) {
					Capability cs1_val = regs[instr.rs1()];
					uint64_t rs2_val = regs[instr.rs2()];
					cs1_val.clearTagIfSealed();
					bool success = cs1_val.setCapOffset(rs2_val);
					cs1_val.clearTagIf(!success);
					regs[instr.rd()] = cs1_val;
				}
				OP_END();
				OP_CASE(C_SET_ADDR) {
					Capability cs1_val = regs[instr.rs1()];
					uint64_t rs2_val = regs[instr.rs2()];
					cs1_val.clearTagIfSealed();
					bool representable = cs1_val.setCapAddr(rs2_val);
					cs1_val.clearTagIf(!representable);
					regs[instr.rd()] = cs1_val;
					reset_reg_zero();
				}
				OP_END();
				// TODO: C_SUB was removed in latest version of the riscv-cheri spec
				// But it is still used by TestRIG and is implemented in sail-riscv-cheri
				// Therefore it was required to be implemented here
				OP_CASE(C_SUB) {
					regs[instr.rd()] = regs[instr.rs1()].cap.fields.address - regs[instr.rs2()].cap.fields.address;
				}
				OP_END();
				OP_CASE(C_INC_OFFSET) {
					Capability cs1_val = regs[instr.rs1()];
					cs1_val.clearTagIfSealed();
					bool success = cs1_val.incCapOffset(regs[instr.rs2()]);
					cs1_val.clearTagIf(!success);
					regs[instr.rd()] = cs1_val;
				}
				OP_END();
				OP_CASE(C_INC_OFFSET_IMM) {
					Capability cs1_val = regs[instr.rs1()];
					cs1_val.clearTagIfSealed();
					bool success = cs1_val.incCapOffset(instr.I_imm());
					cs1_val.clearTagIf(!success);
					regs[instr.rd()] = cs1_val;
				}
				OP_END();
				OP_CASE(C_SET_BOUNDS) {
					Capability cs1_val = regs[instr.rs1()];
					uint64_t rs2_val = regs[instr.rs2()];
					uint64_t newBase = cs1_val.cap.fields.address;
					CapLen_t newTop = (CapLen_t)newBase + (CapLen_t)(rs2_val);
					bool inBounds = cs1_val.inCapBounds(newBase, rs2_val);
					cs1_val.clearTagIfSealed();
					cs1_val.setCapBounds(newBase, newTop);
					cs1_val.clearTagIf(!inBounds);
					regs[instr.rd()] = cs1_val;
				}
				OP_END();
				OP_CASE(C_SET_BOUNDS_EXACT) {
					Capability cs1_val = regs[instr.rs1()];
					uint64_t rs2_val = regs[instr.rs2()];
					uint64_t newBase = cs1_val;
					CapLen_t newTop = (CapLen_t)newBase + (CapLen_t)rs2_val;
					bool inBounds = cs1_val.inCapBounds(newBase, rs2_val);
					cs1_val.clearTagIfSealed();
					bool exact = cs1_val.setCapBounds(newBase, newTop);
					cs1_val.clearTagIf(!(inBounds && exact));
					regs[instr.rd()] = cs1_val;
				}
				OP_END();
				OP_CASE(C_SET_BOUNDS_IMM) {
					Capability cs1_val = regs[instr.rs1()];
					uint64_t newBase = cs1_val;
					uint64_t immVal = BIT_SLICE(instr.data(), 31, 20);  // instr.I_imm() but interpreted as unsigned
					CapLen_t newTop = (CapLen_t)newBase + (CapLen_t)immVal;
					bool inBounds = cs1_val.inCapBounds(newBase, immVal);
					cs1_val.clearTagIfSealed();
					cs1_val.setCapBounds(newBase, newTop);
					cs1_val.clearTagIf(!inBounds);
					regs[instr.rd()] = cs1_val;
				}
				OP_END();
				OP_CASE(C_SET_HIGH) {
					Capability cs1_val = regs[instr.rs1()];
					CapAddr_t intVal = regs[instr.rs2()].cap.fields.address;
					__uint128_t cs1_int = cs1_val.toUint128();
					uint64_t cs1_low = static_cast<uint64_t>(cs1_int);
					__uint128_t newCapInt = ((__uint128_t)intVal << xlen) | cs1_low;
					Capability newCap = Capability(newCapInt, false);
					regs[instr.rd()] = newCap;
				}
				OP_END();
				OP_CASE(C_CLEAR_TAG) {
					Capability cs1_val = regs[instr.rs1()];
					cs1_val.clearTag();
					regs[instr.rd()] = cs1_val;
				}
				OP_END();
				OP_CASE(C_BUILD_CAP) {
					Capability cs1_val;
					if (instr.rs1() == 0) {
						cs1_val = ddc;
					} else {
						cs1_val = regs[instr.rs1()];
					}
					Capability cs2_val = regs[instr.rs2()];
					Capability authorityCap = cs1_val;
					Capability requestedCap = cs2_val;
					requestedCap.cap.fields.tag = true;  // TODO: Should it be possible to set the tag like this?
					bool requestedSentry = requestedCap.cap.fields.otype == cOtypeSentryUnsigned;
					if (!requestedSentry)
						requestedCap.unseal();

					CapAddr_t authorityBase;
					CapLen_t authorityTop;
					authorityCap.getCapBounds(&authorityBase, &authorityTop);
					CapAddr_t requestedBase;
					CapLen_t requestedTop;
					requestedCap.getCapBounds(&requestedBase, &requestedTop);

					uint32_t authorityPerms = authorityCap.getCapPerms();
					uint32_t requestedPerms = requestedCap.getCapPerms();
					uint8_t requestedFlags = requestedCap.getFlags();

					// TODO Cleanup the mess below
					bool subset = (requestedBase >= authorityBase) && (requestedTop <= authorityTop) &&
					              (requestedTop >= requestedBase) &&
					              ((requestedPerms && authorityPerms) == requestedPerms);
					authorityCap.clearTagIfSealed();
					bool exact = authorityCap.setCapBounds(requestedBase, requestedTop);
					authorityCap.setCapOffset(requestedCap.getOffset());
					authorityCap.setCapPerms(requestedPerms);
					authorityCap.setFlags(requestedFlags);
					if (requestedSentry)
						authorityCap.seal(cOtypeSentryUnsigned);
					bool derivable = authorityCap == requestedCap;
					assert(!derivable ||
					       exact);  // If requestedCap was a derivable encoding then setBounds should be exact
					Capability cd6;
					if (subset && derivable) {
						cd6 = authorityCap;
					} else {
						cd6 = requestedCap;
						cd6.clearTag();
					}
					regs[instr.rd()] = cd6;
				}
				OP_END();
				OP_CASE(C_COPY_TYPE) {
					Capability cs1_val = regs[instr.rs1()];
					Capability cs2_val = regs[instr.rs2()];
					bool reserved = cs2_val.hasReservedOType();
					uint64_t otype;
					if (cs2_val.hasReservedOType()) {
						// Sign extension of an otype field (18 bits wide)
						if (cs2_val.cap.fields.otype & (1 << (cCapOTypeWidth - 1))) {
							otype = static_cast<int64_t>(cs2_val.cap.fields.otype | ~((1 << cCapOTypeWidth) - 1));
						} else {
							otype = static_cast<int64_t>(cs2_val.cap.fields.otype & ((1 << cCapOTypeWidth) - 1));
						}
					} else {
						otype = static_cast<uint64_t>(cs2_val.cap.fields.otype);
					}
					cs1_val.clearTagIfSealed();
					bool representable = cs1_val.setCapAddr(otype);
					cs1_val.clearTagIf(reserved || !representable);
					regs[instr.rd()] = cs1_val;
				}
				OP_END();
				OP_CASE(C_C_SEAL) {
					Capability cs1_val = regs[instr.rs1()];
					Capability cs2_val = regs[instr.rs2()];
					uint64_t cs2_cursor = cs2_val.cap.fields.address;
					CapAddr_t cs2_base;
					CapLen_t cs2_top;
					cs2_val.getCapBounds(&cs2_base, &cs2_top);
					bool passthrough =
					    !cs2_val.cap.fields.tag || cs1_val.isSealed() || (cs2_cursor < cs2_base) ||
					    (cs2_top <= cs2_cursor) ||
					    (static_cast<int64_t>(cs2_val.cap.fields.address) == static_cast<int64_t>(cOtypeUnsealed));
					if (passthrough) {
						regs[instr.rd()] = cs1_val;
					} else {
						bool permitted =
						    !cs2_val.isSealed() && cs2_val.cap.fields.permit_seal && (cs2_cursor <= cCapMaxOType);
						cs1_val.seal(cs2_cursor);
						cs1_val.clearTagIf(!permitted);
						regs[instr.rd()] = cs1_val;
					}
				}
				OP_END();
				OP_CASE(C_SEAL_ENTRY) {
					Capability cs1_val = regs[instr.rs1()];
					cs1_val.clearTagIfSealed();
					cs1_val.seal(cOtypeSentryUnsigned);
					regs[instr.rd()] = cs1_val;
				}
				OP_END();
				OP_CASE(C_TO_PTR) {
					Capability cs1_val = regs[instr.rs1()];
					Capability cs2_val = (instr.rs2() == 0) ? ddc : regs[instr.rs2()];

					if (!cs1_val.cap.fields.tag) {
						regs[instr.rd()] = 0;
					} else {
						regs[instr.rd()] = cs1_val.cap.fields.address - cs2_val.getBase();
					}
				}
				OP_END();
				OP_CASE(C_FROM_PTR) {
					Capability cs1_val = (instr.rs1() == 0) ? ddc : regs[instr.rs1()];
					uint64_t rs2_val = regs[instr.rs2()];
					if (rs2_val == 0) {
						regs[instr.rd()] = cNullCap;
					} else {
						cs1_val.clearTagIfSealed();
						bool success = cs1_val.setCapOffset(rs2_val);
						cs1_val.clearTagIf(!success);
						regs[instr.rd()] = cs1_val;
					}
				}
				OP_END();
				OP_CASE(C_MOVE) {
					// Capability register cd is replaced with the contents of cs1
					regs[instr.rd()] = regs[instr.rs1()];
				}
				OP_END();
				OP_CASE(C_TEST_SUBSET) {
					Capability cs1_val = (instr.rs1() == 0) ? ddc : regs[instr.rs1()];
					Capability cs2_val = regs[instr.rs2()];

					CapAddr_t cs2_base;
					CapLen_t cs2_top;
					cs2_val.getCapBounds(&cs2_base, &cs2_top);

					CapAddr_t cs1_base;
					CapLen_t cs1_top;
					cs1_val.getCapBounds(&cs1_base, &cs1_top);
					uint32_t cs2_perms = cs2_val.getCapPerms();
					uint32_t cs1_perms = cs1_val.getCapPerms();

					bool result = true;
					if (cs1_val.cap.fields.tag != cs2_val.cap.fields.tag)
						result = false;
					else if (cs2_base < cs1_base)
						result = false;
					else if (cs2_top > cs1_top)
						result = false;
					else if ((cs2_perms & cs1_perms) != cs2_perms)
						result = false;

					// bool result = (cs1_val.cap.fields.tag == cs2_val.cap.fields.tag) && (cs2_base >= cs1_base) &&
					// (cs2_top <= cs1_top) && ((cs2_perms & cs1_perms) == cs2_perms);
					regs[instr.rd()] = result;
				}
				OP_END();
				OP_CASE(C_SET_EQUAL_EXACT) {
					Capability cs1_val = regs[instr.rs1()];
					Capability cs2_val = regs[instr.rs2()];
					regs[instr.rd()] = cs1_val == cs2_val;
				}
				OP_END();
				OP_CASE(JALR_CAP) {
					execute_c_jalr(0);
				}
				OP_END();
				OP_CASE(JALR_PCC) {
					assert(0);  // TODO Implement this
				}
				OP_END();
				OP_CASE(C_INVOKE) {
					Capability cs1_val = regs[instr.rs1()];
					Capability cs2_val = regs[instr.rs2()];
					uint64_t newPC = cs1_val.cap.fields.address & ~1;
					CapAddr_t newPCCBase = cs1_val.getBase();

					if (!cs1_val.cap.fields.tag) {
						handle_cheri_reg_exception(CapEx_TagViolation, instr.rs1(), &rvfi_dii_output);
					} else if (!cs2_val.cap.fields.tag) {
						handle_cheri_reg_exception(CapEx_TagViolation, instr.rs2(), &rvfi_dii_output);
					} else if (cs1_val.hasReservedOType()) {
						handle_cheri_reg_exception(CapEx_SealViolation, instr.rs1(), &rvfi_dii_output);
					} else if (cs2_val.hasReservedOType()) {
						handle_cheri_reg_exception(CapEx_SealViolation, instr.rs2(), &rvfi_dii_output);
					} else if (cs1_val.cap.fields.otype != cs2_val.cap.fields.otype) {
						handle_cheri_reg_exception(CapEx_TypeViolation, instr.rs1(), &rvfi_dii_output);
					} else if (!cs1_val.cap.fields.permit_cinvoke) {
						handle_cheri_reg_exception(CapEx_PermitCInvokeViolation, instr.rs1(), &rvfi_dii_output);
					} else if (!cs2_val.cap.fields.permit_cinvoke) {
						handle_cheri_reg_exception(CapEx_PermitCInvokeViolation, instr.rs2(), &rvfi_dii_output);
					} else if (!cs1_val.cap.fields.permit_execute) {
						handle_cheri_reg_exception(CapEx_PermitExecuteViolation, instr.rs1(), &rvfi_dii_output);
					} else if (cs2_val.cap.fields.permit_execute) {
						handle_cheri_reg_exception(CapEx_PermitExecuteViolation, instr.rs2(), &rvfi_dii_output);
					} else if ((have_pcc_relocation() && (newPCCBase & 1)) ||
					           (newPCCBase & 2 && !csrs.misa.has_C_extension())) {
						handle_cheri_reg_exception(CapEx_UnalignedBase, instr.rs1(), &rvfi_dii_output);
					} else if (newPC & 1 && !csrs.misa.has_C_extension()) {
						handle_mem_exception(newPC, E_FetchAddrAlign, &rvfi_dii_output);
					} else if (!cs1_val.inCapBounds(newPC, min_instruction_bytes())) {
						handle_cheri_reg_exception(CapEx_LengthViolation, instr.rs1(), &rvfi_dii_output);
					} else {
						regs[31] = cs2_val;
						regs[31].unseal();
						pc = cs1_val;
						pc->unseal();
						pc = newPC;  // Only sets address
						dbbcache.set_pc(pc);
						if (unlikely(rvfi_dii)) {
							rvfi_dii_output.rvfi_dii_rd_addr = 31;
							rvfi_dii_output.rvfi_dii_rd_wdata = regs[31];
							rvfi_dii_output.rvfi_dii_pc_wdata = pc;
						}
					}
				}
				OP_END();
				// TODO: This was removed in latest riscv-cheri spec
				// But it is still used by TestRIG and is implemented in sail-riscv-cheri
				// Therefore it was required to be implemented here
				OP_CASE(C_GET_ADDR) {
					Capability cs1_val = regs[instr.rs1()];
					regs[instr.rd()] = cs1_val.cap.fields.address;
				}
				OP_END();
				OP_CASE(C_SPECIAL_R_W) {
					bool specialExists = false;
					bool ro = true;
					PrivilegeLevel priv = MachineMode;
					bool needASR = true;
					switch (instr.rs2()) {
						case 0:
							specialExists = true;
							ro = true;
							priv = UserMode;
							needASR = false;
							break;
						case 1:
							specialExists = true;
							ro = false;
							priv = UserMode;
							needASR = false;
							break;
						case 4:
							if (csrs.misa.has_N_extension()) {
								specialExists = true;
								ro = false;
								priv = UserMode;
								needASR = true;
							}
							break;
						case 5:
							if (csrs.misa.has_N_extension()) {
								specialExists = true;
								ro = false;
								priv = UserMode;
								needASR = true;
							}
							break;
						case 6:
							if (csrs.misa.has_N_extension()) {
								specialExists = true;
								ro = false;
								priv = UserMode;
								needASR = true;
							}
							break;
						case 7:
							if (csrs.misa.has_N_extension()) {
								specialExists = true;
								ro = false;
								priv = UserMode;
								needASR = true;
							}
							break;
						case 12:
							if (csrs.misa.has_supervisor_mode_extension()) {
								specialExists = true;
								ro = false;
								priv = SupervisorMode;
								needASR = true;
							}
							break;
						case 13:
							if (csrs.misa.has_supervisor_mode_extension()) {
								specialExists = true;
								ro = false;
								priv = SupervisorMode;
								needASR = true;
							}
							break;
						case 14:
							if (csrs.misa.has_supervisor_mode_extension()) {
								specialExists = true;
								ro = false;
								priv = SupervisorMode;
								needASR = true;
							}
							break;
						case 15:
							if (csrs.misa.has_supervisor_mode_extension()) {
								specialExists = true;
								ro = false;
								priv = SupervisorMode;
								needASR = true;
							}
							break;
						case 28:
							specialExists = true;
							ro = false;
							priv = MachineMode;
							needASR = true;
							break;
						case 29:
							specialExists = true;
							ro = false;
							priv = MachineMode;
							needASR = true;
							break;
						case 30:
							specialExists = true;
							ro = false;
							priv = MachineMode;
							needASR = true;
							break;
						case 31:
							specialExists = true;
							ro = false;
							priv = MachineMode;
							needASR = true;
							break;
						default:
							break;
					}
					if (!specialExists || (ro && instr.rs1() != 0) || (prv < priv)) {
						RAISE_ILLEGAL_INSTRUCTION();
					}
					if (needASR && !pc->cap.fields.access_system_regs) {
						uint64_t regnum = instr.rs2() + (1 << 5);
						handle_cheri_cap_exception(CapEx_AccessSystemRegsViolation, regnum, &rvfi_dii_output);
					}
					Capability cs1_val = regs[instr.rs1()];
					Capability cd;
					switch (instr.rs2()) {
						case 0: {
							Capability result = pc;
							bool success = result.setCapAddr(dbbcache.get_last_pc_before_callback());
							assert(success);  // PCC with offset PC should always be representable
							cd = result;
						} break;
						case 1:
							cd = ddc;
							break;
						case 4:
							cd = csrs.utcc;
							break;
						case 5:
							cd = csrs.utdc;
							break;
						case 6:
							cd = csrs.uscratchc;
							break;
						case 7:
							cd = legalizeEpcc(csrs.uepcc);
							break;
						case 12:
							cd = csrs.stcc;
							break;
						case 13:
							cd = csrs.stdc;
							break;
						case 14:
							cd = csrs.sscratchc;
							break;
						case 15:
							cd = legalizeEpcc(csrs.sepcc);
							break;
						case 28:
							cd = csrs.mtcc;
							break;
						case 29:
							cd = csrs.mtdc;
							break;
						case 30:
							cd = csrs.mscratchc;
							break;
						case 31:
							cd = legalizeEpcc(csrs.mepcc);
							break;
						default:
							assert(false);  // unreachable, undefined behavior...
							break;
					}
					if (instr.rs1() != 0) {
						switch (instr.rs2()) {
							case 1:
								ddc = cs1_val;
								break;
							case 4:
								csrs.utcc = legalizeTcc(csrs.utcc, cs1_val);
								break;
							case 5:
								csrs.utdc = cs1_val;
								break;
							case 6:
								csrs.uscratchc = cs1_val;
								break;
							case 7:
								csrs.uepcc = cs1_val;
								break;
							case 12:
								csrs.stcc = legalizeTcc(csrs.stcc, cs1_val);
								break;
							case 13:
								csrs.stdc = cs1_val;
								break;
							case 14:
								csrs.sscratchc = cs1_val;
								break;
							case 15:
								csrs.sepcc = cs1_val;
								break;
							case 28:
								csrs.mtcc = legalizeTcc(csrs.mtcc, cs1_val);
								break;
							case 29:
								csrs.mtdc = cs1_val;
								break;
							case 30:
								csrs.mscratchc = cs1_val;
								break;
							case 31:
								csrs.mepcc = cs1_val;
								break;
							default:
								RAISE_ILLEGAL_INSTRUCTION();
								break;
						}
					}
					regs[instr.rd()] = cd;
					reset_reg_zero();
				}
				OP_END();

				OP_CASE(C_CLEAR) {
					uint8_t q = (instr.rs1() & 0b11000) >> 3;                 // TODO: Check mask, make constant
					uint8_t m = ((instr.rs1() & 0b00111) << 5) | instr.rd();  // TODO: Check mask, make constant

					for (int i = 0; i < 8; i++) {
						if (m & (1 << i)) {
							if (q == 0 && i == 0) {
								ddc = cNullCap;
							} else {
								regs[8 * q + i] = cNullCap;
							}
							if (unlikely(rvfi_dii)) {
								rvfi_dii_output.rvfi_dii_rd_wdata = 0;
								rvfi_dii_output.rvfi_dii_rd_addr = 8 * q + i;
							}
						}
					}
				}
				OP_END();
				OP_CASE(FP_CLEAR) {
					uint8_t q = (instr.rs1() & 0b11000) >> 3;                 // TODO: Check mask, make constant
					uint8_t m = ((instr.rs1() & 0b00111) << 5) | instr.rd();  // TODO: Check mask, make constant

					if (/*haveFExt()*/ 0)  // TODO: Check if floating point extension is enabled
					{
						for (int i = 0; i < 8; i++) {
							if (m & (1 << i)) {
								// F(8 * unsigned(q) + i) = zeros();
								regs[8 * q + i] = 0;  // TODO Find where floating point registers are
								if (unlikely(rvfi_dii)) {
									rvfi_dii_output.rvfi_dii_rd_wdata = 0;
									rvfi_dii_output.rvfi_dii_rd_addr = 8 * q + i;
								}
							}
						}
					} else  // TODO Inverse check for faster execution
					{
						RAISE_ILLEGAL_INSTRUCTION();
					}
				}
				OP_END();
				OP_CASE(C_ROUND_REPRESENTABLE_LENGTH) {
					uint64_t len = regs[instr.rs1()];
					regs[instr.rd()] = getRepresentableLength(len);
				}
				OP_END();
				OP_CASE(C_REPRESENTABLE_ALIGNMENT_MASK)

				{
					uint64_t len = regs[instr.rs1()];
					regs[instr.rd()] = getRepresentableAlignmentMask(len);
				}
				OP_END();
				OP_CASE(C_LOAD_TAGS) {
					Capability cs1_val = regs[instr.rs1()];
					uint64_t vaddr = cs1_val.cap.fields.address;
					// bool aq = false;
					// bool rl = false;

					if (!cs1_val.cap.fields.tag) {
						handle_cheri_reg_exception(CapEx_TagViolation, instr.rs1(), &rvfi_dii_output);
					} else if (cs1_val.isSealed()) {
						handle_cheri_reg_exception(CapEx_SealViolation, instr.rs1(), &rvfi_dii_output);
					} else if (!cs1_val.cap.fields.permit_load) {
						handle_cheri_reg_exception(CapEx_PermitLoadViolation, instr.rs1(), &rvfi_dii_output);
					} else if (!cs1_val.cap.fields.permit_load_cap) {
						handle_cheri_reg_exception(CapEx_PermitLoadCapViolation, instr.rs1(), &rvfi_dii_output);
					} else if (!cs1_val.inCapBounds(vaddr, cCapsPerCacheLine * cCapSize)) {
						handle_cheri_reg_exception(CapEx_LengthViolation, instr.rs1(), &rvfi_dii_output);
					} else if (vaddr % (cCapsPerCacheLine * cCapSize) != 0) {
						handle_mem_exception(vaddr, E_LoadAddrAlign, &rvfi_dii_output);
						assert(false);
					}
					regs[instr.rd()] = mem->load_tags(vaddr);
				}

				OP_END();
				OP_CASE(C_CLEAR_TAGS)
				// TODO This function does not have a specification in the CHERI specification
				// Neither has it a SAIL implementation
				// It is not clear what this function should do

				assert(0);
				OP_END();

				OP_CASE(LB_DDC) {
					uint64_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					regs[instr.rd()] = mem->handle_load_data_via_cap(cDdcIdx, ddc, vaddr, false, 1);
				}
				OP_END();
				OP_CASE(LH_DDC) {
					uint64_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					regs[instr.rd()] = mem->handle_load_data_via_cap(cDdcIdx, ddc, vaddr, false, 2);
				}
				OP_END();
				OP_CASE(LW_DDC) {
					uint64_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					regs[instr.rd()] = mem->handle_load_data_via_cap(cDdcIdx, ddc, vaddr, false, 4);
				}
				OP_END();
				OP_CASE(LD_DDC) {
					uint64_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					regs[instr.rd()] = mem->handle_load_data_via_cap(cDdcIdx, ddc, vaddr, false, 8);
				}
				OP_END();
				OP_CASE(LC_DDC) {
					uint64_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					regs[instr.rd()] = mem->handle_load_cap_via_cap(cDdcIdx, ddc, vaddr);
				}
				OP_END();
				OP_CASE(LBU_DDC) {
					uint64_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					regs[instr.rd()] = mem->handle_load_data_via_cap(cDdcIdx, ddc, vaddr, true, 1);
				}
				OP_END();
				OP_CASE(LHU_DDC) {
					uint64_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					regs[instr.rd()] = mem->handle_load_data_via_cap(cDdcIdx, ddc, vaddr, true, 2);
				}
				OP_END();
				OP_CASE(LWU_DDC) {
					uint64_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					regs[instr.rd()] = mem->handle_load_data_via_cap(cDdcIdx, ddc, vaddr, true, 4);
				}
				OP_END();
				OP_CASE(LB_CAP)
				regs[instr.rd()] = mem->handle_load_data_via_cap(instr.rs1(), regs[instr.rs1()],
				                                                 regs[instr.rs1()].cap.fields.address, false, 1);
				OP_END();
				OP_CASE(LH_CAP)
				regs[instr.rd()] = mem->handle_load_data_via_cap(instr.rs1(), regs[instr.rs1()],
				                                                 regs[instr.rs1()].cap.fields.address, false, 2);
				OP_END();
				OP_CASE(LW_CAP)
				regs[instr.rd()] = mem->handle_load_data_via_cap(instr.rs1(), regs[instr.rs1()],
				                                                 regs[instr.rs1()].cap.fields.address, false, 4);
				OP_END();
				OP_CASE(LD_CAP)
				regs[instr.rd()] = mem->handle_load_data_via_cap(instr.rs1(), regs[instr.rs1()],
				                                                 regs[instr.rs1()].cap.fields.address, false, 8);
				OP_END();
				OP_CASE(LC_CAP)
				regs[instr.rd()] =
				    mem->handle_load_cap_via_cap(instr.rs1(), regs[instr.rs1()], regs[instr.rs1()].cap.fields.address);
				OP_END();
				OP_CASE(LBU_CAP)
				regs[instr.rd()] = mem->handle_load_data_via_cap(instr.rs1(), regs[instr.rs1()],
				                                                 regs[instr.rs1()].cap.fields.address, true, 1);
				OP_END();
				OP_CASE(LHU_CAP)
				regs[instr.rd()] = mem->handle_load_data_via_cap(instr.rs1(), regs[instr.rs1()],
				                                                 regs[instr.rs1()].cap.fields.address, true, 2);
				OP_END();
				OP_CASE(LWU_CAP)
				regs[instr.rd()] = mem->handle_load_data_via_cap(instr.rs1(), regs[instr.rs1()],
				                                                 regs[instr.rs1()].cap.fields.address, true, 4);
				OP_END();
				OP_CASE(LR_B_DDC) {
					uxlen_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					regs[instr.rd()] = mem->atomic_load_reserved_data_via_cap(cDdcIdx, ddc, vaddr, 1);
					if (lr_sc_counter == 0) {
						lr_sc_counter = 17;  // this instruction + 16 additional ones, (an over-approximation) to
						// cover the RISC-V forward progress property
						force_slow_path();
					}
					reset_reg_zero();
				}
				OP_END();
				OP_CASE(LR_H_DDC) {
					uxlen_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					regs[instr.rd()] = mem->atomic_load_reserved_data_via_cap(cDdcIdx, ddc, vaddr, 2);
					if (lr_sc_counter == 0) {
						lr_sc_counter = 17;  // this instruction + 16 additional ones, (an over-approximation) to
						// cover the RISC-V forward progress property
						force_slow_path();
					}
					reset_reg_zero();
				}
				OP_END();
				OP_CASE(LR_W_DDC) {
					uxlen_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					regs[instr.rd()] = mem->atomic_load_reserved_data_via_cap(cDdcIdx, ddc, vaddr, 4);
					if (lr_sc_counter == 0) {
						lr_sc_counter = 17;  // this instruction + 16 additional ones, (an over-approximation) to
						// cover the RISC-V forward progress property
						force_slow_path();
					}
					reset_reg_zero();
				}
				OP_END();
				OP_CASE(LR_D_DDC) {
					uxlen_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					regs[instr.rd()] = mem->atomic_load_reserved_data_via_cap(cDdcIdx, ddc, vaddr, 8);
					if (lr_sc_counter == 0) {
						lr_sc_counter = 17;  // this instruction + 16 additional ones, (an over-approximation) to
						// cover the RISC-V forward progress property
						force_slow_path();
					}
					reset_reg_zero();
				}
				OP_END();
				OP_CASE(LR_C_DDC) {
					uxlen_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					regs[instr.rd()] = mem->atomic_load_reserved_cap_via_cap(cDdcIdx, ddc, vaddr);
					if (lr_sc_counter == 0) {
						lr_sc_counter = 17;  // this instruction + 16 additional ones, (an over-approximation) to
						// cover the RISC-V forward progress property
						force_slow_path();
					}
					reset_reg_zero();
				}
				OP_END();
				OP_CASE(LR_B_CAP) {
					regs[instr.rd()] = mem->atomic_load_reserved_data_via_cap(instr.rs1(), regs[instr.rs1()],
					                                                          regs[instr.rs1()].cap.fields.address, 1);
					if (lr_sc_counter == 0) {
						lr_sc_counter = 17;  // this instruction + 16 additional ones, (an over-approximation) to
						// cover the RISC-V forward progress property
						force_slow_path();
					}
					reset_reg_zero();
				}
				OP_END();
				OP_CASE(LR_H_CAP) {
					regs[instr.rd()] = mem->atomic_load_reserved_data_via_cap(instr.rs1(), regs[instr.rs1()],
					                                                          regs[instr.rs1()].cap.fields.address, 1);
					if (lr_sc_counter == 0) {
						lr_sc_counter = 17;  // this instruction + 16 additional ones, (an over-approximation) to
						// cover the RISC-V forward progress property
						force_slow_path();
					}
					reset_reg_zero();
				}
				OP_END();
				OP_CASE(LR_W_CAP) {
					regs[instr.rd()] = mem->atomic_load_reserved_data_via_cap(instr.rs1(), regs[instr.rs1()],
					                                                          regs[instr.rs1()].cap.fields.address, 1);
					if (lr_sc_counter == 0) {
						lr_sc_counter = 17;  // this instruction + 16 additional ones, (an over-approximation) to
						// cover the RISC-V forward progress property
						force_slow_path();
					}
					reset_reg_zero();
				}
				OP_END();
				OP_CASE(LR_D_CAP) {
					regs[instr.rd()] = mem->atomic_load_reserved_data_via_cap(instr.rs1(), regs[instr.rs1()],
					                                                          regs[instr.rs1()].cap.fields.address, 1);
					if (lr_sc_counter == 0) {
						lr_sc_counter = 17;  // this instruction + 16 additional ones, (an over-approximation) to
						// cover the RISC-V forward progress property
						force_slow_path();
					}
					reset_reg_zero();
				}
				OP_END();
				OP_CASE(LR_C_CAP) {
					regs[instr.rd()] = mem->atomic_load_reserved_cap_via_cap(instr.rs1(), regs[instr.rs1()],
					                                                         regs[instr.rs1()].cap.fields.address);
					if (lr_sc_counter == 0) {
						lr_sc_counter = 17;  // this instruction + 16 additional ones, (an over-approximation) to
						// cover the RISC-V forward progress property
						force_slow_path();
					}
					reset_reg_zero();
				}
				OP_END();
				OP_CASE(SB_DDC) {
					uint64_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					mem->handle_store_data_via_cap(regs[instr.rs2()], cDdcIdx, ddc, vaddr, 1);
				}
				OP_END();
				OP_CASE(SH_DDC) {
					uint64_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					mem->handle_store_data_via_cap(regs[instr.rs2()], cDdcIdx, ddc, vaddr, 2);
				}
				OP_END();
				OP_CASE(SW_DDC) {
					uint64_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					mem->handle_store_data_via_cap(regs[instr.rs2()], cDdcIdx, ddc, vaddr, 4);
				}
				OP_END();
				OP_CASE(SD_DDC) {
					uint64_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					mem->handle_store_data_via_cap(regs[instr.rs2()], cDdcIdx, ddc, vaddr, 8);
				}
				OP_END();
				OP_CASE(SC_DDC) {
					uint64_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					mem->handle_store_data_via_cap(regs[instr.rs2()], cDdcIdx, ddc, vaddr, cCapSize);
				}
				OP_END();
				OP_CASE(SB_CAP)
				mem->handle_store_data_via_cap(regs[instr.rs2()], instr.rs1(), regs[instr.rs1()],
				                               regs[instr.rs1()].cap.fields.address, 1);
				OP_END();
				OP_CASE(SH_CAP)
				mem->handle_store_data_via_cap(regs[instr.rs2()], instr.rs1(), regs[instr.rs1()],
				                               regs[instr.rs1()].cap.fields.address, 2);
				OP_END();
				OP_CASE(SW_CAP)
				mem->handle_store_data_via_cap(regs[instr.rs2()], instr.rs1(), regs[instr.rs1()],
				                               regs[instr.rs1()].cap.fields.address, 4);
				OP_END();
				OP_CASE(SD_CAP)
				mem->handle_store_data_via_cap(regs[instr.rs2()], instr.rs1(), regs[instr.rs1()],
				                               regs[instr.rs1()].cap.fields.address, 8);
				OP_END();
				OP_CASE(SC_CAP)
				mem->handle_store_data_via_cap(regs[instr.rs2()], instr.rs1(), regs[instr.rs1()],
				                               regs[instr.rs1()].cap.fields.address, cCapSize);
				OP_END();
				OP_CASE(SC_B_DDC) {
					uint64_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					Capability cs2 = regs[instr.rs2()];
					regs[instr.rs2()] = mem->atomic_store_conditional_data_via_cap(cs2, cDdcIdx, ddc, vaddr, 1) ? 0 : 1;
					lr_sc_counter = 0;
					reset_reg_zero();
					if (unlikely(rvfi_dii)) {
						rvfi_dii_output.rvfi_dii_rd_addr = instr.rs2();
						rvfi_dii_output.rvfi_dii_rd_wdata = regs[instr.rs2()];
					}
				}
				OP_END();
				OP_CASE(SC_H_DDC) {
					uint64_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					Capability cs2 = regs[instr.rs2()];
					regs[instr.rs2()] = mem->atomic_store_conditional_data_via_cap(cs2, cDdcIdx, ddc, vaddr, 2) ? 0 : 1;
					lr_sc_counter = 0;
					reset_reg_zero();
					if (unlikely(rvfi_dii)) {
						rvfi_dii_output.rvfi_dii_rd_addr = instr.rs2();
						rvfi_dii_output.rvfi_dii_rd_wdata = regs[instr.rs2()];
					}
				}
				OP_END();
				OP_CASE(SC_W_DDC) {
					uint64_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					Capability cs2 = regs[instr.rs2()];
					regs[instr.rs2()] = mem->atomic_store_conditional_data_via_cap(cs2, cDdcIdx, ddc, vaddr, 4) ? 0 : 1;
					lr_sc_counter = 0;
					reset_reg_zero();
					if (unlikely(rvfi_dii)) {
						rvfi_dii_output.rvfi_dii_rd_addr = instr.rs2();
						rvfi_dii_output.rvfi_dii_rd_wdata = regs[instr.rs2()];
					}
				}
				OP_END();
				OP_CASE(SC_D_DDC) {
					uint64_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					Capability cs2 = regs[instr.rs2()];
					regs[instr.rs2()] = mem->atomic_store_conditional_data_via_cap(cs2, cDdcIdx, ddc, vaddr, 8) ? 0 : 1;
					lr_sc_counter = 0;
					reset_reg_zero();
					if (unlikely(rvfi_dii)) {
						rvfi_dii_output.rvfi_dii_rd_addr = instr.rs2();
						rvfi_dii_output.rvfi_dii_rd_wdata = regs[instr.rs2()];
					}
				}
				OP_END();
				OP_CASE(SC_C_DDC) {
					uint64_t vaddr = get_ddc_addr(regs[instr.rs1()]);
					Capability cs2 = regs[instr.rs2()];
					regs[instr.rs2()] =
					    mem->atomic_store_conditional_data_via_cap(cs2, cDdcIdx, ddc, vaddr, cCapSize) ? 0 : 1;
					lr_sc_counter = 0;
					reset_reg_zero();
					if (unlikely(rvfi_dii)) {
						rvfi_dii_output.rvfi_dii_rd_addr = instr.rs2();
						rvfi_dii_output.rvfi_dii_rd_wdata = regs[instr.rs2()];
					}
				}
				OP_END();
				OP_CASE(SC_B_CAP) {
					Capability cs2 = regs[instr.rs2()];
					Capability cs1 = regs[instr.rs1()];
					// regs[instr.rs2()] = 1;  // failure by default (in case a trap is thrown)
					regs[instr.rs2()] =
					    mem->atomic_store_conditional_data_via_cap(cs2, instr.rs1(), cs1, cs1.cap.fields.address, 1)
					        ? 0
					        : 1;
					lr_sc_counter = 0;
					reset_reg_zero();
					if (unlikely(rvfi_dii)) {
						rvfi_dii_output.rvfi_dii_rd_addr = instr.rs2();
						rvfi_dii_output.rvfi_dii_rd_wdata = regs[instr.rs2()];
					}
				}
				OP_END();
				OP_CASE(SC_H_CAP) {
					Capability cs2 = regs[instr.rs2()];
					Capability cs1 = regs[instr.rs1()];
					// regs[instr.rs2()] = 1;  // failure by default (in case a trap is thrown)
					regs[instr.rs2()] =
					    mem->atomic_store_conditional_data_via_cap(cs2, instr.rs1(), cs1, cs1.cap.fields.address, 2)
					        ? 0
					        : 1;
					lr_sc_counter = 0;
					reset_reg_zero();
					if (unlikely(rvfi_dii)) {
						rvfi_dii_output.rvfi_dii_rd_addr = instr.rs2();
						rvfi_dii_output.rvfi_dii_rd_wdata = regs[instr.rs2()];
					}
				}
				OP_END();
				OP_CASE(SC_W_CAP) {
					Capability cs2 = regs[instr.rs2()];
					Capability cs1 = regs[instr.rs1()];
					// regs[instr.rs2()] = 1;  // failure by default (in case a trap is thrown)
					regs[instr.rs2()] =
					    mem->atomic_store_conditional_data_via_cap(cs2, instr.rs1(), cs1, cs1.cap.fields.address, 4)
					        ? 0
					        : 1;
					lr_sc_counter = 0;
					reset_reg_zero();
					if (unlikely(rvfi_dii)) {
						rvfi_dii_output.rvfi_dii_rd_addr = instr.rs2();
						rvfi_dii_output.rvfi_dii_rd_wdata = regs[instr.rs2()];
					}
				}
				OP_END();
				OP_CASE(SC_D_CAP) {
					Capability cs2 = regs[instr.rs2()];
					Capability cs1 = regs[instr.rs1()];
					// regs[instr.rs2()] = 1;  // failure by default (in case a trap is thrown)
					regs[instr.rs2()] =
					    mem->atomic_store_conditional_data_via_cap(cs2, instr.rs1(), cs1, cs1.cap.fields.address, 8)
					        ? 0
					        : 1;
					lr_sc_counter = 0;
					reset_reg_zero();
					if (unlikely(rvfi_dii)) {
						rvfi_dii_output.rvfi_dii_rd_addr = instr.rs2();
						rvfi_dii_output.rvfi_dii_rd_wdata = regs[instr.rs2()];
					}
				}
				OP_END();
				OP_CASE(SC_C_CAP) {
					Capability cs2 = regs[instr.rs2()];
					Capability cs1 = regs[instr.rs1()];
					// regs[instr.rs2()] = 1;  // failure by default (in case a trap is thrown)
					regs[instr.rs2()] =
					    mem->atomic_store_conditional_data_via_cap(cs2, instr.rs1(), cs1, cs1.cap.fields.address, 16)
					        ? 0
					        : 1;
					lr_sc_counter = 0;
					reset_reg_zero();
					if (unlikely(rvfi_dii)) {
						rvfi_dii_output.rvfi_dii_rd_addr = instr.rs2();
						rvfi_dii_output.rvfi_dii_rd_wdata = regs[instr.rs2()];
					}
				}
				OP_END();
				OP_CASE(LC) {
					Capability auth_val;
					uint64_t vaddr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), instr.I_imm(), &auth_val, &vaddr);
					regs[instr.rd()] = mem->handle_load_cap_via_cap(auth_idx, auth_val, vaddr);
				}
				OP_END();
				OP_CASE(SC) {
					Capability auth_val;
					uint64_t vaddr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), instr.S_imm(), &auth_val, &vaddr);
					mem->handle_store_data_via_cap(regs[instr.rs2()], auth_idx, auth_val, vaddr, 16);
				}
				OP_END();
				OP_CASE(LR_C) {
					Capability auth_val;
					uint64_t vaddr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), 0, &auth_val, &vaddr);
					regs[instr.rd()] = mem->atomic_load_reserved_cap_via_cap(auth_idx, auth_val, vaddr);
					if (lr_sc_counter == 0) {
						lr_sc_counter = 17;  // this instruction + 16 additional ones, (an over-approximation) to
						// cover the RISC-V forward progress property
						force_slow_path();
					}
					reset_reg_zero();
				}
				OP_END();
				OP_CASE(SC_C) {
					Capability auth_val;
					uint64_t vaddr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), 0, &auth_val, &vaddr);
					regs[instr.rd()] =
					    mem->atomic_store_conditional_data_via_cap(regs[instr.rs2()], auth_idx, auth_val, vaddr, 16)
					        ? 0
					        : 1;
					if (unlikely(rvfi_dii)) {
						rvfi_dii_output.rvfi_dii_rd_addr = instr.rd();
						rvfi_dii_output.rvfi_dii_rd_wdata = regs[instr.rd()];
					}
					lr_sc_counter = 0;
					reset_reg_zero();
				}
				OP_END();
				OP_CASE(AMOSWAP_C) {
					Capability auth_val;
					uint64_t vaddr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), 0, &auth_val, &vaddr);
					if (!auth_val.cap.fields.tag) {
						handle_cheri_cap_exception(CapEx_TagViolation, auth_idx, &rvfi_dii_output);
					}
					if (auth_val.isSealed()) {
						handle_cheri_cap_exception(CapEx_SealViolation, auth_idx, &rvfi_dii_output);
					}
					if (!auth_val.cap.fields.permit_load) {
						handle_cheri_cap_exception(CapEx_PermitLoadViolation, auth_idx, &rvfi_dii_output);
					}
					if (!auth_val.cap.fields.permit_store) {
						handle_cheri_cap_exception(CapEx_PermitStoreViolation, auth_idx, &rvfi_dii_output);
					}
					if (!auth_val.cap.fields.permit_store_cap && regs[instr.rs2()].cap.fields.tag) {
						handle_cheri_cap_exception(CapEx_PermitStoreViolation, auth_idx, &rvfi_dii_output);
					}
					if (!auth_val.cap.fields.permit_store_local_cap && regs[instr.rs2()].cap.fields.tag &&
					    !regs[instr.rs2()].cap.fields.global) {
						handle_cheri_cap_exception(CapEx_PermitStoreLocalCapViolation, auth_idx, &rvfi_dii_output);
					}
					if (!auth_val.inCapBounds(vaddr, cCapSize)) {
						handle_cheri_cap_exception(CapEx_LengthViolation, auth_idx, &rvfi_dii_output);
					}
					trap_check_addr_alignment<cCapSize, true>(vaddr);
					Capability data;
					try {
						data = mem->atomic_load_cap(vaddr);
					} catch (SimulationTrap &e) {
						if (e.reason == EXC_LOAD_ACCESS_FAULT) {
							e.reason = EXC_STORE_AMO_ACCESS_FAULT;
						}
						throw e;
					}
					mem->atomic_store_cap(vaddr, regs[instr.rs2()]);
					data.clearTagIf(/*ptw_info.ptw_lc == PTW_LC_CLEAR | */ !auth_val.cap.fields
					                    .permit_load_cap);  // TODO Check what ptw_info should do in VP
					regs[instr.rd()] = data;
				}
				OP_END();
				OP_CASE(LR_B) {
					Capability auth_val;
					uint64_t vaddr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), 0, &auth_val, &vaddr);
					regs[instr.rd()] = mem->atomic_load_reserved_data_via_cap(auth_idx, auth_val, vaddr, 1);
					// TODO: separate counter for SB?
					if (lr_sc_counter == 0) {
						lr_sc_counter = 17;  // this instruction + 16 additional ones, (an over-approximation) to
						// cover the RISC-V forward progress property
						force_slow_path();
					}
					reset_reg_zero();
				}
				OP_END();
				OP_CASE(LR_H) {
					Capability auth_val;
					uint64_t vaddr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), 0, &auth_val, &vaddr);
					regs[instr.rd()] = mem->atomic_load_reserved_data_via_cap(auth_idx, auth_val, vaddr, 2);
					// TODO: separate counter for SH?
					if (lr_sc_counter == 0) {
						lr_sc_counter = 17;  // this instruction + 16 additional ones, (an over-approximation) to
						// cover the RISC-V forward progress property
						force_slow_path();
					}
					reset_reg_zero();
				}
				OP_END();
				OP_CASE(SC_B) {
					Capability auth_val;
					uint64_t vaddr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), 0, &auth_val, &vaddr);
					regs[instr.rd()] =
					    mem->atomic_store_conditional_data_via_cap(regs[instr.rs2()], auth_idx, auth_val, vaddr, 1) ? 0
					                                                                                                : 1;
					if (unlikely(rvfi_dii)) {
						rvfi_dii_output.rvfi_dii_rd_addr = instr.rd();
						rvfi_dii_output.rvfi_dii_rd_wdata = regs[instr.rd()];
					}
					// TODO: separate counter for SC_B?
					lr_sc_counter = 0;
					reset_reg_zero();
				}
				OP_END();
				OP_CASE(SC_H) {
					Capability auth_val;
					uint64_t vaddr;
					uint64_t auth_idx = get_cheri_mode_cap_addr(instr.rs1(), 0, &auth_val, &vaddr);
					regs[instr.rd()] =
					    mem->atomic_store_conditional_data_via_cap(regs[instr.rs2()], auth_idx, auth_val, vaddr, 2) ? 0
					                                                                                                : 1;
					if (unlikely(rvfi_dii)) {
						rvfi_dii_output.rvfi_dii_rd_addr = instr.rd();
						rvfi_dii_output.rvfi_dii_rd_wdata = regs[instr.rd()];
					}
					// TODO: separate counter for SC_H?
					lr_sc_counter = 0;
					reset_reg_zero();
				}
			}
			OP_SWITCH_END();
		} catch (SimulationTrap &e) {
			uxlen_t last_pc = dbbcache.get_last_pc_exception_safe();
			// TODO Change back to only trace mode
			if (trace) {
				std::cout << "take trap " << e.reason << ", mtval=" << boost::format("%x") % e.mtval
				          << ", pc=" << boost::format("%x") % last_pc << std::endl;
			}
			stats.inc_trap(e.reason);

			/*
			 * If a instruction traps, the number of instructions and cycles are
			 * not updated by the above flow.
			 * This is definitly wrong behavior, e.g. for ECALL, EBREAK, etc.
			 * where trapping is their intended behavior, but it difficult e.g.
			 * for faulting load/store instructions (e.g. page fault) and
			 * similar cases.
			 * There is no perfect solution for this, but for the moment, we decide
			 * to count faulting instructions.
			 */
			ninstr++;

			handle_trap(e, last_pc);
		}
	} while (1);

	/*
	 * ensure everything is synchronized before we leave
	 */

	/* update pc member variable */
	pc = dbbcache.get_pc_maybe_after_callback();

	/* update counters by local fast counters */
	commit_instructions(ninstr);
	commit_cycles();

	/* sync quantum: make sure that no action is missed */
	stats.inc_qk_sync();
	quantum_keeper.sync();
}
/*
 * end of exec_steps
 * restore diagnostic (see above)
 */
#pragma GCC diagnostic pop

uint64_t ISS_CT::_compute_and_get_current_cycles() {
	assert(cycle_counter % prop_clock_cycle_period == sc_core::SC_ZERO_TIME);
	assert(cycle_counter.value() % prop_clock_cycle_period.value() == 0);

	uint64_t num_cycles = cycle_counter.value() / prop_clock_cycle_period.value();

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
	bool u_invalid = (csr_prv == UserMode) && !(csrs.misa.has_user_mode_extension());
	// N extension checks based on riscv_sys_control.sail
	// next extension requirements for machine mode
	bool n_invalid = false;
	if (csr_prv == MachineMode) {
		if (csr_addr == csr::MEDELEG_ADDR || csr_addr == csr::MIDELEG_ADDR) {
			n_invalid |= !(csrs.misa.has_supervisor_mode_extension() || csrs.misa.has_N_extension());
		}
	}
	// next extension requirements for supervisor mode
	if (csr_prv == SupervisorMode) {
		if (csr_addr == csr::SEDELEG_ADDR || csr_addr == csr::SIDELEG_ADDR) {
			n_invalid |= !(csrs.misa.has_user_mode_extension() || csrs.misa.has_N_extension());
		}
	}
	// next extension requirements for user mode
	if (csr_prv == UserMode) {
		if (csr_addr == csr::UIE_ADDR || csr_addr == csr::UTVEC_ADDR || csr_addr == csr::USCRATCH_ADDR ||
		    csr_addr == csr::UEPC_ADDR || csr_addr == csr::UCAUSE_ADDR || csr_addr == csr::UTVAL_ADDR ||
		    csr_addr == csr::UIP_ADDR || csr_addr == csr::USTATUS_ADDR) {
			n_invalid |= !(csrs.misa.has_N_extension());
		}
	}

	return (is_write && csr_readonly) || (prv < csr_prv) || s_invalid || u_invalid || n_invalid;
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

	auto read = [=](auto &x, uxlen_t mask) { return x.reg.val & mask; };

	using namespace csr;

	// Check if pc is allowed to access the system registers for all privileged csrs
	// Do not check for unpriviliged csrs
	if (
	    // Unprivileged floating point crs
	    addr != FFLAGS_ADDR && addr != FRM_ADDR && addr != FCSR_ADDR &&
	    // Unprivileged counter/timers
	    addr != CYCLE_ADDR && addr != TIME_ADDR && addr != INSTRET_ADDR && addr != HPMCOUNTER3_ADDR &&
	    addr != HPMCOUNTER4_ADDR && addr != HPMCOUNTER5_ADDR && addr != HPMCOUNTER6_ADDR && addr != HPMCOUNTER7_ADDR &&
	    addr != HPMCOUNTER8_ADDR && addr != HPMCOUNTER9_ADDR && addr != HPMCOUNTER10_ADDR &&
	    addr != HPMCOUNTER11_ADDR && addr != HPMCOUNTER12_ADDR && addr != HPMCOUNTER13_ADDR &&
	    addr != HPMCOUNTER14_ADDR && addr != HPMCOUNTER15_ADDR && addr != HPMCOUNTER16_ADDR &&
	    addr != HPMCOUNTER17_ADDR && addr != HPMCOUNTER18_ADDR && addr != HPMCOUNTER19_ADDR &&
	    addr != HPMCOUNTER20_ADDR && addr != HPMCOUNTER21_ADDR && addr != HPMCOUNTER22_ADDR &&
	    addr != HPMCOUNTER23_ADDR && addr != HPMCOUNTER24_ADDR && addr != HPMCOUNTER25_ADDR &&
	    addr != HPMCOUNTER26_ADDR && addr != HPMCOUNTER27_ADDR && addr != HPMCOUNTER28_ADDR &&
	    addr != HPMCOUNTER29_ADDR && addr != HPMCOUNTER30_ADDR && addr != HPMCOUNTER31_ADDR &&
	    addr != HPMCOUNTER3H_ADDR && addr != HPMCOUNTER4H_ADDR && addr != HPMCOUNTER5H_ADDR &&
	    addr != HPMCOUNTER6H_ADDR && addr != HPMCOUNTER7H_ADDR && addr != HPMCOUNTER8H_ADDR &&
	    addr != HPMCOUNTER9H_ADDR && addr != HPMCOUNTER10H_ADDR && addr != HPMCOUNTER11H_ADDR &&
	    addr != HPMCOUNTER12H_ADDR && addr != HPMCOUNTER13H_ADDR && addr != HPMCOUNTER14H_ADDR &&
	    addr != HPMCOUNTER15H_ADDR && addr != HPMCOUNTER16H_ADDR && addr != HPMCOUNTER17H_ADDR &&
	    addr != HPMCOUNTER18H_ADDR && addr != HPMCOUNTER19H_ADDR && addr != HPMCOUNTER20H_ADDR &&
	    addr != HPMCOUNTER21H_ADDR && addr != HPMCOUNTER22H_ADDR && addr != HPMCOUNTER23H_ADDR &&
	    addr != HPMCOUNTER24H_ADDR && addr != HPMCOUNTER25H_ADDR && addr != HPMCOUNTER26H_ADDR &&
	    addr != HPMCOUNTER27H_ADDR && addr != HPMCOUNTER28H_ADDR && addr != HPMCOUNTER29H_ADDR &&
	    addr != HPMCOUNTER30H_ADDR && addr != HPMCOUNTER31H_ADDR) {
		if (!pc->cap.fields.access_system_regs)
			handle_cheri_reg_exception(CapEx_AccessSystemRegsViolation, cPccIdx, &rvfi_dii_output);
	}

	switch (addr) {
		case TIME_ADDR:
		case MTIME_ADDR: {
			uint64_t mtime = clint->update_and_get_mtime();
			csrs.time.reg.val = mtime;
			return csrs.time.reg.val;
		}

		case CYCLE_ADDR:
		case MCYCLE_ADDR:
			commit_cycles();
			csrs.cycle.reg.val = _compute_and_get_current_cycles();
			return csrs.cycle.reg.val;

		case MINSTRET_ADDR:
			return csrs.instret.reg.val;

		SWITCH_CASE_MATCH_ANY_HPMCOUNTER_RV64:  // not implemented
			return 0;

		// TODO: SD should be updated as SD=XS|FS and SD should be read-only -> update mask
		case MSTATUS_ADDR:
			return read(csrs.mstatus, MSTATUS_READ_MASK);
		case SSTATUS_ADDR:
			return read(csrs.mstatus, SSTATUS_READ_MASK);
		case USTATUS_ADDR:
			if (prv != UserMode)
				RAISE_ILLEGAL_INSTRUCTION();
			return read(csrs.mstatus, USTATUS_READ_MASK);

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
			if (csrs.mstatus.reg.fields.tvm)
				RAISE_ILLEGAL_INSTRUCTION();
			break;

		case FCSR_ADDR:
			return read(csrs.fcsr, FCSR_MASK);

		case FFLAGS_ADDR:
			return csrs.fcsr.reg.fields.fflags;

		case FRM_ADDR:
			return csrs.fcsr.reg.fields.frm;

		case VCSR_ADDR:
			/* mirror vxrm and vxsat in vcsr */
			csrs.vcsr.reg.fields.vxrm = csrs.vxrm.reg.fields.vxrm;
			csrs.vcsr.reg.fields.vxsat = csrs.vxsat.reg.fields.vxsat;
			return csrs.vcsr.reg.val;

		// Cheri capability read checks
		case MEPC_ADDR:
			if (!pc->cap.fields.access_system_regs)
				handle_cheri_reg_exception(CapEx_AccessSystemRegsViolation, cPccIdx, &rvfi_dii_output);
			return legalizeEpcc(csrs.mepcc);
			break;
		case UEPC_ADDR:
			if (!pc->cap.fields.access_system_regs)
				handle_cheri_reg_exception(CapEx_AccessSystemRegsViolation, cPccIdx, &rvfi_dii_output);
			return legalizeEpcc(csrs.uepcc);
			break;
		case SEPC_ADDR:
			if (!pc->cap.fields.access_system_regs)
				handle_cheri_reg_exception(CapEx_AccessSystemRegsViolation, cPccIdx, &rvfi_dii_output);
			return legalizeEpcc(csrs.sepcc);
			break;

		case UTVEC_ADDR:
		case USCRATCH_ADDR:
		case STVEC_ADDR:
		case SSCRATCH_ADDR:
		case MTVEC_ADDR:
		case MSCRATCH_ADDR:
			if (!pc->cap.fields.access_system_regs)
				handle_cheri_reg_exception(CapEx_AccessSystemRegsViolation, cPccIdx, &rvfi_dii_output);
	}

	if (!csrs.is_valid_csr64_addr(addr))
		RAISE_ILLEGAL_INSTRUCTION();

	return csrs.default_read64(addr);
}

void ISS_CT::set_csr_value(uxlen_t addr, uxlen_t value) {
	auto write = [=](auto &x, uxlen_t mask) { x.reg.val = (x.reg.val & ~mask) | (value & mask); };

	using namespace csr;

	switch (addr) {
		case MISA_ADDR:                         // currently, read-only, thus cannot be changed at runtime
		SWITCH_CASE_MATCH_ANY_HPMCOUNTER_RV64:  // not implemented
			break;

		case SATP_ADDR: {
			if (csrs.mstatus.reg.fields.tvm)
				RAISE_ILLEGAL_INSTRUCTION();
			auto mode = csrs.satp.reg.fields.mode;
			write(csrs.satp, SATP_MASK);
			if (csrs.satp.reg.fields.mode != SATP_MODE_BARE && csrs.satp.reg.fields.mode != SATP_MODE_SV39 &&
			    csrs.satp.reg.fields.mode != SATP_MODE_SV48)
				csrs.satp.reg.fields.mode = mode;
			// std::cout << "[iss] satp=" << boost::format("%x") % csrs.satp.reg << std::endl;
		} break;

		case MTVEC_ADDR:
			if (!pc->cap.fields.access_system_regs)
				handle_cheri_reg_exception(CapEx_AccessSystemRegsViolation, cPccIdx, &rvfi_dii_output);
			write(csrs.mtvec, MTVEC_MASK);
			break;
		case STVEC_ADDR:
			if (!pc->cap.fields.access_system_regs)
				handle_cheri_reg_exception(CapEx_AccessSystemRegsViolation, cPccIdx, &rvfi_dii_output);
			write(csrs.stvec, MTVEC_MASK);
			break;
		case UTVEC_ADDR:
			if (!pc->cap.fields.access_system_regs)
				handle_cheri_reg_exception(CapEx_AccessSystemRegsViolation, cPccIdx, &rvfi_dii_output);
			write(csrs.utvec, MTVEC_MASK);
			break;

		case MEPC_ADDR:
			if (!pc->cap.fields.access_system_regs)
				handle_cheri_reg_exception(CapEx_AccessSystemRegsViolation, cPccIdx, &rvfi_dii_output);
			set_xret_target(value, &csrs.mepcc);
			break;
		case SEPC_ADDR:
			if (!pc->cap.fields.access_system_regs)
				handle_cheri_reg_exception(CapEx_AccessSystemRegsViolation, cPccIdx, &rvfi_dii_output);
			set_xret_target(value, &csrs.sepcc);
			break;
		case UEPC_ADDR:
			if (!pc->cap.fields.access_system_regs)
				handle_cheri_reg_exception(CapEx_AccessSystemRegsViolation, cPccIdx, &rvfi_dii_output);
			set_xret_target(value, &csrs.uepcc);
			break;

		case MSTATUS_ADDR:
			write(csrs.mstatus, MSTATUS_WRITE_MASK);
			break;
		case SSTATUS_ADDR:
			write(csrs.mstatus, SSTATUS_WRITE_MASK);
			break;
		case USTATUS_ADDR:
			write(csrs.mstatus, USTATUS_WRITE_MASK);
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
			commit_cycles();
			write(csrs.mcountinhibit, MCOUNTINHIBIT_MASK);
			break;

		case FCSR_ADDR:
			write(csrs.fcsr, FCSR_MASK);
			break;

		case FFLAGS_ADDR:
			csrs.fcsr.reg.fields.fflags = value;
			break;

		case FRM_ADDR:
			csrs.fcsr.reg.fields.frm = value;
			break;

		case VXSAT_ADDR:
			write(csrs.vxsat, VXSAT_MASK);
			/* mirror vxsat in vcsr */
			csrs.vcsr.reg.fields.vxsat = csrs.vxsat.reg.fields.vxsat;
			break;

		case VXRM_ADDR:
			write(csrs.vxrm, VXRM_MASK);
			/* mirror vxrm in vcsr */
			csrs.vcsr.reg.fields.vxrm = csrs.vxrm.reg.fields.vxrm;
			break;

		case VCSR_ADDR:
			write(csrs.vcsr, VCSR_MASK);
			/* mirror vxrm and vxsat in vcsr */
			csrs.vxrm.reg.fields.vxrm = csrs.vcsr.reg.fields.vxrm;
			csrs.vxsat.reg.fields.vxsat = csrs.vcsr.reg.fields.vxsat;
			break;
		// Cheri capability read checks
		case USCRATCH_ADDR:
		case SSCRATCH_ADDR:
		case MSCRATCH_ADDR:
			if (!pc->cap.fields.access_system_regs)
				handle_cheri_reg_exception(CapEx_AccessSystemRegsViolation, cPccIdx, &rvfi_dii_output);
			if (!csrs.is_valid_csr64_addr(addr))
				RAISE_ILLEGAL_INSTRUCTION();

			csrs.default_write64(addr, value);
			break;

		default:
			if (!csrs.is_valid_csr64_addr(addr))
				RAISE_ILLEGAL_INSTRUCTION();

			csrs.default_write64(addr, value);
	}
	/*
	 * interrupt enables may have changed
	 * TODO: optimize -> move to specific csrs above
	 */
	maybe_interrupt_pending();
}
void ISS_CT::init(instr_memory_if *instr_mem, bool use_dbbcache, data_memory_if *data_mem, bool use_lscache,
                  clint_if *clint, uxlen_t entrypoint, uxlen_t sp_base) {
	init(instr_mem, use_dbbcache, data_mem, use_lscache, clint, entrypoint, sp_base, false);
}

void ISS_CT::init(instr_memory_if *instr_mem, bool use_dbbcache, data_memory_if *data_mem, bool use_lscache,
                  clint_if *clint, uxlen_t entrypoint, uxlen_t sp_base, bool cheri_purecap) {
	this->instr_mem = instr_mem;
	this->mem = data_mem;
	this->clint = clint;

	if (cheri_purecap) {
		/* TODO: alignemnt to 8-byte should be sufficient (less than on RV32/RV64 without CHERI -> check */
		regs[RegFile::sp] = rv64_cheriv9_align_address(sp_base);
	} else {
		regs[RegFile::sp] = rv_align_stack_pointer_address(sp_base);
	}

	pc = entrypoint;
	pc->cap.fields.flag_cap_mode =
	    cheri_purecap;  // If purecap mode is set, pc must have capability mode enabled by default

	/* TODO: make const? (make all label ptrs const?) */
	void *fast_abort_and_fdd_labelPtr = genOpMap();

	uint64_t hartId = get_hart_id();
	dbbcache.init(use_dbbcache, isa_config, hartId, instr_mem, opMap, fast_abort_and_fdd_labelPtr, entrypoint,
	              cheri_purecap, &rvfi_dii_output);
	lscache.init(use_lscache, hartId, data_mem);
	cycle_counter_raw_last = 0;
}

void ISS_CT::sys_exit() {
	shall_exit = true;
	force_slow_path();
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

void ISS_CT::set_status(CoreExecStatus s) {
	status = s;
	force_slow_path();
}

void ISS_CT::enable_debug(void) {
	debug_mode = true;
	force_slow_path();
}

void ISS_CT::insert_breakpoint(uint64_t addr) {
	breakpoints.insert(addr);
}

void ISS_CT::remove_breakpoint(uint64_t addr) {
	breakpoints.erase(addr);
}

uint64_t ISS_CT::get_hart_id() {
	return csrs.mhartid.reg.val;
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
	csrs.mstatus.reg.fields.sd = 1;
	csrs.mstatus.reg.fields.fs = FS_DIRTY;
}

void ISS_CT::fp_update_exception_flags() {
	if (softfloat_exceptionFlags) {
		fp_set_dirty();
		csrs.fcsr.reg.fields.fflags |= softfloat_exceptionFlags;
		softfloat_exceptionFlags = 0;
	}
}

void ISS_CT::fp_setup_rm() {
	auto rm = instr.frm();
	if (rm == FRM_DYN)
		rm = csrs.fcsr.reg.fields.frm;
	if (rm > FRM_RMM)
		RAISE_ILLEGAL_INSTRUCTION();
	softfloat_roundingMode = rm;
}

void ISS_CT::fp_require_not_off() {
	if (csrs.mstatus.reg.fields.fs == FS_OFF)
		RAISE_ILLEGAL_INSTRUCTION();
}

void ISS_CT::prepare_xret_target(Capability epcc) {
	Capability legalizedEpcc = legalizeEpcc(epcc);
	pc = legalizedEpcc;
	if (legalizedEpcc.cap.fields.otype == cOtypeSentryUnsigned) {
		pc->unseal();
	}
}

void ISS_CT::set_xret_target(uxlen_t value, csr_eepc_capability *p_epcc) {
	p_epcc->cap = updateCapWithIntegerPC(p_epcc->cap, value);
}

void ISS_CT::return_from_trap_handler(PrivilegeLevel return_mode) {
	switch (return_mode) {
		case MachineMode:
			prv = csrs.mstatus.reg.fields.mpp;
			csrs.mstatus.reg.fields.mie = csrs.mstatus.reg.fields.mpie;
			csrs.mstatus.reg.fields.mpie = 1;
			prepare_xret_target(csrs.mepcc);
			if (csrs.misa.has_user_mode_extension())
				csrs.mstatus.reg.fields.mpp = UserMode;
			else
				csrs.mstatus.reg.fields.mpp = MachineMode;
			break;

		case SupervisorMode:
			prv = csrs.mstatus.reg.fields.spp;
			csrs.mstatus.reg.fields.sie = csrs.mstatus.reg.fields.spie;
			csrs.mstatus.reg.fields.spie = 1;
			prepare_xret_target(csrs.sepcc);
			if (csrs.misa.has_user_mode_extension())
				csrs.mstatus.reg.fields.spp = UserMode;
			else
				csrs.mstatus.reg.fields.spp = SupervisorMode;
			break;

		case UserMode:
			prv = UserMode;
			csrs.mstatus.reg.fields.uie = csrs.mstatus.reg.fields.upie;
			csrs.mstatus.reg.fields.upie = 1;
			prepare_xret_target(csrs.uepcc);
			break;

		default:
			throw std::runtime_error("unknown privilege level " + std::to_string(return_mode));
	}

	if (trace)
		printf("[vp::iss] return from trap handler, time %s, pc %16lx, prv %1x\n",
		       quantum_keeper.get_current_time().to_string().c_str(), pc->cap.fields.address, prv);

	dbbcache.ret_trap(pc);
	force_slow_path();
}

void ISS_CT::trigger_external_interrupt(PrivilegeLevel level) {
	stats.inc_irq_trig_ext(level);

	if (trace)
		std::cout << "[vp::iss] trigger external interrupt, " << sc_core::sc_time_stamp() << std::endl;

	switch (level) {
		case UserMode:
			csrs.mip.reg.fields.ueip = true;
			break;
		case SupervisorMode:
			csrs.mip.reg.fields.seip = true;
			break;
		case MachineMode:
			csrs.mip.reg.fields.meip = true;
			break;
	}

	maybe_interrupt_pending();
}

void ISS_CT::clear_external_interrupt(PrivilegeLevel level) {
	if (trace)
		std::cout << "[vp::iss] clear external interrupt, " << sc_core::sc_time_stamp() << std::endl;

	switch (level) {
		case UserMode:
			csrs.mip.reg.fields.ueip = false;
			break;
		case SupervisorMode:
			csrs.mip.reg.fields.seip = false;
			break;
		case MachineMode:
			csrs.mip.reg.fields.meip = false;
			break;
	}
}

void ISS_CT::trigger_timer_interrupt() {
	stats.inc_irq_trig_timer();

	if (trace)
		std::cout << "[vp::iss] trigger timer interrupt, " << sc_core::sc_time_stamp() << std::endl;
	csrs.mip.reg.fields.mtip = true;
	maybe_interrupt_pending();
}

void ISS_CT::clear_timer_interrupt() {
	if (trace)
		std::cout << "[vp::iss] clear timer interrupt, " << sc_core::sc_time_stamp() << std::endl;
	csrs.mip.reg.fields.mtip = false;
}

void ISS_CT::trigger_software_interrupt() {
	stats.inc_irq_trig_sw();

	if (trace)
		std::cout << "[vp::iss] trigger software interrupt, " << sc_core::sc_time_stamp() << std::endl;
	csrs.mip.reg.fields.msip = true;
	maybe_interrupt_pending();
}

void ISS_CT::clear_software_interrupt() {
	if (trace)
		std::cout << "[vp::iss] clear software interrupt, " << sc_core::sc_time_stamp() << std::endl;
	csrs.mip.reg.fields.msip = false;
}

void ISS_CT::halt() {
	if (debug_mode) {
		this->status = CoreExecStatus::HitBreakpoint;
	}
}

std::string ISS_CT::name() {
	return this->systemc_name;
}

PrivilegeLevel ISS_CT::prepare_trap(SimulationTrap &e, uxlen_t last_pc) {
	// undo any potential pc update (for traps the pc should point to the originating instruction and not it's
	// successor)
	pc = last_pc;
	unsigned exc_bit = (1 << e.reason);

	// 1) machine mode execution takes any traps, independent of delegation setting
	// 2) non-delegated traps are processed in machine mode, independent of current execution mode
	if (prv == MachineMode || !(exc_bit & csrs.medeleg.reg.val)) {
		csrs.mcause.reg.fields.interrupt = 0;
		csrs.mcause.reg.fields.exception_code = e.reason;
		csrs.mtval.reg.val = e.mtval;
		return MachineMode;
	}

	// see above machine mode comment
	if (prv == SupervisorMode || !(exc_bit & csrs.sedeleg.reg.val)) {
		csrs.scause.reg.fields.interrupt = 0;
		csrs.scause.reg.fields.exception_code = e.reason;
		csrs.stval.reg.val = e.mtval;
		return SupervisorMode;
	}

	assert(prv == UserMode && (exc_bit & csrs.medeleg.reg.val) && (exc_bit & csrs.sedeleg.reg.val));
	csrs.ucause.reg.fields.interrupt = 0;
	csrs.ucause.reg.fields.exception_code = e.reason;
	csrs.utval.reg.val = e.mtval;
	return UserMode;
}

void ISS_CT::prepare_interrupt(const PendingInterrupts &e) {
	if (trace) {
		std::cout << "[vp::iss] prepare interrupt, pending=" << e.pending << ", target-mode=" << e.target_mode
		          << std::endl;
	}

	csr_mip x{e.pending};

	ExceptionCode exc;
	if (x.reg.fields.meip)
		exc = EXC_M_EXTERNAL_INTERRUPT;
	else if (x.reg.fields.msip)
		exc = EXC_M_SOFTWARE_INTERRUPT;
	else if (x.reg.fields.mtip)
		exc = EXC_M_TIMER_INTERRUPT;
	else if (x.reg.fields.seip)
		exc = EXC_S_EXTERNAL_INTERRUPT;
	else if (x.reg.fields.ssip)
		exc = EXC_S_SOFTWARE_INTERRUPT;
	else if (x.reg.fields.stip)
		exc = EXC_S_TIMER_INTERRUPT;
	else if (x.reg.fields.ueip)
		exc = EXC_U_EXTERNAL_INTERRUPT;
	else if (x.reg.fields.usip)
		exc = EXC_U_SOFTWARE_INTERRUPT;
	else if (x.reg.fields.utip)
		exc = EXC_U_TIMER_INTERRUPT;
	else
		throw std::runtime_error("some pending interrupt must be available here");

	switch (e.target_mode) {
		case MachineMode:
			csrs.mcause.reg.fields.exception_code = exc;
			csrs.mcause.reg.fields.interrupt = 1;
			break;

		case SupervisorMode:
			csrs.scause.reg.fields.exception_code = exc;
			csrs.scause.reg.fields.interrupt = 1;
			break;

		case UserMode:
			csrs.ucause.reg.fields.exception_code = exc;
			csrs.ucause.reg.fields.interrupt = 1;
			break;

		default:
			throw std::runtime_error("unknown privilege level " + std::to_string(e.target_mode));
	}
}

PendingInterrupts ISS_CT::compute_pending_interrupts() {
	uxlen_t pending = csrs.mie.reg.val & csrs.mip.reg.val;

	if (!pending)
		return {NoneMode, 0};

	auto m_pending = pending & ~csrs.mideleg.reg.val;
	if (m_pending && (prv < MachineMode || (prv == MachineMode && csrs.mstatus.reg.fields.mie))) {
		return {MachineMode, m_pending};
	}

	pending = pending & csrs.mideleg.reg.val;
	auto s_pending = pending & ~csrs.sideleg.reg.val;
	if (s_pending && (prv < SupervisorMode || (prv == SupervisorMode && csrs.mstatus.reg.fields.sie))) {
		return {SupervisorMode, s_pending};
	}

	auto u_pending = pending & csrs.sideleg.reg.val;
	if (u_pending && (prv == UserMode && csrs.mstatus.reg.fields.uie)) {
		return {UserMode, u_pending};
	}

	return {NoneMode, 0};
}

void ISS_CT::switch_to_trap_handler(PrivilegeLevel target_mode) {
	if (trace) {
		printf("[vp::iss] switch to trap handler, time %s, before pc %16lx, irq %u, t-prv %1x\n",
		       quantum_keeper.get_current_time().to_string().c_str(), pc->cap.fields.address,
		       csrs.mcause.reg.fields.interrupt, target_mode);
	}

	// free any potential LR/SC bus lock before processing a trap/interrupt
	release_lr_sc_reservation();

	auto pp = prv;
	prv = target_mode;

	switch (target_mode) {
		case MachineMode:
			csrs.mepcc = pc.pcc;

			csrs.mstatus.reg.fields.mpie = csrs.mstatus.reg.fields.mie;
			csrs.mstatus.reg.fields.mie = 0;
			csrs.mstatus.reg.fields.mpp = pp;

			pc = csrs.mtcc.get_base_address();

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

			if (csrs.mcause.reg.fields.interrupt && csrs.mtvec.reg.fields.mode == csr_mtvec::Mode::Vectored)
				pc += 4 * csrs.mcause.reg.fields.exception_code;
			break;

		case SupervisorMode:
			assert(prv == SupervisorMode || prv == UserMode);

			csrs.sepcc = pc.pcc;

			csrs.mstatus.reg.fields.spie = csrs.mstatus.reg.fields.sie;
			csrs.mstatus.reg.fields.sie = 0;
			csrs.mstatus.reg.fields.spp = pp;

			pc = csrs.stcc.get_base_address();

			if (csrs.scause.reg.fields.interrupt && csrs.stvec.reg.fields.mode == csr_mtvec::Mode::Vectored)
				pc += 4 * csrs.scause.reg.fields.exception_code;
			break;

		case UserMode:
			assert(prv == UserMode);

			csrs.uepcc = pc.pcc;

			csrs.mstatus.reg.fields.upie = csrs.mstatus.reg.fields.uie;
			csrs.mstatus.reg.fields.uie = 0;

			pc = csrs.utcc.get_base_address();

			if (csrs.ucause.reg.fields.interrupt && csrs.utvec.reg.fields.mode == csr_mtvec::Mode::Vectored)
				pc += 4 * csrs.ucause.reg.fields.exception_code;
			break;

		default:
			throw std::runtime_error("unknown privilege level " + std::to_string(target_mode));
	}
	if (unlikely(rvfi_dii)) {
		rvfi_dii_output.rvfi_dii_pc_wdata = pc;
	}
	// if (pc->cap.fields.flag_cap_mode != 1) {
	// 	std::cout << "[ISS] Warning: pc is not in capability mode, this is probably an error." << std::endl;
	// }

	dbbcache.enter_trap(pc);
}

void ISS_CT::handle_interrupt() {
	auto x = compute_pending_interrupts();
	if (x.target_mode != NoneMode) {
		prepare_interrupt(x);
		switch_to_trap_handler(x.target_mode);
	}
}

void ISS_CT::handle_trap(SimulationTrap &e, uxlen_t last_pc) {
	auto target_mode = prepare_trap(e, last_pc);
	switch_to_trap_handler(target_mode);
}

void ISS_CT::run_step() {
	if (!debug_mode) {
		throw std::runtime_error("call of run_step with disabled debug-mode not permitted");
	}
	exec_steps(true);
}

void ISS_CT::run() {
	exec_steps(false);
}

void ISS_CT::show() {
	boost::io::ios_flags_saver ifs(std::cout);
	print_stats();
	std::cout << "=[ core : " << csrs.mhartid.reg.val << " ]===========================" << std::endl;
	std::cout << "simulation time: " << sc_core::sc_time_stamp() << std::endl;
	regs.show();
	std::cout << "pc = " << std::hex << pc << std::endl;
	std::cout << "num-instr = " << std::dec << csrs.instret.reg.val << std::endl;
	// Note: Like mcycles -> Does not contain any cycles that were executed while the CSR bit mcountinhibit.CY was set.
	std::cout << "num-cycles (mcycle) = " << _compute_and_get_current_cycles() << std::endl;
}

void ISS_CT::init_cheri_regs() {
	pc = cDefaultCap;
	ddc = cDefaultCap;
	regs = RegFile(rvfi_dii);
}

void ISS_CT::reset() {
	init_cheri_regs();
	csrs.reset();
	if (unlikely(rvfi_dii)) {
		csrs.misa.reg.fields.extensions &= ~rv64::csr_misa_64::N;
		csrs.misa.reg.fields.extensions &= ~rv64::csr_misa_64::V;
	}
	if ((csrs.misa.reg.fields.extensions & rv64::csr_misa_64::V) != rv64::csr_misa_64::V) {
		// Clear V in mstatus
		csrs.mstatus.reg.fields.vs = 0;
	}
	// TODO Reset dbb cache?
	prv = MachineMode;
}

void ISS_CT::execute_c_jalr(int32_t immediate) {
	Capability cs1_val = regs[instr.rs1()];
	int64_t off = static_cast<int64_t>(immediate);
	uint64_t newPC = cs1_val.cap.fields.address + off;
	newPC = (newPC & ~1);  // clear bit zero as for RISCV JALR
	CapAddr_t newPCCBase = cs1_val.getBase();
	if (!cs1_val.cap.fields.tag) {
		handle_cheri_reg_exception(CapEx_TagViolation, instr.rs1(), &rvfi_dii_output);
	} else if (cs1_val.isSealed() & ((cs1_val.cap.fields.otype != cOtypeSentryUnsigned) | (off != 0))) {
		handle_cheri_reg_exception(CapEx_SealViolation, instr.rs1(), &rvfi_dii_output);
	} else if (!cs1_val.cap.fields.permit_execute) {
		handle_cheri_reg_exception(CapEx_PermitExecuteViolation, instr.rs1(), &rvfi_dii_output);
	} else if (have_pcc_relocation() &&
	           (((newPCCBase & 0b01) == 0b01) || (((newPCCBase & 0b10) == 0b10) && !csrs.misa.has_C_extension()))) {
		handle_cheri_reg_exception(CapEx_UnalignedBase, instr.rs1(), &rvfi_dii_output);
	} else if ((newPC & 1) && !csrs.misa.has_C_extension()) {
		handle_mem_exception(newPC, E_FetchAddrAlign, &rvfi_dii_output);
	} else if (!cs1_val.inCapBounds(newPC, min_instruction_bytes())) {
		handle_cheri_reg_exception(CapEx_LengthViolation, instr.rs1(), &rvfi_dii_output);
	} else {
		regs[instr.rd()] = dbbcache.jump_dyn_and_link(newPC, cs1_val, true).pcc;
	}
	// Write to rd=0 must be ignored
	reset_reg_zero();
}

/*!
 * For given base register and offset returns, depending on current capability
 * mode flag, a bounding capability, effective address, and capreg_idx (for use
 * in cap cause).
 */
uint64_t ISS_CT::get_cheri_mode_cap_addr(uint64_t base_reg, uint64_t offset, Capability *auth_val, uint64_t *vaddr) {
	Capability base_cap = regs[base_reg];
	if (pc->cap.fields.flag_cap_mode) {
		*auth_val = base_cap;
		*vaddr = base_cap.cap.fields.address + offset;
		return base_reg;
	}
	*vaddr = base_cap.cap.fields.address + offset;
	// Integer mode
	if (have_ddc_relocation()) {
		*vaddr += ddc.cap.fields.address;
	}
	*auth_val = ddc;
	return cDdcIdx;
}

uint64_t ISS_CT::get_ddc_addr(uint64_t addr) {
	if (have_ddc_relocation()) {
		return addr + ddc.cap.fields.address;
	}
	return addr;
}
} /* namespace cheriv9::rv64 */
