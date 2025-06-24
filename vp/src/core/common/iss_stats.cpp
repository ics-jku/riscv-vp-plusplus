#include "iss_stats.h"

#include <cstring>
#include <iostream>

/*
 * enable/disabled opId stat output (disabled by default)
 * (counting is always enabled in ISSStats)
 */
// #define ISS_STATS_OUTPUT_OPID_STATS_ENABLED
#undef ISS_STATS_OUTPUT_OPID_STATS_ENABLED

void ISSStats::reset() {
	memset(&s, 0, sizeof(s));
}

#define ISSSTATS_STAT_RATE_ONLY(_val, _sum) "(" << (double)(_val) / (_sum) << ")\n"
#define ISSSTATS_STAT_RATE(_val, _sum) (_val) << "\t\t" << ISSSTATS_STAT_RATE_ONLY(_val, _sum)
#define ISSSTATS_STAT_RATE_CNT(_val) ISSSTATS_STAT_RATE(_val, s.cnt)

void ISSStats::print() {
	std::cout << "============================================================================================="
	             "==============================\n";
	std::cout << "ISS Stats (hartId: " << this->hartId << "):\n" << std::dec;
	std::cout << " instr:                     " << s.cnt << "\n";
	std::cout << " fast_fdd:                  " << ISSSTATS_STAT_RATE_CNT(s.fast_fdd);
	std::cout << " fast_fdd_abort:            " << ISSSTATS_STAT_RATE_CNT(s.fast_fdd_abort);
	std::cout << " med_fdd:                   " << ISSSTATS_STAT_RATE_CNT(s.med_fdd);
	std::cout << " slow_fdd:                  " << ISSSTATS_STAT_RATE_CNT(s.slow_fdd);
	std::cout << " lr_sc:                     " << ISSSTATS_STAT_RATE_CNT(s.lr_sc);
	std::cout << " commit_instructions:       " << ISSSTATS_STAT_RATE_CNT(s.commit_instructions);
	std::cout << " commit_cycles:             " << ISSSTATS_STAT_RATE_CNT(s.commit_cycles);
	std::cout << " qk_need_sync:              " << ISSSTATS_STAT_RATE_CNT(s.qk_need_sync);
	std::cout << " qk_sync:                   " << ISSSTATS_STAT_RATE_CNT(s.qk_sync);
	std::cout << " NOP:                       " << ISSSTATS_STAT_RATE_CNT(s.nops);
	std::cout << " jal:                       " << ISSSTATS_STAT_RATE_CNT(s.jal);
	std::cout << " j:                         " << ISSSTATS_STAT_RATE_CNT(s.j);
	std::cout << " jalr:                      " << ISSSTATS_STAT_RATE_CNT(s.jalr);
	std::cout << " jr:                        " << ISSSTATS_STAT_RATE_CNT(s.jr);
	std::cout << " loadstore:                 " << ISSSTATS_STAT_RATE_CNT(s.loadstore);
	std::cout << " CSR:                       " << ISSSTATS_STAT_RATE_CNT(s.csr);
	std::cout << " AMO:                       " << ISSSTATS_STAT_RATE_CNT(s.amo);
	std::cout << " set_zero:                  " << ISSSTATS_STAT_RATE_CNT(s.set_zero);
	std::cout << " fence_i:                   " << ISSSTATS_STAT_RATE_CNT(s.fence_i);
	std::cout << " fence_vma:                 " << ISSSTATS_STAT_RATE_CNT(s.fence_vma);
	std::cout << " wfi:                       " << ISSSTATS_STAT_RATE_CNT(s.wfi);
	std::cout << " uret:                      " << ISSSTATS_STAT_RATE_CNT(s.uret);
	std::cout << " sret:                      " << ISSSTATS_STAT_RATE_CNT(s.sret);
	std::cout << " mret:                      " << ISSSTATS_STAT_RATE_CNT(s.mret);
	std::cout << " trap_sum:                  " << s.trap_sum << "\n";
	for (unsigned int trapnr = 0; trapnr <= TRAPNR_MAX; trapnr++) {
		if (s.trap[trapnr] == 0) {
			continue;
		}
		char tmp[255];
		sprintf(tmp, "%6u:    %10lu", trapnr, s.trap[trapnr]);
		std::cout << "    " << tmp << ISSSTATS_STAT_RATE_ONLY(s.trap[trapnr], s.trap_sum);
	}

#ifdef ISS_STATS_OUTPUT_OPID_STATS_ENABLED
	std::cout << " op_sum:                  " << s.op_sum << "\n";
	for (unsigned int opId = 0; opId < Operation::OpId::NUMBER_OF_OPERATIONS; opId++) {
		if (s.op[opId] == 0) {
			continue;
		}
		char tmp[255];
		sprintf(tmp, "%6u   %-30s    :    %10lu", opId, Operation::opIdStr.at(opId), s.op[opId]);
		std::cout << "    " << tmp << ISSSTATS_STAT_RATE_ONLY(s.op[opId], s.op_sum);
	}
#endif /* ISS_STATS_OUTPUT_OPID_STATS_ENABLED */

	std::cout << "============================================================================================="
	             "==============================\n";

	std::cout << std::endl;
}
