/*
 * Dynamic Basic Block Cache Dummy for CHERIv9
 * see core/common/dbbcache for full rv32 and rv64 variant
 */

#ifndef RISCV_CHERIV9_ISA_DBBCACHE_H
#define RISCV_CHERIV9_ISA_DBBCACHE_H

#include <climits>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "../rv64_cheriv9/cheri_addr_checks.h"        // TODO: rv64 should not be included in common
#include "../rv64_cheriv9/rvfi-dii/rvfi_dii_trace.h"  // TODO: rv64 should not be included in common
#include "cheri_cap_common.h"
#include "cheri_exceptions.h"
#include "core/common/core_defs.h"
#include "core/common/trap.h"
#include "instr.h"
#include "util/common.h"

namespace cheriv9 {

/******************************************************************************
 * BEGIN: MISC
 ******************************************************************************/

struct OpMapEntry {
	/* order optimized for alignment */
	Operation::OpId opId;
	uint64_t instr_time;
	void *labelPtr;
};

/******************************************************************************
 * END: MISC
 ******************************************************************************/

/******************************************************************************
 * BEGIN: COMMON BASE CLASS
 ******************************************************************************/

template <enum Architecture arch, typename T_uxlen_t, typename T_instr_memory_if>
class DBBCacheBase_T {
   protected:
	bool enabled = false;
	RV_ISA_Config *isa_config = nullptr;
	uint64_t hartId = 0;
	bool has_compressed = false;
	T_instr_memory_if *instr_mem = nullptr;
	struct OpMapEntry *opMap = nullptr;
	void *fast_abort_labelPtr = nullptr;
	uint32_t mem_word = 0x0;
	uint8_t min_instruction_bytes = 0;
	rvfi_dii_trace_t *rvfi_dii_output;

   public:
	DBBCacheBase_T() {
		init(false, nullptr, 0, nullptr, nullptr, nullptr, 0, false, nullptr);
	}

	void init(bool enabled, RV_ISA_Config *isa_config, uint64_t hartId, T_instr_memory_if *instr_mem,
	          struct OpMapEntry opMap[], void *fast_abort_labelPtr, T_uxlen_t entrypoint, bool cheri_purecap,
	          rvfi_dii_trace_t *rvfi_dii_output) {
		this->enabled = enabled;
		this->isa_config = isa_config;
		this->hartId = hartId;
		if (isa_config != nullptr) {
			this->has_compressed = isa_config->get_misa_extensions();
		} else {
			this->has_compressed = false;
		}
		this->instr_mem = instr_mem;
		this->opMap = opMap;
		this->fast_abort_labelPtr = fast_abort_labelPtr;
		this->mem_word = 0;
		// this->min_instruction_bytes = this->has_compressed ? 4 : 2;
		this->min_instruction_bytes = 2;  // TODO: This is what is required by rvfi-dii...
		this->rvfi_dii_output = rvfi_dii_output;
	}

	__always_inline bool is_enabled() {
		return enabled;
	}
};

/******************************************************************************
 * END: COMMON BASE CLASS
 ******************************************************************************/

/******************************************************************************
 * BEGIN: DUMMY IMPLEMENTATION
 ******************************************************************************/

template <enum Architecture arch, typename T_uxlen_t, typename T_instr_memory_if>
class DBBCacheDummy_T : public DBBCacheBase_T<arch, T_uxlen_t, T_instr_memory_if> {
   private:
	ProgramCounterCapability pc;
	ProgramCounterCapability last_pc;
	uint64_t cycle_counter_raw = 0;

	__always_inline uint32_t fetch(ProgramCounterCapability &pc, Instruction &instr) {
		try {
			// Fetch is split in 2 * 16 bits, because if last instruction is compressed, CHERI checks would fail if 32
			// bits were fetched First handle CHERI checks on pc
#ifdef HANDLE_CHERI_EXCEPTIONS
			cheriFetchCheckPc(pc, pc, this->rvfi_dii_output, this->has_compressed);
			uint16_t instr_low = this->instr_mem->load_instr_half(pc->cap.fields.address);
			instr = Instruction(instr_low);
			if (instr.is_compressed()) {
				return instr_low;  // Compressed instruction, return immediately
			}
			// If not compressed, fetch the second half of the instruction
			cheriFetchCheckPc(pc, pc + 2, this->rvfi_dii_output, this->has_compressed);
			uint16_t instr_high = this->instr_mem->load_instr_half(pc->cap.fields.address + 2);
			uint32_t mem_word = (instr_high << 16) | instr_low;
#else
			uint32_t mem_word = this->instr_mem->load_instr(pc->cap.fields.address);
#endif
			instr = Instruction(mem_word);
			return mem_word;
		} catch (SimulationTrap &e) {
			instr = Instruction(0);
			throw;
		}
	}

