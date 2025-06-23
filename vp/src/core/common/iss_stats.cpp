#include "iss_stats.h"

#include <cstring>
#include <iostream>

void ISSStats::reset() {
	memset(&s, 0, sizeof(s));
}

#define ISSSTATS_STAT_RATE(_val, _sum) (_val) << "\t\t(" << (double)(_val) / (_sum) << ")\n"
#define ISSSTATS_STAT_RATE_CNT(_val) ISSSTATS_STAT_RATE(_val, s.cnt)
#define ISSSTATS_STAT_RATE_TRAP_SUM(_val) ISSSTATS_STAT_RATE(_val, s.trap_sum)

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
	std::cout << " trap_sum:                  " << s.trap_sum << "\n";
	for (unsigned int trapnr = 0; trapnr <= TRAPNR_MAX; trapnr++) {
		if (s.trap[trapnr] == 0) {
			continue;
		}
		std::cout << "    " << trapnr << "            " << ISSSTATS_STAT_RATE_TRAP_SUM(s.trap[trapnr]);
	}

	std::cout << "============================================================================================="
	             "==============================\n";

	std::cout << std::endl;
}
