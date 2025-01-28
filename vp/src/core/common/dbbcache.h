/*
 * Copyright (C) 2024 Manfred Schlaegl <manfred.schlaegl@gmx.at>
 *
 * Dynamic Basic Block Cache
 * Generates an alternative representation of the executed code, the Dynamic Basic Block Graph (DBBG), to efficiently
 * cache data needed for instruction processing by the ISS to significantly speed-up instruction interpretation.
 *
 * More details on the initial version can be found in the paper
 * "Fast Interpreter-Based Instruction Set Simulation for Virtual Prototypes"
 * by Manfred Schlaegl and Daniel Grosse
 * presented at the Design, Automation and Test in Europe Conference 2025
 *
 * DBBCache provides the basis for following additional ISS optimizations
 * (see rv32/rv64 ISS implementations).
 * These optimizations are:
 *  * Computed goto: The op switch is replaced with jump labels for cases.
 *    The DBBCache caches the jump label pointers which are then used
 *    to directly dispatch (jump to) the corresponding operation
 *    implementation.
 *  * Threaded code: For the fast path (see below) the fetch/decode
 *    (from DBBCache) and dispatch of the next operation is inlined
 *    after each operation implementation.
 *    -> After each operation, the ISS can directly jump to the next.
 *  * Fast/Medium/Slow-Path
 *    * The Fast path uses computed gotos and threaded code as described
 *      above (it does not check for any flags or events).
 *      As long as there are no cache misses or ISS events (see below)
 *      the execution can stay in this path.
 *    * The Medium path is entered, when the DBBCache leaves its fast path,
 *      which is either because of a cache miss or, because of a request by
 *      the ISS. The medium path checks if the ISS slow path flag is
 *      set. If so, the ISS enters the slow path (see below).
 *      After that, a full DBBCache fetch/decode is called to handle the
 *      cache miss, and the next operation is dispatched (jump).
 *    * The slow path is entered if DBBCache is in its slow path and
 *      the ISS slow path flag is set. (i.e. to force the slow path, the
 *      ISS has to set its flag and must also force the DBBCache to its
 *      slow path). The slow path is for example set on interrupt events,
 *      changes of interrupt/trap related csrs, tlm quantum sync requests
 *      (see below), handling of atomic instructions, enabled debug/trace
 *      mode, ...
 *      After handling these events, a full DBBCache fetch/decode is called
 *      and the next operation is dispatched (jump).
 *  * Lazy/Approximate tlm quantum checks:
 *    * an estimate of the consumed tlm quantum cycles is tracked
 *      using the instruction count (reduces quantum keeper update calls
 *      significantly)
 *    * check and synchronization with the quantum keeper is done only
 *      before systemc context switches or control flow changes based
 *      on the estimate.
 *      (reduces quantum keeper check and sync calls significantly)
 *  * Executed cycles and PC calculated on demand: The DBBCache implicitly
 *    keeps track of the current PC and the number executed cycles. The ISS can
 *    request generation of these values on demand (e.g. PC for trap,
 *    cycles see below)
 *  * On demand performance counters (minstret and mcycles CSRs) are
 *    created on demand. Number of executed instructions is tracked by the
 *    ISS (->minstret), number of executed cycles is tracked by DBBCache
 *    (see above) (->mcycles)
 *  * Target PC addresses for taken branche and static jumps are handled
 *    by DBBCache. They are calculated once and cached on a cache miss.
 *    On cache hits, the values are not calculated but taken from the
 *    cache.
 *
 * TODO:
 *  * !!! fix dummy implementation DBBCache_IF_T (to be able to disable
 *    DBBCache at compile time)
 *  * !!! fix DBBCache for RV32
 *  * !! allow enabling/disabling the DBBCache at runtime (or at VP start)
 *     * similar to dmi
 *     * ideally without performance impact
 *  * ! cleanup DBBCache
 *  * improve performance: rework the re-fetch to bypass the memory interface,
 *    if dmi is available (see lscache).
 */

#ifndef RISCV_ISA_DBBCACHE_H
#define RISCV_ISA_DBBCACHE_H

#include <climits>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "core_defs.h"
#include "dbbcache_stats.h"
#include "instr.h"
#include "trap.h"
#include "util/common.h"

/******************************************************************************
 * BEGIN: CONFIG
 ******************************************************************************/

#define DBBCACHE_ENABLED
//#undef DBBCACHE_ENABLED
//#define DBBCACHE_STATS_ENABLED
#undef DBBCACHE_STATS_ENABLED

// TODO
#define DBBCACHE_ENABLE_FAST  // CAUTION: DISABLING THIS BREAKS EXECUTION!!!
// #define DBBCACHE_ENABLE_CHECKS /* SLOW! */

/******************************************************************************
 * END: CONFIG
 ******************************************************************************/

/******************************************************************************
 * BEGIN: MISC (TODO)
 ******************************************************************************/

