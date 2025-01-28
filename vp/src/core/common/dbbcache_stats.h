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
	void inc_ufast_hit() {}
	void dec_ufast_hit() {}
	void inc_ufast_abort() {}
	void inc_fast_hit() {}
	void inc_slow_hit() {}
	void inc_err_invalid_pc() {}
	void print() {}
};

template <class T_DBBCache, unsigned int T_JUMPDYNLINKCACHE_SIZE>
class DBBCacheStats_T : public DBBCacheStatsDummy_T<T_DBBCache, T_JUMPDYNLINKCACHE_SIZE> {
	friend T_DBBCache;

   protected:
	struct {
		unsigned long cnt;
		unsigned long cache_ignored_instr;

		unsigned long fetches;
		unsigned long decodes;
		unsigned long fetch_exceptions;

		unsigned long coherence_updates;
		unsigned long refetches;
		unsigned long refetch_exceptions;
		unsigned long redecodes;

		unsigned long blocks;

		unsigned long map_search;
		unsigned long map_found;

		unsigned long branches;
		unsigned long branches_not_taken;
		unsigned long branches_taken;
		unsigned long sjumps;
		unsigned long branch_sjump;
		unsigned long branch_sjump_fast_hits;
		unsigned long branch_sjump_slow_hits;
		unsigned long branch_sjump_hits;
		unsigned long djumps;
		unsigned long djump_hits;
		unsigned long trap_enters;
		unsigned long trap_enter_hits;
		unsigned long trap_rets;
		unsigned long swtch;
		unsigned long swtch_same;
		unsigned long swtch_same_fast;
		unsigned long swtch_same_slow;
		unsigned long swtch_other;

		unsigned long hit;
		unsigned long ufast_hit;
		unsigned long ufast_abort;
		unsigned long fast_hit;
		unsigned long slow_hit;

		unsigned long err_invalid_pc;

		unsigned long stats_cnt;
		unsigned long long usedCohMemSum;
		unsigned long long overallCohMemSum;
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
	void inc_ufast_hit() {
		inc_hit();
		s.ufast_hit++;
	}
	void dec_ufast_hit() {
		dec_hit();
		s.ufast_hit--;
	}
	void inc_ufast_abort() {
		dec_ufast_hit();
		s.ufast_abort++;
	}
	void inc_fast_hit() {
		inc_hit();
		s.fast_hit++;
	}
	void inc_slow_hit() {
		inc_hit();
		s.slow_hit++;
	}
	void inc_err_invalid_pc() {
		s.err_invalid_pc++;
	}

   public:
#define DBBCACHE_STAT_RATE(_val, _cnt) (_val) << "\t\t(" << (float)(_val) / (_cnt) << ")\n"
	void print() {
		s.stats_cnt++;

		// TODO: hartid
		std::cout << "============================================================================================="
		             "==============================\n";
		std::cout << "Instruction Cache13 Stats:\n" << std::dec;
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
		std::cout << " ufast_hit:                 " << DBBCACHE_STAT_RATE(s.ufast_hit, s.cnt);
		std::cout << " ufast_abort:               " << DBBCACHE_STAT_RATE(s.ufast_abort, s.cnt);
		std::cout << " fast_hit:                  " << DBBCACHE_STAT_RATE(s.fast_hit, s.cnt);
		std::cout << " slow_hit:                  " << DBBCACHE_STAT_RATE(s.slow_hit, s.cnt);
		std::cout << " hit:                       " << DBBCACHE_STAT_RATE(s.hit, s.cnt);
		std::cout << " err invalid pc:            " << s.err_invalid_pc << "\n";

		auto blkAllocLenHist = Histogram_T<0, 10000>("BlockAllocLen");
		auto blkLenHist = Histogram_T<0, 10000>("BlockLen");
		auto jumpDynLinkCacheHist = Histogram_T<1, T_JUMPDYNLINKCACHE_SIZE>("JumpDynLinkCache");
		auto branchLinkListHist = Histogram_T<1, 10000>("branchLinkList");
		unsigned int n_blocks = 0;
		unsigned int n_coherent_blocks = 0;
		unsigned int n_alloc_entries = 0;
		unsigned int n_coherent_alloc_entries = 0;
		unsigned int n_entries = 0;
		unsigned int n_coherent_entries = 0;
		for (const auto &it : this->dbbcache.decoderCache) {
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

		unsigned int usedCohMem =
		    n_coherent_blocks * sizeof(class T_DBBCache::Block) + n_coherent_entries * sizeof(class T_DBBCache::Entry);
		std::cout << " usedCoherentMem:     " << usedCohMem << " bytes\t\t(" << (float)usedCohMem / 1024.0 << " KiB)\n";
		unsigned int overallCohMem = n_coherent_blocks * sizeof(class T_DBBCache::Block) +
		                             n_coherent_alloc_entries * sizeof(class T_DBBCache::Entry);
		std::cout << " overallCoherentMem:  " << overallCohMem << " bytes\t\t(" << (float)overallCohMem / 1024.0
		          << " KiB)\n";
		unsigned int wastedCohMem = overallCohMem - usedCohMem;
		std::cout << " wastedCoherentMem:   " << wastedCohMem << " bytes\t\t(" << (float)wastedCohMem / 1024.0
		          << " KiB)\n";
		std::cout << " CohMemOverhead:      " << (float)overallCohMem / usedCohMem << "\n";

		s.usedCohMemSum += usedCohMem;
		unsigned int usedCohMemAvg = s.usedCohMemSum / s.stats_cnt;
		std::cout << " usedCoherentMemAvg:  " << usedCohMemAvg << " bytes\t\t(" << (float)usedCohMemAvg / 1024.0
		          << " KiB)\n";
		s.overallCohMemSum += overallCohMem;
		unsigned int overallCohMemAvg = s.overallCohMemSum / s.stats_cnt;
		std::cout << " overallCohMemAvg:    " << overallCohMemAvg << " bytes\t\t(" << (float)overallCohMemAvg / 1024.0
		          << " KiB)\n";

		unsigned int usedMem = n_blocks * sizeof(class T_DBBCache::Block) + n_entries * sizeof(class T_DBBCache::Entry);
		std::cout << " usedMem:             " << usedMem << " bytes\t\t(" << (float)usedMem / 1024.0 << " KiB)\n";
		unsigned int overallMem =
		    n_blocks * sizeof(class T_DBBCache::Block) + n_alloc_entries * sizeof(class T_DBBCache::Entry);
		std::cout << " overallMem:          " << overallMem << " bytes\t\t(" << (float)overallMem / 1024.0 << " KiB)\n";
		unsigned int wastedMem = overallMem - usedMem;
		std::cout << " wastedMem:           " << wastedMem << " bytes\t\t(" << (float)wastedMem / 1024.0 << " KiB)\n";
		std::cout << " memOverhead:         " << (float)overallMem / usedMem << "\n";

		std::cout << "============================================================================================="
		             "==============================\n";

		std::cout << std::endl;
	}
#undef DBBCACHE_STAT_RATE
};

#endif /* RISCV_ISA_DBBCACHE_STATS_H */
