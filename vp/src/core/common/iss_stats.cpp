#include "iss_stats.h"

#include <cstring>
#include <iostream>

void ISSStats::reset() {
	memset(&s, 0, sizeof(s));
}

#define ISSSTATS_STAT_RATE(_val) (_val) << "\t\t(" << (double)(_val) / s.cnt << ")\n"
void ISSStats::print() {
	std::cout << "============================================================================================="
	             "==============================\n";
	std::cout << "ISS Stats (hartId: " << this->hartId << "):\n" << std::dec;
	std::cout << " instr:                     " << s.cnt << "\n";
	std::cout << " fast_fdd:                  " << ISSSTATS_STAT_RATE(s.fast_fdd);
	std::cout << " fast_fdd_abort:            " << ISSSTATS_STAT_RATE(s.fast_fdd_abort);
	std::cout << " med_fdd:                   " << ISSSTATS_STAT_RATE(s.med_fdd);
	std::cout << " slow_fdd:                  " << ISSSTATS_STAT_RATE(s.slow_fdd);
	std::cout << " lr_sc:                     " << ISSSTATS_STAT_RATE(s.lr_sc);
	std::cout << " commit_instructions:       " << ISSSTATS_STAT_RATE(s.commit_instructions);
	std::cout << " commit_cycles:             " << ISSSTATS_STAT_RATE(s.commit_cycles);
	std::cout << " qk_need_sync:              " << ISSSTATS_STAT_RATE(s.qk_need_sync);
	std::cout << " qk_sync:                   " << ISSSTATS_STAT_RATE(s.qk_sync);
	std::cout << " NOP:                       " << ISSSTATS_STAT_RATE(s.nops);
	std::cout << " jal:                       " << ISSSTATS_STAT_RATE(s.jal);
	std::cout << " j:                         " << ISSSTATS_STAT_RATE(s.j);
	std::cout << " jalr:                      " << ISSSTATS_STAT_RATE(s.jalr);
	std::cout << " jr:                        " << ISSSTATS_STAT_RATE(s.jr);
	std::cout << " loadstore:                 " << ISSSTATS_STAT_RATE(s.loadstore);
	std::cout << " CSR:                       " << ISSSTATS_STAT_RATE(s.csr);
	std::cout << " AMO:                       " << ISSSTATS_STAT_RATE(s.amo);
	std::cout << " set_zero:                  " << ISSSTATS_STAT_RATE(s.set_zero);

	std::cout << "============================================================================================="
	             "==============================\n";

	std::cout << std::endl;
}