struct OpMapEntry {
	/* order optimized for alignment */
	void *label_ptr;
	Opcode::Mapping op;
	unsigned long instr_time;
};

/******************************************************************************
 * END: MISC (TODO)
 ******************************************************************************/

/******************************************************************************
 * BEGIN: DUMMY/INTERFACE IMPLEMENTATION
 ******************************************************************************/

template <enum Architecture arch, typename T_uxlen_t, typename T_instr_memory_if>
class DBBCache_IF_T {
   protected:
	T_instr_memory_if *instr_mem = nullptr;
	struct OpMapEntry *opMap = nullptr;
	void *ufast_abort_label_ptr = nullptr;
	uint32_t misa_extensions = 0x0;
	uint32_t mem_word = 0x0;

   public:
	DBBCache_IF_T() {
		init(nullptr, nullptr, nullptr, 0, 0);
	}

	void init(T_instr_memory_if *instr_mem, struct OpMapEntry opMap[], void *ufast_abort_label_ptr,
	          uint32_t misa_extensions, T_uxlen_t entrypoint) {
		this->instr_mem = instr_mem;
		this->opMap = opMap;
		this->ufast_abort_label_ptr = ufast_abort_label_ptr;
		this->misa_extensions = misa_extensions;
		this->mem_word = 0;
	}

	void branch_not_taken(T_uxlen_t pc) {}

	void branch_taken(T_uxlen_t pc) {}

	void jump(T_uxlen_t pc) {}

	void jump_dyn(T_uxlen_t pc) {}

	void fence_i(T_uxlen_t pc) {}

	void fence_vma(T_uxlen_t pc) {}

	void enter_trap(T_uxlen_t pc) {}

	void ret_trap(T_uxlen_t pc) {}

	void fetch_decode(T_uxlen_t &pc, Instruction &instr, Opcode::Mapping &op) {
		try {
			this->mem_word = this->instr_mem->load_instr(pc);
			instr = Instruction(this->mem_word);
		} catch (SimulationTrap &e) {
			op = Opcode::UNDEF;
			instr = Instruction(0);
			throw;
		}

		if (instr.is_compressed()) {
			op = instr.decode_and_expand_compressed(arch, this->misa_extensions);
			pc += 2;
		} else {
			op = instr.decode_normal(arch, this->misa_extensions);
			pc += 4;
		}
	}

	uint32_t get_mem_word() {
		return mem_word;
	}
};
template <enum Architecture arch, typename T_uxlen_t, typename T_instr_memory_if>
using DBBCacheDummy_T = DBBCache_IF_T<arch, T_uxlen_t, T_instr_memory_if>;

/******************************************************************************
 * END: DUMMY/INTERFACE IMPLEMENTATION
 ******************************************************************************/

/******************************************************************************
 * BEGIN: FUNCTIONAL IMPLEMENTATION
 ******************************************************************************/

template <enum Architecture arch, typename T_uxlen_t, typename T_instr_memory_if>
class DBBCache_T : public DBBCache_IF_T<arch, T_uxlen_t, T_instr_memory_if> {
	/* Configuration */
	const static unsigned int N_ENTRIES_START = 2;
	const static unsigned int JUMPDYNLINKCACHE_SIZE = 16;
	const static unsigned int BRANCHLINKLIST_SIZE = 16;
	const static unsigned int TRAPLINKCACHE_SIZE = 8;

	/* instructions can 4 or 2 bytes
	 * -> valid pc's are aligned to 2 or 4 bytes.
	 * -> if bit 0 is set to 1, the pc is in-valid
	 */
	static __always_inline bool pc_is_valid(T_uxlen_t pc) {
		return !(pc & 0b1);
	}

	/* all bits set -> last unaligned address in address space */
	const static T_uxlen_t INVALID_PC = ((T_uxlen_t)(0 - 1));

	class Block;

	class Entry {
		/* order optimized for alignment */
	   private:
		void *link;

	   public:
		void *opLabelPtr;
		T_uxlen_t pc;
		uint32_t cycle_counter_raw;
		uint32_t mem_word;
		int32_t instr;

		uint16_t pc_increment;
		uint16_t idx;

		__always_inline void set_terminal(const DBBCache_T &dbbcache) {
			opLabelPtr = dbbcache.ufast_abort_label_ptr;
		}
		__always_inline bool is_terminal(const DBBCache_T &dbbcache) {
			return opLabelPtr == dbbcache.ufast_abort_label_ptr;
		}

		__always_inline void resetLink() {
			link = nullptr;
		}

		__always_inline void setLinkBlock(Block *linkBlock) {
			link = linkBlock;
		}

		__always_inline bool linkValid() {
			return (link != nullptr);
		}

		__always_inline Block *getLinkBlock() {
			return (Block *)link;
		}

