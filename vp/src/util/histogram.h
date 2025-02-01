/*
 * Copyright (C) 2024 Manfred Schlaegl <manfred.schlaegl@gmx.at>
 *
 * Simple histogram for statistics used for example in vp/src/core/common/dbbcache_stats.h
 */

#ifndef RISCV_UTIL_HISTOGRAM_H
#define RISCV_UTIL_HISTOGRAM_H

#include <climits>
#include <cstring>
#include <iostream>

/* ignore values below MIN_VALUE and above MAX_VALUE */
/* TODO: MAKE MORE EFFICIENT -> USE BINS INSTEAD OF ARRAY!!! */
template <unsigned int MIN_VALUE, unsigned int MAX_VALUE>
class Histogram_T {
   public:
	const static unsigned int SIZE = MAX_VALUE - MIN_VALUE + 1;
	const char *name;
	unsigned int n;
	unsigned int hist[SIZE];
	unsigned int min;
	unsigned int max;
	unsigned long sum;

	Histogram_T(const char *name) : name(name) {
		reset();
	}

	void reset() {
		n = 0;
		memset(hist, 0, sizeof(hist));
		min = UINT_MAX;
		max = 0;
		sum = 0;
	}

	void iteration(unsigned int value) {
		if (value < MIN_VALUE || value > MAX_VALUE) {
			return;
		}
		n++;
		hist[value - MIN_VALUE]++;
		min = value < min ? value : min;
		max = value > max ? value : max;
		sum += value;
	}

	void print(bool enPercentAcc) {
		double avg = (double)sum / n;
		std::cout << " Histogram(" << name << "):\n";
		double percentAcc = 0.0;
		unsigned int nsamples = 0;
		for (unsigned int i = 0; i < SIZE; i++) {
			/* ignore zero entries */
			if (hist[i] == 0) {
				continue;
			}
			double percent = 100.0 * hist[i] / n;
			percentAcc += percent;
			std::cout << "  " << i + MIN_VALUE << ": " << hist[i] << "\t\t" << percent << "%";
			if (enPercentAcc) {
				std::cout << "\t" << percentAcc << "%";
				nsamples += hist[i];
			}
			std::cout << "\n";
		}
		std::cout << " Stats(" << name << "): " << n << ", min/max/avg: " << min << "/" << max << "/" << avg
		          << std::endl;
	}
};

#endif /* RISCV_UTIL_HISTOGRAM_H */
