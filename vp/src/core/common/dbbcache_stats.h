/*
 * Copyright (C) 2024 Manfred Schlaegl <manfred.schlaegl@gmx.at>
 * see dbbcache.h
 */

#ifndef RISCV_ISA_DBBCACHE_STATS_H
#define RISCV_ISA_DBBCACHE_STATS_H

#include <climits>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "util/histogram.h"

/*
 * dummy implementation
 * = interface and high efficient (all calls optimized out)
 */
template <class T_DBBCache, unsigned int T_JUMPDYNLINKCACHE_SIZE>
class DBBCacheStatsDummy_T {
	friend T_DBBCache;

   protected:
	T_DBBCache &dbbcache;

	DBBCacheStatsDummy_T(T_DBBCache &dbbcache) : dbbcache(dbbcache) {}
	/* dec methods are used for aborts */
	void reset() {}
	void inc_cnt() {}
	void dec_cnt() {}
	void inc_cache_ignored_instr() {}
	void inc_fetches() {}
	void inc_fetch_exceptions() {}
	void inc_decodes() {}
	void inc_coherence_updates() {}
	void inc_refetches() {}
	void inc_refetch_exceptions() {}
	void inc_redecodes() {}
	void inc_blocks() {}
	void inc_map_search() {}
	void inc_map_found() {}
	void inc_branches_not_taken() {}
	void inc_branch_list_full() {}
	void inc_branches_taken() {}
	void inc_sjumps() {}
	void inc_branch_sjump_fast_hits() {}
	void inc_branch_sjump_slow_hits() {}
	void inc_djumps() {}
	void inc_djump_hits() {}
	void inc_trap_enters() {}
	void inc_trap_enter_hits() {}
	void inc_trap_rets() {}
	void inc_swtch_same_fast() {}
	void inc_swtch_same_slow() {}
	void inc_swtch_other() {}
	void inc_hit() {}
	void dec_hit() {}
	void inc_fast_hit() {}
	void dec_fast_hit() {}
	void inc_fast_abort() {}
	void inc_med_hit() {}
	void inc_slow_hit() {}
	void inc_err_invalid_pc() {}
	void print() {}
};

template <class T_DBBCache, unsigned int T_JUMPDYNLINKCACHE_SIZE>
class DBBCacheStats_T : public DBBCacheStatsDummy_T<T_DBBCache, T_JUMPDYNLINKCACHE_SIZE> {
	friend T_DBBCache;

   protected:
	struct {
		uint64_t cnt;
		uint64_t cache_ignored_instr;

		uint64_t fetches;
		uint64_t decodes;
		uint64_t fetch_exceptions;

		uint64_t coherence_updates;
		uint64_t refetches;
		uint64_t refetch_exceptions;
		uint64_t redecodes;

		uint64_t blocks;

		uint64_t map_search;
		uint64_t map_found;

		uint64_t branches;
		uint64_t branches_not_taken;
		uint64_t branches_taken;
		uint64_t sjumps;
		uint64_t branch_sjump;
		uint64_t branch_sjump_fast_hits;
		uint64_t branch_sjump_slow_hits;
		uint64_t branch_sjump_hits;
		uint64_t djumps;
		uint64_t djump_hits;
		uint64_t trap_enters;
		uint64_t trap_enter_hits;
		uint64_t trap_rets;
		uint64_t swtch;
		uint64_t swtch_same;
		uint64_t swtch_same_fast;
		uint64_t swtch_same_slow;
		uint64_t swtch_other;

		uint64_t hit;
		uint64_t fast_hit;
		uint64_t fast_abort;
		uint64_t med_hit;
		uint64_t slow_hit;

		uint64_t err_invalid_pc;

		uint64_t stats_cnt;
		uint64_t usedCohMemSum;
		uint64_t overallCohMemSum;
	} s;

	DBBCacheStats_T(T_DBBCache &lscache) : DBBCacheStatsDummy_T<T_DBBCache, T_JUMPDYNLINKCACHE_SIZE>(lscache) {
		reset();
	}