		/* consistency check */
		bool check(const DBBCache_T &dbbcache) {
			bool ok = true;
			if (!is_terminal(dbbcache)) {
				if (!pc_is_valid(pc)) {
					std::cerr << " Entry: Invalid pc: " << std::hex << pc << std::dec << std::endl;
					ok = false;
				}

				if (pc_increment != 2 && pc_increment != 4) {
					std::cerr << " Entry: Invalid pc_increment: " << pc_increment << std::endl;
					ok = false;
				}
			}
			if (!ok) {
				dump();
			}
			return ok;
		}

		void dump(const DBBCache_T &dbbcache) {
			std::cout << "    "
			          << "addr: " << std::hex << this << std::dec << ", idx: " << idx << ", is_terminal: ";
			if (is_terminal(dbbcache)) {
				std::cout << "true";
			} else {
				std::cout << "false" << std::hex << ", pc: " << pc << ", mem_word: " << mem_word << ", instr: " << instr
				          << ", opLabelPtr: " << opLabelPtr << ", link: " << link << std::dec
				          << ", pc_increment: " << pc_increment << ", cycle_counter_raw: " << cycle_counter_raw;
			}
			std::cout << std::endl;
		}
	};

	template <unsigned int SIZE>
	class BlockLinkList_T {
	   protected:
		T_uxlen_t pc[SIZE];
		Block *block[SIZE];

	   public:
		void BlockLinkList() {
			reset();
		}

		static unsigned int size() {
			return SIZE;
		}

		void reset() {
			/* invalidate pc (see INVALID_PC) */
			memset(pc, 0xff, sizeof(pc));
		}

		void set(unsigned int idx, T_uxlen_t pc, Block *block) {
			this->pc[idx] = pc;
			this->block[idx] = block;
		}

		struct Block *get(unsigned int idx, T_uxlen_t pc) {
			if (this->pc[idx] == pc) {
				return block[idx];
			} else {
				return nullptr;
			}
		}

		__always_inline struct Block *find(T_uxlen_t pc) {
			for (unsigned int idx = 0; idx < SIZE; idx++) {
				if (this->pc[idx] == pc) {
					return block[idx];
				}
			}
			return nullptr;
		}

		unsigned int n_dirty() {
			unsigned int dirty = 0;
			for (unsigned int idx = 0; idx < SIZE; idx++) {
				if (pc_is_valid(pc[idx])) {
					dirty++;
				}
			}
			return dirty;
		}

		void dump() {
			std::cout << "Links:\n";
			for (unsigned int idx = 0; idx < SIZE; idx++) {
				std::cout << std::dec << " " << idx << ": pc: ";
				if (pc_is_valid(pc[idx])) {
					std::cout << std::hex << pc[idx] << ", block: " << block[idx];
				} else {
					std::cout << "invalid";
				}
				std::cout << std::endl;
			}
			std::cout << std::dec;
		}
	};

	template <unsigned int SIZE>
	class BlockLinkCache_T : public BlockLinkList_T<SIZE> {
	   protected:
		unsigned int curIdx;

	   public:
		void BlockLinkCache() {
			reset();
		};

		void reset() {
			BlockLinkList_T<SIZE>::reset();
			curIdx = 0;
		}

		void add(T_uxlen_t pc, struct Block *block) {
			this->set(curIdx, pc, block);
			curIdx++;
			if (curIdx == SIZE) {
				curIdx = 0;
			}
		}

		void dump() {
			BlockLinkList_T<SIZE>::dump();
			std::cout << "curIdx: " << curIdx << std::endl;
		}
	};

	class Block {
	   public:
		T_uxlen_t start_addr;
		uint32_t alloc_len;
		uint32_t len;
		struct Entry *entries;

		/* cache links of dynamic jump addresses (jalr) */
		BlockLinkCache_T<JUMPDYNLINKCACHE_SIZE> jumpDynLinkCache;

		uint32_t coherence_cnt;

		Block() : Block(0) {}
		Block(T_uxlen_t pc, const DBBCache_T &dbbcache) {
			init(pc, dbbcache);
		}

		void init(T_uxlen_t pc, const DBBCache_T &dbbcache) {
			alloc_len = N_ENTRIES_START;
			entries = (class Entry *)malloc(alloc_len * sizeof(*entries));
			entries[0].idx = 0;
			entries[0].pc = pc;
			/* counter of first entry is always 0, because the next entry holds the value after execution */
			entries[0].cycle_counter_raw = 0;
			entries[0].set_terminal(dbbcache);
			start_addr = pc;
			len = 0;
			this->coherence_cnt = coherence_cnt;
			invalidate_links();
		}

		void invalidate_links() {
			jumpDynLinkCache.reset();
			for (unsigned int i = 0; i < len; i++) {
				entries[i].resetLink();
			}
		}

		unsigned n_links_dirty() {
			unsigned int ret = 0;
			for (unsigned int i = 0; i < len; i++) {
				if (entries[i].linkValid()) {
					ret++;
				}
			}
			return ret;
		}

