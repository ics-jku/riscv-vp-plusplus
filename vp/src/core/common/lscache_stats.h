/*
 * Copyright (C) 2024-2025 Manfred Schlaegl <manfred.schlaegl@gmx.at>
 * see lscache.h
 */

#ifndef RISCV_ISA_LSCACHE_STATS_H
#define RISCV_ISA_LSCACHE_STATS_H

/*
 * enable/disabled raw csv output of all stats (disabled by default)
 */
// #define LSCACHE_STATS_OUTPUT_CSV_ENABLED
#undef LSCACHE_STATS_OUTPUT_CSV_ENABLED

#include <climits>
#include <cstdint>
#include <cstring>
#include <iostream>

/*
 * dummy implementation
 * = interface and high efficient (all calls optimized out)
 */
template <typename T_LSCache>
class LSCacheStatsDummy_T {
	friend T_LSCache;

   protected:
	const T_LSCache &lscache;

	LSCacheStatsDummy_T(T_LSCache &lscache) : lscache(lscache) {}
	void reset() {}
	void inc_cnt() {}
	void inc_flushs() {}
	void inc_loads() {}
	void inc_stores() {}
	void inc_bus_locked() {}
	void inc_no_dmi() {}
	void inc_dmi() {}
	void inc_hit_load() {}
	void inc_hit_store() {}
	void print() {}
};

template <typename T_LSCache>
class LSCacheStats_T : public LSCacheStatsDummy_T<T_LSCache> {
	friend T_LSCache;

   protected:
	/* must be used for all entries in struct below (see csv print) */
	using selem_t = uint64_t;
	/* use struct to simplifiy reset */
	struct {
		selem_t cnt;
		selem_t flushs;
		selem_t loads;
		selem_t stores;
		selem_t bus_locked;
		selem_t no_dmi;
		selem_t dmi;
		selem_t hit;
		selem_t hit_load;
		selem_t hit_store;
	} s;

	LSCacheStats_T(T_LSCache &lscache) : LSCacheStatsDummy_T<T_LSCache>(lscache) {
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
		if ((s.cnt & 0x3ffffff) == 0) {
			print();
		}
	}
	void inc_flushs() {
		s.flushs++;
	}
	void inc_loads() {
		inc_cnt();
		s.loads++;
	}
	void inc_stores() {
		inc_cnt();
		s.stores++;
	}
	void inc_bus_locked() {
		s.bus_locked++;
	}
	void inc_no_dmi() {
		s.no_dmi++;
	}
	void inc_dmi() {
		s.dmi++;
	}
	void inc_hit_load() {
		s.hit++;
		s.hit_load++;
	}
	void inc_hit_store() {
		s.hit++;
		s.hit_store++;
	}

   public:
#define LSCACHE_STAT_RATE(_val, _cnt) (_val) << "\t\t(" << (double)(_val) / (_cnt) << ")\n"
	void print() {
		std::cout << "============================================================================================="
		             "==============================\n";
		std::cout << "LSCache Stats (hartId: " << this->lscache.hartId << "):\n" << std::dec;
		std::cout << " flushs:                    " << s.flushs << "\n";
		std::cout << " loadstores:                " << s.cnt << "\n";
		std::cout << " loads:                     " << LSCACHE_STAT_RATE(s.loads, s.cnt);
		std::cout << " stores:                    " << LSCACHE_STAT_RATE(s.stores, s.cnt);
		std::cout << " bus_locked:                " << LSCACHE_STAT_RATE(s.bus_locked, s.cnt);
		std::cout << " no_dmi:                    " << LSCACHE_STAT_RATE(s.no_dmi, s.cnt);
		std::cout << " dmi:                       " << LSCACHE_STAT_RATE(s.dmi, s.cnt);
		std::cout << " hit_load:                  " << LSCACHE_STAT_RATE(s.hit_load, s.loads);
		std::cout << " hit_store:                 " << LSCACHE_STAT_RATE(s.hit_store, s.stores);
		std::cout << " hit:                       " << LSCACHE_STAT_RATE(s.hit, s.cnt);

#ifdef LSCACHE_STATS_OUTPUT_CSV_ENABLED
		/* raw output (csv for machine interpreters) */
		printf("\nRAWCSV;LSCACHE_STATS;%lu;", this->lscache.hartId);
		selem_t *raw_s = (selem_t *)&s;
		for (unsigned int i = 0; i < sizeof(s) / sizeof(selem_t); i++) {
			printf("%lu;", raw_s[i]);
		}
		printf("\n");
#endif /* LSCACHE_STATS_OUTPUT_CSV_ENABLED */

		std::cout << "============================================================================================="
		             "==============================\n";

		std::cout << std::endl;
	}
#undef LSCACHE_STAT_RATE
};

#endif /* RISCV_ISA_LSCACHE_STATS_H */