	void reset() {
		memset(&s, 0, sizeof(s));
	}

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
	void inc_cache_ignored_instr() {
		s.cache_ignored_instr++;
	}
	void inc_fetches() {
		s.fetches++;
	}
	void inc_fetch_exceptions() {
		s.fetch_exceptions++;
	}
	void inc_decodes() {
		s.decodes++;
	}
	void inc_coherence_updates() {
		s.coherence_updates++;
	}
	void inc_refetches() {
		s.refetches++;
	}
	void inc_refetch_exceptions() {
		s.refetch_exceptions++;
	}
	void inc_redecodes() {
		s.redecodes++;
	}
	void inc_blocks() {
		s.blocks++;
	}
	void inc_map_search() {
		s.map_search++;
	}
	void inc_map_found() {
		s.map_found++;
	}
	void inc_branches_not_taken() {
		s.branches++;
		s.branches_not_taken++;
	}
	void inc_branches_taken() {
		s.branches++;
		s.branches_taken++;
		s.branch_sjump++;
	}
	void inc_sjumps() {
		s.sjumps++;
		s.branch_sjump++;
	}
	void inc_branch_sjump_fast_hits() {
		s.branch_sjump_hits++;
		s.branch_sjump_fast_hits++;
	}
	void inc_branch_sjump_slow_hits() {
		s.branch_sjump_hits++;
		s.branch_sjump_slow_hits++;
	}
	void inc_djumps() {
		s.djumps++;
	}
	void inc_djump_hits() {
		s.djump_hits++;
	}
	void inc_trap_enters() {
		s.trap_enters++;
	}
	void inc_trap_enter_hits() {
		s.trap_enter_hits++;
	}
	void inc_trap_rets() {
		s.trap_rets++;
	}
	void inc_swtch_same_fast() {
		s.swtch++;
		s.swtch_same++;
		s.swtch_same_fast++;
	}
	void inc_swtch_same_slow() {
		s.swtch++;
		s.swtch_same++;
		s.swtch_same_slow++;
	}
	void inc_swtch_other() {
		s.swtch++;
		s.swtch_other++;
	}
	void inc_hit() {
		s.hit++;
	}
	void dec_hit() {
		s.hit--;
	}
	void inc_fast_hit() {
		inc_hit();
		s.fast_hit++;
	}
	void dec_fast_hit() {
		dec_hit();
		s.fast_hit--;
	}
	void inc_fast_abort() {
		dec_fast_hit();
		s.fast_abort++;
	}
	void inc_med_hit() {
		inc_hit();
		s.med_hit++;
	}
	void inc_slow_hit() {
		inc_hit();
		s.slow_hit++;
	}
	void inc_err_invalid_pc() {
		s.err_invalid_pc++;
	}