		/* consistency check */
		bool check(const DBBCache_T &dbbcache) {
			bool ok = true;
			for (unsigned int idx = 0; idx < len; idx++) {
				if (entries[idx].idx != idx) {
					std::cerr << "Block: Index error at " << idx << std::endl;
					ok = false;
				}
				ok = entries[idx].check(dbbcache);
			}
			if (!entries[len].is_terminal(dbbcache)) {
				std::cerr << "Missing terminal in block" << std::endl;
				ok = false;
			}
			if (!ok) {
				dump();
			}
			return ok;
		}

		void dump(const DBBCache_T &dbbcache) {
			std::cout << "Block: start_addr: " << std::hex << start_addr << std::dec << ", alloc_len: " << alloc_len
			          << ", len: " << len << ", coherence_cnt: " << this->coherence_cnt << ", links:"
			          << "\n"
			          << std::hex;
			/* dump with terminal (+1) */
			for (unsigned int i = 0; i < len + 1; i++) {
				entries[i].dump(dbbcache);
			}
			std::cout << std::dec << std::endl;
			std::cout << "dynLinkCache\n";
			jumpDynLinkCache.dump();
			std::cout << std::dec << std::endl;
		}
	};

   protected:
#ifdef DBBCACHE_STATS_ENABLED
	using dbbcachestats_t = DBBCacheStats_T<DBBCache_T, JUMPDYNLINKCACHE_SIZE>;
#else
	using dbbcachestats_t = DBBCacheStatsDummy_T<DBBCache_T, JUMPDYNLINKCACHE_SIZE>;
#endif
	friend dbbcachestats_t;

	dbbcachestats_t stats = dbbcachestats_t(*this);

   private:
	std::unordered_map<uint64_t, struct Block *> decoderCache;
	struct Block *curBlock;
	struct Block dummyBlock = Block(0, *this);
	int32_t curEntryIdx = 0;

	bool slow_path = false;
	struct Block ufastDisableBlock = Block(0, *this);
	struct Entry *ufastEntry;

	bool exception = false;

	BlockLinkCache_T<TRAPLINKCACHE_SIZE> trapLinkCache;

	uint32_t coherence_cnt = 0;

	uint64_t cycle_counter_raw = 0;

	/* consistency check */
	bool check_ufastEntry() {
		if (!in_ufast_path()) {
			/* ok */
			return true;
		}

		/*
		 * This function can be called in any state:
		 *  * After fetch_decode and before any callback -> ufastEntry points to current entry
		 *  * After fetch_decode and a callback -> ufastEntry might point to its block start value
		 * (curBlock->entries[-1])! We have to consider both cases!
		 */
		struct Entry *entry;
		if (ufastEntry == &curBlock->entries[-1]) {
			/* ufastEntry points has initial value -> set initial curEntryIdx*/
			entry = ufastEntry + 1;
		} else {
			entry = ufastEntry;
		}
		if (entry != &curBlock->entries[entry->idx]) {
			std::cerr << "ufastEntry does not match curBlock->entries[ufastEntry->idx]!"
			          << "\n";
			ufastEntry->dump();
			curBlock->dump();
			return false;
		}
		/* ok */
		return true;
	}

	__always_inline uint32_t fetch_raw(T_uxlen_t &pc) {
		stats.inc_fetches();
		return this->instr_mem->load_instr(pc);
	}

	__always_inline uint32_t fetch(T_uxlen_t &pc, Instruction &instr) {
		try {
			exception = false;
			uint32_t mem_word = fetch_raw(pc);
			instr = Instruction(mem_word);
			return mem_word;
		} catch (SimulationTrap &e) {
			stats.inc_fetch_exceptions();
			instr = Instruction(0);
			/* safe pc for get_last_pc_exception_safe */
			dummyBlock.entries[0].pc = pc;
			exception = true;
			throw;
		}
	}

	__always_inline int decode(Instruction &instr, Opcode::Mapping &op) {
		stats.inc_decodes();
		if (instr.is_compressed()) {
			op = instr.decode_and_expand_compressed(arch, this->misa_extensions);
			return 2;
		} else {
			op = instr.decode_normal(arch, this->misa_extensions);
			return 4;
		}
	}

	__always_inline uint32_t fetch_decode(T_uxlen_t &pc, Instruction &instr, Opcode::Mapping &op) {
		uint32_t mem_word = fetch(pc, instr);
		pc += decode(instr, op);
		return mem_word;
	}

	__always_inline void decode_update_entry(struct Entry *entry, T_uxlen_t &pc, Instruction &instr) {
		Opcode::Mapping op;
		entry->mem_word = instr.data();
		entry->pc = pc;
		entry->pc_increment = decode(instr, op);
		pc += entry->pc_increment;
		entry->opLabelPtr = this->opMap[op].label_ptr;

		/*
		 * We want to have the cycles AFTER the next decode ->
		 * The next entry must hold the current cycles
		 * update cycle_counter_raw of follow-up entry (there is always a terminal!)
		 */
		(entry + 1)->cycle_counter_raw = entry->cycle_counter_raw + this->opMap[op].instr_time;

		entry->instr = instr.data();
		entry->resetLink();
	}

