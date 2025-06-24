/*
 * Copyright (C) 2024-2025 Manfred Schlaegl <manfred.schlaegl@gmx.at>
 */

#ifndef RISCV_ISA_ISS_STATS_H
#define RISCV_ISA_ISS_STATS_H

#include <cstdint>
#include <iostream>

#include "instr.h"

/*
 * dummy implementation
 * = interface and high efficient (all calls optimized out)
 */
class ISSStatsDummy {
   protected:
	uint64_t hartId;

   public:
	ISSStatsDummy(uint64_t hartId) : hartId(hartId) {}
	/* dec methods are used for aborts */
	void reset() {}
	void inc_cnt() {}
	void dec_cnt() {}
	void inc_fast_fdd() {}
	void dec_fast_fdd() {}
	void inc_fast_fdd_abort() {}
	void inc_med_fdd() {}
	void inc_slow_fdd() {}
	void inc_lr_sc() {}
	void inc_commit_instructions() {}
	void inc_commit_cycles() {}
	void inc_qk_need_sync() {}
	void inc_qk_sync() {}
	void inc_nops() {}
	void inc_jal() {}
	void inc_j() {}
	void inc_jalr() {}
	void inc_jr() {}
	void inc_loadstore() {}
	void inc_csr() {}
	void inc_amo() {}
	void inc_set_zero() {}
	void inc_fence_i() {}
	void inc_fence_vma() {}
	void inc_wfi() {}
	void inc_uret() {}
	void inc_mret() {}
	void inc_sret() {}
	void inc_trap(unsigned int trapnr) {}
	void inc_op(Operation::OpId opId) {}
	void print() {}
};

class ISSStats : public ISSStatsDummy {
   private:
	static constexpr unsigned int TRAPNR_MAX = 31;

	struct {
		uint64_t cnt;
		uint64_t fast_fdd;
		uint64_t fast_fdd_abort;
		uint64_t med_fdd;
		uint64_t slow_fdd;
		uint64_t lr_sc;
		uint64_t commit_instructions;
		uint64_t commit_cycles;
		uint64_t qk_need_sync;
		uint64_t qk_sync;
		uint64_t nops;
		uint64_t jal;
		uint64_t j;
		uint64_t jalr;
		uint64_t jr;
		uint64_t loadstore;
		uint64_t csr;
		uint64_t amo;
		uint64_t set_zero;
		uint64_t fence_i;
		uint64_t fence_vma;
		uint64_t wfi;
		uint64_t uret;
		uint64_t mret;
		uint64_t sret;
		uint64_t trap_sum;
		uint64_t trap[TRAPNR_MAX + 1];
		uint64_t op_sum;
		uint64_t op[Operation::OpId::NUMBER_OF_OPERATIONS];
	} s;

   public:
	ISSStats(uint64_t hartId) : ISSStatsDummy(hartId) {
		reset();
	}

	void reset();

	void inc_cnt() {
		s.cnt++;
		/*
		 * print statistics periodically based on cnt
		 * TODO: find cleaner, similar efficient way for periodic output (maybe centrally controlled? -> global stats
		 * module?)
		 */
		if ((s.cnt & 0xfffffff) == 0) {
			print();
		}
	}
	void dec_cnt() {
		s.cnt--;
	}
	void inc_fast_fdd() {
		s.fast_fdd++;
	}
	void dec_fast_fdd() {
		s.fast_fdd--;
	}
	void inc_fast_fdd_abort() {
		dec_fast_fdd();
		s.fast_fdd_abort++;
	}
	void inc_med_fdd() {
		s.med_fdd++;
	}
	void inc_slow_fdd() {
		s.slow_fdd++;
	}
	void inc_lr_sc() {
		s.lr_sc++;
	}
	void inc_commit_instructions() {
		s.commit_instructions++;
	}
	void inc_commit_cycles() {
		s.commit_cycles++;
	}
	void inc_qk_need_sync() {
		s.qk_need_sync++;
	}
	void inc_qk_sync() {
		s.qk_sync++;
	}
	void inc_nops() {
		s.nops++;
	}
	void inc_jal() {
		s.jal++;
	}
	void inc_j() {
		s.j++;
	}
	void inc_jalr() {
		s.jalr++;
	}
	void inc_jr() {
		s.jr++;
	}
	void inc_loadstore() {
		s.loadstore++;
	}
	void inc_csr() {
		s.csr++;
	}
	void inc_amo() {
		s.amo++;
	}
	void inc_set_zero() {
		s.set_zero++;
	}
	void inc_fence_i() {
		s.fence_i++;
	}
	void inc_fence_vma() {
		s.fence_vma++;
	}
	void inc_wfi() {
		s.wfi++;
	}
	void inc_uret() {
		s.uret++;
	}
	void inc_mret() {
		s.mret++;
	}
	void inc_sret() {
		s.sret++;
	}
	void inc_op(Operation::OpId opId) {
		s.op_sum++;
		s.op[opId]++;
	}
	void inc_trap(unsigned int trapnr) {
		if (trapnr > TRAPNR_MAX) {
			std::cerr << "ISS_STATS: invalid trap nr " << trapnr << std::endl;
			return;
		}
		s.trap_sum++;
		s.trap[trapnr]++;
	}

	void print();
};

#endif /* RISCV_ISA_ISS_STATS_H */