   public:
#define DBBCACHE_STAT_RATE(_val, _cnt) (_val) << "\t\t(" << (double)(_val) / (_cnt) << ")\n"
	void print() {
		s.stats_cnt++;

		std::cout << "============================================================================================="
		             "==============================\n";
		std::cout << "DBBCache Stats (hartId: " << this->dbbcache.hartId << "):\n" << std::dec;
		std::cout << " instr:                     " << s.cnt << "\n";
		std::cout << " cache_ignored_instr:       " << DBBCACHE_STAT_RATE(s.cache_ignored_instr, s.cnt);
		std::cout << " fetches:                   " << DBBCACHE_STAT_RATE(s.fetches, s.cnt);
		std::cout << "  fetch_exceptions:         " << DBBCACHE_STAT_RATE(s.fetch_exceptions, s.fetches);
		std::cout << " decodes:                   " << DBBCACHE_STAT_RATE(s.decodes, s.cnt);
		std::cout << " coherence_updates:         " << DBBCACHE_STAT_RATE(s.coherence_updates, s.cnt);
		std::cout << " coherence_cnt:             " << this->dbbcache.coherence_cnt << "\n";
		std::cout << " refetches:                 " << DBBCACHE_STAT_RATE(s.refetches, s.cnt);
		std::cout << "  refetch_exceptions:       " << DBBCACHE_STAT_RATE(s.refetch_exceptions, s.refetches);
		std::cout << " redecodes:                 " << DBBCACHE_STAT_RATE(s.redecodes, s.cnt);
		std::cout << " blocks:                    " << s.blocks << "\n";
		std::cout << " map_search:                " << DBBCACHE_STAT_RATE(s.map_search, s.cnt);
		std::cout << "  map_found:                " << DBBCACHE_STAT_RATE(s.map_found, s.map_search);
		std::cout << " branches:                  " << DBBCACHE_STAT_RATE(s.branches, s.cnt);
		std::cout << " branches_not_taken:        " << DBBCACHE_STAT_RATE(s.branches_not_taken, s.cnt);
		std::cout << " branches_taken:            " << DBBCACHE_STAT_RATE(s.branches_taken, s.cnt);
		std::cout << " sjumps:                    " << DBBCACHE_STAT_RATE(s.sjumps, s.cnt);
		std::cout << "  branch_sjump_fast_hits:   " << DBBCACHE_STAT_RATE(s.branch_sjump_fast_hits, s.branch_sjump);
		std::cout << "  branch_sjump_slow_hits:   " << DBBCACHE_STAT_RATE(s.branch_sjump_slow_hits, s.branch_sjump);
		std::cout << " branch_sjump_hits:         " << DBBCACHE_STAT_RATE(s.branch_sjump_hits, s.branch_sjump);
		std::cout << " djumps:                    " << DBBCACHE_STAT_RATE(s.djumps, s.cnt);
		std::cout << "  djump_hits:               " << DBBCACHE_STAT_RATE(s.djump_hits, s.djumps);
		std::cout << " trap_enters:               " << DBBCACHE_STAT_RATE(s.trap_enters, s.cnt);
		std::cout << "  trap_enter_hits:          " << DBBCACHE_STAT_RATE(s.trap_enter_hits, s.trap_enters);
		std::cout << " trap_rets:                 " << DBBCACHE_STAT_RATE(s.trap_rets, s.cnt);
		std::cout << " swtch:                     " << DBBCACHE_STAT_RATE(s.swtch, s.cnt);
		std::cout << "  swtch_same:               " << DBBCACHE_STAT_RATE(s.swtch_same, s.swtch);
		std::cout << "   swtch_same_fast:         " << DBBCACHE_STAT_RATE(s.swtch_same_fast, s.swtch_same);
		std::cout << "   swtch_same_slow:         " << DBBCACHE_STAT_RATE(s.swtch_same_slow, s.swtch_same);
		std::cout << "  swtch_other:              " << DBBCACHE_STAT_RATE(s.swtch_other, s.swtch);
		std::cout << " hit:                       " << DBBCACHE_STAT_RATE(s.hit, s.cnt);
		std::cout << "  fast_hit:                 " << DBBCACHE_STAT_RATE(s.fast_hit, s.hit);
		std::cout << "  med_hit:                  " << DBBCACHE_STAT_RATE(s.med_hit, s.hit);
		std::cout << "  slow_hit:                 " << DBBCACHE_STAT_RATE(s.slow_hit, s.hit);
		std::cout << " fast_abort:                " << DBBCACHE_STAT_RATE(s.fast_abort, s.cnt);
		std::cout << " miss:                      " << DBBCACHE_STAT_RATE(s.cnt - s.hit, s.cnt);
		std::cout << " err invalid pc:            " << s.err_invalid_pc << "\n";

		auto blkAllocLenHist = Histogram_T<0, 10000>("BlockAllocLen");
		auto blkLenHist = Histogram_T<0, 10000>("BlockLen");
		auto jumpDynLinkCacheHist = Histogram_T<1, T_JUMPDYNLINKCACHE_SIZE>("JumpDynLinkCache");
		auto branchLinkListHist = Histogram_T<1, 10000>("branchLinkList");
		uint64_t n_blocks = 0;
		uint64_t n_coherent_blocks = 0;
		uint64_t n_alloc_entries = 0;
		uint64_t n_coherent_alloc_entries = 0;
		uint64_t n_entries = 0;
		uint64_t n_coherent_entries = 0;
		for (const auto &it : this->dbbcache.blockmap) {
			n_blocks++;
			n_alloc_entries += it.second->alloc_len;
			n_entries += it.second->len;
			if (it.second->coherence_cnt == this->dbbcache.coherence_cnt) {
				n_coherent_blocks++;
				n_coherent_alloc_entries += it.second->alloc_len;
				n_coherent_entries += it.second->len;
			}
			blkAllocLenHist.iteration(it.second->alloc_len);
			blkLenHist.iteration(it.second->len);
			jumpDynLinkCacheHist.iteration(it.second->jumpDynLinkCache.n_dirty());
			branchLinkListHist.iteration(it.second->n_links_dirty());
		}
		blkAllocLenHist.print(true);
		blkLenHist.print(true);
		jumpDynLinkCacheHist.print(true);
		branchLinkListHist.print(true);

		std::cout << " trapCacheUse:        " << this->dbbcache.trapLinkCache.n_dirty() << "/"
		          << this->dbbcache.trapLinkCache.size() << "\n";

		uint64_t usedCohMem =
		    n_coherent_blocks * sizeof(class T_DBBCache::Block) + n_coherent_entries * sizeof(class T_DBBCache::Entry);
		std::cout << " usedCoherentMem:     " << usedCohMem << " bytes\t\t(" << (double)usedCohMem / 1024.0
		          << " KiB)\n";
		uint64_t overallCohMem = n_coherent_blocks * sizeof(class T_DBBCache::Block) +
		                         n_coherent_alloc_entries * sizeof(class T_DBBCache::Entry);
		std::cout << " overallCoherentMem:  " << overallCohMem << " bytes\t\t(" << (double)overallCohMem / 1024.0
		          << " KiB)\n";
		uint64_t wastedCohMem = overallCohMem - usedCohMem;
		std::cout << " wastedCoherentMem:   " << wastedCohMem << " bytes\t\t(" << (double)wastedCohMem / 1024.0
		          << " KiB)\n";
		std::cout << " CohMemOverhead:      " << (double)overallCohMem / usedCohMem << "\n";

		s.usedCohMemSum += usedCohMem;
		uint64_t usedCohMemAvg = s.usedCohMemSum / s.stats_cnt;
		std::cout << " usedCoherentMemAvg:  " << usedCohMemAvg << " bytes\t\t(" << (double)usedCohMemAvg / 1024.0
		          << " KiB)\n";
		s.overallCohMemSum += overallCohMem;
		uint64_t overallCohMemAvg = s.overallCohMemSum / s.stats_cnt;
		std::cout << " overallCohMemAvg:    " << overallCohMemAvg << " bytes\t\t(" << (double)overallCohMemAvg / 1024.0
		          << " KiB)\n";

		uint64_t usedMem = n_blocks * sizeof(class T_DBBCache::Block) + n_entries * sizeof(class T_DBBCache::Entry);
		std::cout << " usedMem:             " << usedMem << " bytes\t\t(" << (double)usedMem / 1024.0 << " KiB)\n";
		uint64_t overallMem =
		    n_blocks * sizeof(class T_DBBCache::Block) + n_alloc_entries * sizeof(class T_DBBCache::Entry);
		std::cout << " overallMem:          " << overallMem << " bytes\t\t(" << (double)overallMem / 1024.0
		          << " KiB)\n";
		uint64_t wastedMem = overallMem - usedMem;
		std::cout << " wastedMem:           " << wastedMem << " bytes\t\t(" << (double)wastedMem / 1024.0 << " KiB)\n";
		std::cout << " memOverhead:         " << (double)overallMem / usedMem << "\n";

		std::cout << "============================================================================================="
		             "==============================\n";

		std::cout << std::endl;
	}
#undef DBBCACHE_STAT_RATE
};

#endif /* RISCV_ISA_DBBCACHE_STATS_H */