	__always_inline class Entry *fetch_decode_add_entry(T_uxlen_t &pc, Instruction &instr) {
		unsigned int idx = curBlock->len;

		/* space for entries and terminal (+1) */
		if (idx + 1 == curBlock->alloc_len) {
			curBlock->alloc_len *= 2;
			curBlock->entries =
			    (class Entry *)realloc(curBlock->entries, curBlock->alloc_len * sizeof(*curBlock->entries));
		}

		struct Entry *entry = &curBlock->entries[idx];
		fetch(pc, instr);
		decode_update_entry(entry, pc, instr);
		entry->idx = idx;

		/* set next as terminal */
		(entry + 1)->idx = idx + 1;
		(entry + 1)->set_terminal(*this);

		curBlock->len++;
		return entry;
	}

	__always_inline void switch_block_dummy(T_uxlen_t pc) {
		Entry *lastEntry;
		if (likely(in_ufast_path())) {
			lastEntry = ufastEntry;
		} else {
			lastEntry = &curBlock->entries[curEntryIdx];
		}

		/* update cycles from last block */
		cycle_counter_raw += (lastEntry + 1)->cycle_counter_raw;

		dummyBlock.entries[0].pc = pc;

		/* reset block cycles (will be added up below) */
		dummyBlock.entries[1].cycle_counter_raw = 0;

		ufast_path_raw_disable();
		curBlock = &dummyBlock;
		curEntryIdx = -1;
	}

	__always_inline void switch_block(class Block *block) {
		Entry *lastEntry;
		if (likely(in_ufast_path())) {
			lastEntry = ufastEntry;
		} else {
			lastEntry = &curBlock->entries[curEntryIdx];
		}

		/* update cycles from last block */
		cycle_counter_raw += (lastEntry + 1)->cycle_counter_raw;

		if (curBlock == block) {
			/* we switch to the same block -> we know already that len>0 and that it is coherent -> switch directly */
			if (likely(in_ufast_path() || slow_path == false)) {
				stats.inc_swtch_same_fast();
				ufast_path_raw_enable(&curBlock->entries[-1]);
			} else {
				stats.inc_swtch_same_slow();
				curEntryIdx = -1;
			}
			return;
		}

		/* switch to other block */

		stats.inc_swtch_other();

		curBlock = block;
#ifdef DBBCACHE_ENABLE_FAST
		/*
		 * block has entries and is coherent -> run fast
		 * NOTE:
		 * This kind of coherency check does only work because we are always at the start of a block.
		 * It would not work, if we were in the middle of a block!
		 */
		if (likely(slow_path == false && curBlock->len > 0 && coherence_cnt == curBlock->coherence_cnt)) {
			ufast_path_raw_enable(&curBlock->entries[-1]);
		} else {
			ufast_path_raw_disable();
			curEntryIdx = -1;
			slow_path = false;
		}
#else
		curEntryIdx = -1;
#endif
	}

	__always_inline void find_create_block(T_uxlen_t pc) {
		stats.inc_map_search();
		Block *block = nullptr;
		auto it = decoderCache.find(pc);
		if (it != decoderCache.end()) {
			/* found */
			stats.inc_map_found();
			block = it->second;
		} else {
			/* not found -> new */
			stats.inc_blocks();
			block = new Block(pc, *this);
			decoderCache[pc] = block;
		}
		switch_block(block);
	}

	__always_inline void branch_taken_sjump(int32_t pc_offset) {
		/* taken hit fast */
		if (likely(in_ufast_path())) {
			if (likely(ufastEntry->linkValid())) {
				stats.inc_branch_sjump_fast_hits();
				switch_block(ufastEntry->getLinkBlock());
				return;
			}
			curEntryIdx = ufastEntry->idx;
		}

		/* taken hit slow */
		if ((curBlock != &dummyBlock) && (curBlock->entries[curEntryIdx].linkValid())) {
			stats.inc_branch_sjump_slow_hits();
			switch_block(curBlock->entries[curEntryIdx].getLinkBlock());
			return;
		}

		/* taken miss -> link */

		/* calculate target pc */
		T_uxlen_t pc =
		    curBlock->entries[curEntryIdx].pc + pc_offset;  // also works for dummyBlock (pc in curEntryIdx = 0)
		assert(pc_is_valid(pc) && "not possible due to immediate formats and jump execution");
		if (unlikely((pc & 0x3) && (!(this->misa_extensions & csr_misa::C)))) {
			// NOTE: misaligned instruction address not possible on machines supporting compressed instructions
			raise_trap(EXC_INSTR_ADDR_MISALIGNED, pc);
		}

		Block *lastBlock = curBlock;
		int lastEntryIdx = curEntryIdx;
		find_create_block(pc);
		if (lastBlock != &dummyBlock) {
			lastBlock->entries[lastEntryIdx].setLinkBlock(curBlock);
		}
	}