	__always_inline int decode(Instruction &instr, Operation::OpId &opId, bool flag_cap_mode) {
		if (instr.is_compressed()) {
			opId = instr.decode_and_expand_compressed(arch, *this->isa_config, flag_cap_mode);
			return 2;
		} else {
			opId = instr.decode_normal(arch, *this->isa_config);
			return 4;
		}
	}

	__always_inline uint32_t fetch_rvfi_decode(ProgramCounterCapability &pc, Instruction &instr, Operation::OpId &opId,
	                                           uint64_t instr_val, uint8_t &decoded_instr_len) {
		// CHERI PC checks must still be performed, although no actual memory fetch is done
		cheriFetchCheckPc(pc, pc, this->rvfi_dii_output, this->has_compressed);
		// Perform v2p address translation, but do not actually fetch the instruction
		// This is to ensure that all checks on PC are performed as in Sail
		this->instr_mem->translate_pc(
		    pc->cap.fields.address);  // Attempt to read from memory, because load checks must still be performed
		instr = instr_val;
		decoded_instr_len = 2;
		uint32_t mem_word = instr.data();
		if (!instr.is_compressed()) {
			cheriFetchCheckPc(pc, pc + 2, this->rvfi_dii_output, this->has_compressed);
			this->instr_mem->translate_pc(
			    pc->cap.fields.address +
			    2);  // Attempt to read from memory, because load checks must still be performed
			decoded_instr_len = 4;
		}
		pc += decode(instr, opId, pc->cap.fields.flag_cap_mode);
		return mem_word;
	}

	__always_inline uint32_t fetch_decode(ProgramCounterCapability &pc, Instruction &instr, Operation::OpId &opId) {
		uint32_t mem_word = fetch(pc, instr);
		pc += decode(instr, opId, pc->cap.fields.flag_cap_mode);
		return mem_word;
	}

	__always_inline void branch_taken_sjump(int32_t pc_offset, bool c_extension) {
		uint64_t newPc = this->last_pc->cap.fields.address + pc_offset;
		if (!pc.pcc.inCapBounds(newPc, this->min_instruction_bytes)) {
			handle_cheri_cap_exception(CapEx_LengthViolation, 0, this->rvfi_dii_output);
		}
		if (pc->cap.fields.flag_cap_mode & (newPc & 1) & !c_extension) {
			assert(0);  // TODO
			            // handle_mem_exception(newPC, E_Fetch_Addr_Align()); // TODO
		}
		this->pc = this->last_pc->cap.fields.address + pc_offset;
	}

   public:
	DBBCacheDummy_T() {
		init(false, nullptr, 0, nullptr, nullptr, nullptr, 0, false, nullptr);
	}

	void init(bool enabled, RV_ISA_Config *isa_config, uint64_t hartId, T_instr_memory_if *instr_mem,
	          struct OpMapEntry opMap[], void *fast_abort_labelPtr, T_uxlen_t entrypoint, bool cheri_purecap,
	          rvfi_dii_trace_t *rvfi_dii_output) {
		DBBCacheBase_T<arch, T_uxlen_t, T_instr_memory_if>::init(enabled, isa_config, hartId, instr_mem, opMap,
		                                                         fast_abort_labelPtr, entrypoint, cheri_purecap,
		                                                         rvfi_dii_output);
		this->pc = entrypoint;
		this->pc->cap.fields.flag_cap_mode = cheri_purecap;
#ifdef HANDLE_CHERI_EXCEPTIONS
		std::cout << "WARNING: CHERI exception handling is enabled!" << std::endl;
#endif
	}

	__always_inline void branch_not_taken(ProgramCounterCapability pc) {}

	__always_inline void branch_taken(int32_t pc_offset) {
		branch_taken_sjump(pc_offset, (this->isa_config->get_misa_extensions() & csr_misa::C) == csr_misa::C);
	}

	__always_inline void jump(int32_t pc_offset) {
		branch_taken_sjump(pc_offset, (this->isa_config->get_misa_extensions() & csr_misa::C) == csr_misa::C);
	}

