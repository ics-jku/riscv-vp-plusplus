/*
 * Copyright (C) 2024 Manfred Schlaegl <manfred.schlaegl@gmx.at>
 */

#ifndef RISCV_ISA_ISS_STATS_H
#define RISCV_ISA_ISS_STATS_H

#include <cstdint>

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
	void print() {}
};

class ISSStats : public ISSStatsDummy {
   private:
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

	void print();
};

#endif /* RISCV_ISA_ISS_STATS_H */