	__always_inline void coherency_update(T_uxlen_t pc) {
		stats.inc_coherence_updates();

		// TODO: handle counters before overflow!
		coherence_cnt++;

		/* stop fast execution, if enabled */
		if (likely((in_ufast_path()))) {
			/* Restore curEntryIdx from ufastEntry */
			curEntryIdx = ufastEntry->idx;
			/* stop */
			ufast_path_raw_disable();
		}
	}

	__always_inline void ufast_path_raw_disable() {
		ufastEntry = &ufastDisableBlock.entries[0];
	}

	__always_inline void ufast_path_raw_enable(class Entry *entry) {
		ufastEntry = entry;
	}

   public:
	DBBCache_T() {
		init(nullptr, nullptr, nullptr, false, 0);
	}

	void init(T_instr_memory_if *instr_mem, struct OpMapEntry opMap[], void *ufast_abort_label_ptr,
	          uint32_t misa_extensions, T_uxlen_t entrypoint) {
		DBBCache_IF_T<arch, T_uxlen_t, T_instr_memory_if>::init(instr_mem, opMap, ufast_abort_label_ptr,
		                                                        misa_extensions, entrypoint);

		/* set abort label ptr and reinit blocks to have valid terminal entries */
		this->ufast_abort_label_ptr = ufast_abort_label_ptr;
		dummyBlock.init(0, *this);
		ufastDisableBlock.init(0, *this);

		coherence_cnt = 0;

		for (const auto it : decoderCache) {
			delete it.second;
		}

		decoderCache.clear();
		trapLinkCache.reset();
		exception = false;

		/* use dummyBlock (and its first entry) as valid predecessor */
		curBlock = &dummyBlock;
		curEntryIdx = 0;

		/* use disable block entries to indicate disabled ufast path */
		slow_path = false;
		ufastDisableBlock.entries[1].set_terminal(*this);
		ufast_path_raw_disable();

		find_create_block(entrypoint);
	}

	__always_inline void branch_not_taken(T_uxlen_t pc) {
		stats.inc_branches_not_taken();
	}

	__always_inline void branch_taken(int32_t pc_offset) {
		stats.inc_branches_taken();
		branch_taken_sjump(pc_offset);
	}

	__always_inline void jump(int32_t pc_offset) {
		stats.inc_sjumps();
		branch_taken_sjump(pc_offset);
	}

	__always_inline T_uxlen_t jump_and_link(int32_t pc_offset) {
		stats.inc_sjumps();

		Entry *e;
		if (likely(in_ufast_path())) {
			e = ufastEntry;
		} else {
			e = &curBlock->entries[curEntryIdx];
		}

		T_uxlen_t link = e->pc + e->pc_increment;

		branch_taken_sjump(pc_offset);

		return link;
	}

	__always_inline void jump_dyn(T_uxlen_t pc) {
		stats.inc_djumps();

		struct Block *linkBlock = curBlock->jumpDynLinkCache.find(pc);
		if (likely(linkBlock != nullptr)) {
			stats.inc_djump_hits();
			switch_block(linkBlock);
			return;
		}

		Block *lastBlock = curBlock;
		find_create_block(pc);
		lastBlock->jumpDynLinkCache.add(pc, curBlock);
	}

	__always_inline T_uxlen_t jump_dyn_and_link(T_uxlen_t pc) {
		stats.inc_djumps();

		Entry *e;
		if (likely(in_ufast_path())) {
			e = ufastEntry;
		} else {
			e = &curBlock->entries[curEntryIdx];
		}

		T_uxlen_t link = e->pc + e->pc_increment;

		struct Block *linkBlock = curBlock->jumpDynLinkCache.find(pc);
		if (likely(linkBlock != nullptr)) {
			stats.inc_djump_hits();
			switch_block(linkBlock);
			return link;
		}

		Block *lastBlock = curBlock;
		find_create_block(pc);
		lastBlock->jumpDynLinkCache.add(pc, curBlock);
		return link;
	}

	__always_inline T_uxlen_t get_curBlock_start_addr() {
		return curBlock->start_addr;
	}

	__always_inline void fence_i(T_uxlen_t pc) {
		coherency_update(pc);
	}

	__always_inline void fence_vma(T_uxlen_t pc) {
		coherency_update(pc);
	}

	__always_inline void enter_trap(T_uxlen_t pc) {
		// TODO maybe stack push (curBlock, CurEntryIdx?)
		stats.inc_trap_enters();

		struct Block *linkBlock = trapLinkCache.find(pc);
		if (likely(linkBlock != nullptr)) {
			stats.inc_trap_enter_hits();
			switch_block(linkBlock);
			return;
		}

		find_create_block(pc);
		trapLinkCache.add(pc, curBlock);
	}