	__always_inline ProgramCounterCapability jump_and_link(int32_t pc_offset) {
		ProgramCounterCapability link = pc;
		branch_taken_sjump(pc_offset, (this->isa_config->get_misa_extensions() & csr_misa::C) == csr_misa::C);
		if (pc->cap.fields.flag_cap_mode) {
			link.pcc.setCapAddr(link->cap.fields.address);
			link.pcc.seal(cOtypeSentryUnsigned);
			return link;
		}
		// Integer pointer mode
		link->clearMetadata();
		link->cap.fields.tag = false;
		return link;
	}

	__always_inline void jump_dyn(T_uxlen_t pc) {
		this->pc = pc;
	}
	__always_inline ProgramCounterCapability jump_dyn_and_link(T_uxlen_t pc, bool cap_mode) {
		return jump_dyn_and_link(pc, cNullCap, cap_mode);
	}
	__always_inline ProgramCounterCapability jump_dyn_and_link(T_uxlen_t pc, Capability cs1, bool cap_mode) {
		ProgramCounterCapability link = this->pc;
		if (cap_mode) {
			Capability linkCap = this->pc;
			bool success = linkCap.setCapAddr(this->pc);
			linkCap.seal(cOtypeSentryUnsigned);
			linkCap.clearTagIf(!success);
			this->pc = cs1;
			this->pc->unseal();
			this->pc = pc;  // Set address to new address
			link.pcc = linkCap;
			return link;
		}
		// Integer pointer mode
		if (!this->pc.pcc.inCapBounds(pc, this->min_instruction_bytes)) {
			handle_cheri_cap_exception(CapEx_LengthViolation, 0, this->rvfi_dii_output);
		}
		this->pc = pc;
		link->clearMetadata();
		link->cap.fields.tag = false;
		return link;
	}

	__always_inline void fence_i(ProgramCounterCapability pc) {}

	__always_inline void fence_vma(ProgramCounterCapability pc) {}

	__always_inline void enter_trap(ProgramCounterCapability pc) {
		this->pc = pc;
	}

	__always_inline void ret_trap(ProgramCounterCapability pc) {
		this->pc = pc;
	}

	__always_inline void force_slow_path() {}

	__always_inline bool in_fast_path() {
		return false;
	}

	__always_inline void *fetch_decode_fast(Instruction &instr) {
		return this->fast_abort_labelPtr;
	}

	__always_inline void abort_fetch_decode_fast() {}

	__always_inline void *fetch_decode(ProgramCounterCapability &pc, Instruction &instr) {
		Operation::OpId opId;
		this->last_pc = this->pc;
		this->mem_word = fetch_decode(pc, instr, opId);
		this->pc = pc;
		cycle_counter_raw += this->opMap[opId].instr_time;
		return this->opMap[opId].labelPtr;
	}

	__always_inline void *fetch_rvfi_decode(ProgramCounterCapability &pc, Instruction &instr, uint64_t instr_val,
	                                        uint8_t &decoded_instr_len) {
		Operation::OpId opId;
		this->last_pc = this->pc;
		this->mem_word = fetch_rvfi_decode(pc, instr, opId, instr_val, decoded_instr_len);
		this->pc = pc;
		cycle_counter_raw += this->opMap[opId].instr_time;
		return this->opMap[opId].labelPtr;
	}

	__always_inline ProgramCounterCapability get_last_pc_before_callback() {
		return last_pc;
	}

	__always_inline ProgramCounterCapability get_last_pc_exception_safe() {
		return last_pc;
	}

	/* TODO: UNUSED - REMOVE? */
	__always_inline ProgramCounterCapability get_pc_before_callback() {
		return pc;
	}

	__always_inline ProgramCounterCapability get_pc_maybe_after_callback() {
		return pc;
	}

	__always_inline uint64_t get_cycle_counter_raw() {
		return cycle_counter_raw;
	}

	uint32_t get_mem_word() {
		return this->mem_word;
	}

	// Required for RVFI-DII
	__always_inline void set_pc(ProgramCounterCapability pc) {
		this->pc = pc;
	}
	__always_inline void set_last_pc(ProgramCounterCapability pc) {
		this->last_pc = pc;
	}
};

/******************************************************************************
 * END: DUMMY IMPLEMENTATION
 ******************************************************************************/

/******************************************************************************
 * CACHE SELECT
 ******************************************************************************/

template <enum Architecture arch, typename T_uxlen_t, typename T_instr_memory_if>
using DBBCacheDefault_T = DBBCacheDummy_T<arch, T_uxlen_t, T_instr_memory_if>;

/******************************************************************************
 * CACHE SELECT
 ******************************************************************************/

} /* namespace cheriv9 */

#endif /* RISCV_CHERIV9_ISA_DBBCACHE_H */