	__always_inline void ret_trap(T_uxlen_t pc) {
		stats.inc_trap_rets();
		// TODO maybe stack restore
		switch_block_dummy(pc);
	}

	__always_inline void force_slow_path() {
		/* stop fast execution, if enabled */
		slow_path = true;

		if (unlikely((!in_ufast_path()))) {
			return;
		}

		/*
		 * Restore curEntryIdx from ufastEntry
		 * This function is called by other harts too. It can therefore be called in any state:
		 *  * After fetch_decode and before any callback -> ufastEntry points to current entry
		 *  * After fetch_decode and a callback -> ufastEntry might point to its block start value
		 * (curBlock->entries[-1])! We have to consider both cases!
		 */
		if (unlikely(ufastEntry == &curBlock->entries[-1])) {
			/* ufastEntry points has initial value -> set initial curEntryIdx*/
			curEntryIdx = -1;
		} else {
			/* ufastEntry points to valid entry -> copy idx */
			curEntryIdx = ufastEntry->idx;
		}
		ufast_path_raw_disable();
	}

	__always_inline bool in_ufast_path() {
		if (unlikely(ufastEntry == &ufastDisableBlock.entries[0])) {
			return false;
		}
		return true;
	}

	__always_inline void *fetch_decode_fast(Instruction &instr) {
		stats.inc_cnt();
		stats.inc_ufast_hit();

		ufastEntry++;
		instr = Instruction(ufastEntry->instr);
		return ufastEntry->opLabelPtr;
	}

	__always_inline void abort_fetch_decode_fast() {
		/* revert to state before fetch_decode_fast */
		ufastEntry--;

		stats.dec_cnt();
		stats.inc_ufast_abort();
	}

	__always_inline uint32_t try_fetch_decode_fast_commit() {
		ufastEntry++;
		return ufastEntry->instr;
	}

	__always_inline void *fetch_decode(T_uxlen_t &pc, Instruction &instr) {
		stats.inc_cnt();

#ifdef DBBCACHE_ENABLE_CHECKS
		curBlock->check();
		check_ufastEntry();
#endif

		/* UFAST PATH */
		if (likely(in_ufast_path())) {
			/* next instruction */
			ufastEntry++;

			/* entry valid? */
			if (likely(!ufastEntry->is_terminal(*this))) {
				stats.inc_fast_hit();

				instr = Instruction(ufastEntry->instr);
				pc += ufastEntry->pc_increment;
				return ufastEntry->opLabelPtr;
			}

			/* entry is not valid -> start slow path for this instruction */

			/* index of last valid */
			curEntryIdx = ufastEntry->idx - 1;
			ufast_path_raw_disable();
		}

		/* SLOW PATH */

		/* ignore cache after return from interrupt at random position */
		if (unlikely(curBlock == &dummyBlock)) {
			Opcode::Mapping op = Opcode::UNDEF;
			stats.inc_cache_ignored_instr();
			/* all tracked data is stored in first entry */
			curEntryIdx = 0;
			/* save pc for branch in first entry */
			T_uxlen_t last_pc = pc;
			dummyBlock.entries[0].pc = last_pc;
			/* save mem_word for get_mem_word */
			this->mem_word = fetch_decode(pc, instr, op);
			dummyBlock.entries[0].pc_increment = pc - last_pc;

			/* update block cycle counter -> see comments in decode_update_entry above */
			dummyBlock.entries[1].cycle_counter_raw += this->opMap[op].instr_time;

			return this->opMap[op].label_ptr;
		}

		/* next instruction
		 * fetches may cause exceptions, so curEntryIdx is update only after all potential exceptions
		 */
		uint32_t nextEntryIdx = curEntryIdx + 1;

		if (nextEntryIdx < curBlock->len) {
			/* hit -> use existing entry */

			struct Entry *curEntry = &curBlock->entries[nextEntryIdx];

			if (curBlock->coherence_cnt != coherence_cnt) {
				/* check and repair whole block at once */
				bool invalidate_links_once = true;
				unsigned int idx = 0;
				try {
					exception = false;
					T_uxlen_t addr = curBlock->start_addr;
					for (idx = 0; idx < curBlock->len; idx++) {
						struct Entry *e = &curBlock->entries[idx];

						/* fetch and check -> decode only if read word differs */

						stats.inc_refetches();
						uint32_t mem_word = fetch_raw(addr);

						if (unlikely(mem_word != e->mem_word)) {
							/* repair */
							stats.inc_redecodes();

							/* block content changed -> invalidate links */
							if (invalidate_links_once) {
								curBlock->invalidate_links();
								invalidate_links_once = false;
							}

							instr = Instruction(mem_word);

							decode_update_entry(e, addr, instr);
						} else {
							addr += e->pc_increment;
						}
					}

					/* success -> block is now coherent */
					curBlock->coherence_cnt = coherence_cnt;

#ifdef DBBCACHE_ENABLE_FAST
					/* since we are sure, the current block is coherent, we can now switch to ufast for next call */
					ufast_path_raw_enable(curEntry);
#endif

				} catch (SimulationTrap &e) {
					stats.inc_refetch_exceptions();

					/* exception on load */

					/* if current instruction is affected -> re-throw -> trap in ISS */
					if (idx == nextEntryIdx) {
						instr = Instruction(0);
						/* safe pc for get_last_pc_exception_safe */
						dummyBlock.entries[0].pc = pc;
						exception = true;
						throw;
					}

					/* if instructions before current are affected
					 * -> try to get instruction directly
					 * (this may also cause a exception, which is then re-thrown (see fetch) -> trap in ISS)
					 */
					if (idx < nextEntryIdx) {
						// TODO: count!!!
						fetch(pc, instr);
						decode_update_entry(curEntry, pc, instr);
						curEntryIdx = nextEntryIdx;
						return curEntry->opLabelPtr;
					}

					/* if instructions after current are affected
					 * -> cache entry is coherent, but block is not yet
					 * -> continue and try block update next time
					 */
					stats.inc_slow_hit();
					curEntryIdx = nextEntryIdx;
					instr = Instruction(curEntry->instr);
					pc += curEntry->pc_increment;
					return curEntry->opLabelPtr;
				}
			}
			/* X2 = coherent */

			stats.inc_slow_hit();

#ifdef DBBCACHE_ENABLE_FAST
			/* since we are sure, the current block is coherent, we can now switch to ufast for next call */
			ufast_path_raw_enable(curEntry);
#endif

			curEntryIdx = nextEntryIdx;
			instr = Instruction(curEntry->instr);
			pc += curEntry->pc_increment;
			return curEntry->opLabelPtr;

		} else {
			/* miss -> add new entry */
			class Entry *e = fetch_decode_add_entry(pc, instr);
			curEntryIdx = nextEntryIdx;
			return e->opLabelPtr;
		}
	}

	__always_inline T_uxlen_t get_last_pc_before_callback() {
		/* taken hit fast */
		if (likely(in_ufast_path())) {
			return ufastEntry->pc;
		}

		return curBlock->entries[curEntryIdx].pc;
	}

	__always_inline T_uxlen_t get_last_pc_exception_safe() {
		if (unlikely(exception)) {
			return dummyBlock.entries[0].pc;
		}

		/* taken hit fast */
		if (likely(in_ufast_path())) {
			return ufastEntry->pc;
		}

		return curBlock->entries[curEntryIdx].pc;
	}

	__always_inline T_uxlen_t get_pc_before_callback() {
		/* taken hit fast */
		if (likely(in_ufast_path())) {
			return ufastEntry->pc + ufastEntry->pc_increment;
		}

		return curBlock->entries[curEntryIdx].pc + curBlock->entries[curEntryIdx].pc_increment;
	}

	__always_inline T_uxlen_t get_pc_maybe_after_callback() {
		if (likely(in_ufast_path())) {
			if (likely(ufastEntry != &curBlock->entries[-1])) {
				return ufastEntry->pc + ufastEntry->pc_increment;
			}
			return (ufastEntry + 1)->pc;
		}

		if (likely(curEntryIdx != -1)) {
			return curBlock->entries[curEntryIdx].pc + curBlock->entries[curEntryIdx].pc_increment;
		}

		return curBlock->entries[curEntryIdx + 1].pc;
	}

	__always_inline uint64_t get_cycle_counter_raw() {
		/* value for after current execution is in next entry (see comments above) */
		if (likely(in_ufast_path())) {
			return cycle_counter_raw + (ufastEntry + 1)->cycle_counter_raw;
		}
		return cycle_counter_raw + curBlock->entries[curEntryIdx + 1].cycle_counter_raw;
	}

	uint32_t get_mem_word() {
		/* TODO: untested */

		if (likely(in_ufast_path())) {
			return ufastEntry->mem_word;
		}

		/* check for dummyBlock not necessary because dummyBlock->len is always zero */
		if (curEntryIdx < (int)curBlock->len) {
			return curBlock->entries[curEntryIdx].mem_word;
		}

		/* dummyBlock is running */
		return this->mem_word;
	}
};

/******************************************************************************
 * END: FUNCTIONAL IMPLEMENTATION
 ******************************************************************************/

/******************************************************************************
 * CACHE SELECT
 ******************************************************************************/

template <enum Architecture arch, typename T_uxlen_t, typename T_instr_memory_if>
#ifdef DBBCACHE_ENABLED
using DBBCacheDefault_T = DBBCache_T<arch, T_uxlen_t, T_instr_memory_if>;
#else
using DBBCacheDefault_T = DBBCacheDummy_T<arch, T_uxlen_t, T_instr_memory_if>;
#endif

/******************************************************************************
 * CACHE SELECT
 ******************************************************************************/

#endif /* RISCV_ISA_DBBCACHE_H */
