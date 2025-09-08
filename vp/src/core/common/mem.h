#ifndef RISCV_ISA_MEM_H
#define RISCV_ISA_MEM_H

#include "bus_lock_if.h"
#include "dmi.h"
#include "mem_if.h"
#include "mmu.h"
#include "util/initator_ext.h"

/*
 * For optimization, use DMI to fetch instructions
 * NOTE: Also by-passes the MMU for efficiency reasons -> CAN NOT BE USED ON PLATFORMS WITH VIRTUAL MEMORY MANAGEMENT!
 */
template <typename T_RVX_ISS>
struct InstrMemoryProxy_T : public instr_memory_if {
	MemoryDMI dmi;

	tlm_utils::tlm_quantumkeeper &quantum_keeper;
	sc_core::sc_time clock_cycle = sc_core::sc_time(10, sc_core::SC_NS);
	sc_core::sc_time access_delay = clock_cycle * 2;

	InstrMemoryProxy_T(const MemoryDMI &dmi, T_RVX_ISS &owner) : dmi(dmi), quantum_keeper(owner.quantum_keeper) {}

	virtual uint32_t load_instr(uint64_t pc) override {
		quantum_keeper.inc(access_delay);
		return dmi.load<uint32_t>(pc);
	}
};

template <typename T_RVX_ISS, typename T_sxlen_t, typename T_uxlen_t>
struct CombinedMemoryInterface_T : public sc_core::sc_module,
                                   public instr_memory_if,
                                   public data_memory_if_T<T_sxlen_t, T_uxlen_t>,
                                   public mmu_memory_if {
	T_RVX_ISS &iss;
	std::shared_ptr<bus_lock_if> bus_lock;
	uint64_t lr_addr = 0;

	tlm_utils::simple_initiator_socket<CombinedMemoryInterface_T> isock;
	tlm_utils::tlm_quantumkeeper &quantum_keeper;

	// optionally add DMI ranges for optimization
	sc_core::sc_time clock_cycle = sc_core::sc_time(10, sc_core::SC_NS);
	sc_core::sc_time dmi_access_delay = clock_cycle * 4;
	std::vector<MemoryDMI> dmi_ranges;

	tlm::tlm_generic_payload trans;
	initiator_ext *ext;

	MMU_T<T_RVX_ISS> *mmu;

	bool last_access_was_dmi = false;
	void *last_dmi_page_host_addr = nullptr;

	CombinedMemoryInterface_T(sc_core::sc_module_name, T_RVX_ISS &owner, MMU_T<T_RVX_ISS> *mmu = nullptr)
	    : iss(owner), quantum_keeper(iss.quantum_keeper), mmu(mmu) {
		ext = new initiator_ext(&owner);  // tlm_generic_payload frees all extension objects in destructor, therefore
		                                  // dynamic allocation is needed
		trans.set_extension<initiator_ext>(ext);
	}

	uint64_t v2p(uint64_t vaddr, MemoryAccessType type) override {
		if (mmu == nullptr)
			return vaddr;
		return mmu->translate_virtual_to_physical_addr(vaddr, type);
	}

	inline void _do_transaction(tlm::tlm_command cmd, uint64_t addr, uint8_t *data, unsigned num_bytes) {
		trans.set_command(cmd);
		trans.set_address(addr);
		trans.set_data_ptr(data);
		trans.set_data_length(num_bytes);
		trans.set_response_status(tlm::TLM_OK_RESPONSE);

		/* ensure, that quantum_keeper value of ISS is up-to-date */
		iss.commit_cycles();

		/* update quantum values by transaction delay */

		sc_core::sc_time local_delay = quantum_keeper.get_local_time();

		isock->b_transport(trans, local_delay);

		quantum_keeper.set(local_delay);

		if (trans.is_response_error()) {
			if (iss.trace_enabled())
				std::cout << "WARNING: core memory transaction failed for address 0x" << std::hex << addr << std::dec
				          << " -> raise trap" << std::endl;
			if (cmd == tlm::TLM_READ_COMMAND)
				raise_trap(EXC_LOAD_PAGE_FAULT, addr);
			else if (cmd == tlm::TLM_WRITE_COMMAND)
				raise_trap(EXC_STORE_AMO_PAGE_FAULT, addr);
			else
				throw std::runtime_error("TLM command must be read or write");
		}
	}

	template <typename T>
	inline T _raw_load_data(uint64_t addr) {
		// NOTE: a DMI load will not context switch (SystemC) and not modify the memory, hence should be able to
		// postpone the lock after the dmi access
		bus_lock->wait_for_access_rights(iss.get_hart_id());

		T ans;

		for (auto &e : dmi_ranges) {
			if (e.contains(addr)) {
				quantum_keeper.inc(dmi_access_delay);
				ans = e.load<T>(addr);

				/* save the host address of the start of the 4KiB page containing addr */
				last_access_was_dmi = true;
				last_dmi_page_host_addr = e.get_mem_ptr_to_global_addr<T>(addr & ~0xFFF);

				return ans;
			}
		}

		_do_transaction(tlm::TLM_READ_COMMAND, addr, (uint8_t *)&ans, sizeof(T));

		/*
		 * A transaction may lead to a context switch. The other context may issue transaction handled via dmi.
		 * i.e.: When we come back to this context, it is possible that we end up with last_access_was_dmi set to true!
		 * Solution: set last_access_was_dmi to false AFTER _do_transaction
		 */
		last_access_was_dmi = false;

		return ans;
	}

	template <typename T>
	inline void _raw_store_data(uint64_t addr, T value) {
		bus_lock->wait_for_access_rights(iss.get_hart_id());

		for (auto &e : dmi_ranges) {
			if (e.contains(addr)) {
				quantum_keeper.inc(dmi_access_delay);
				e.store(addr, value);

				/* save the host address of the start of the 4KiB page containing addr */
				last_access_was_dmi = true;
				last_dmi_page_host_addr = e.get_mem_ptr_to_global_addr<T>(addr & ~0xFFF);

				atomic_unlock();
				return;
			}
		}

		_do_transaction(tlm::TLM_WRITE_COMMAND, addr, (uint8_t *)&value, sizeof(T));
		atomic_unlock();

		/* see comment in _raw_load_data */
		last_access_was_dmi = false;
	}

	template <typename T>
	inline T _load_data(uint64_t addr) {
		return _raw_load_data<T>(v2p(addr, LOAD));
	}

	template <typename T>
	inline void _store_data(uint64_t addr, T value) {
		_raw_store_data(v2p(addr, STORE), value);
	}

	uint64_t mmu_load_pte64(uint64_t addr) override {
		return _raw_load_data<uint64_t>(addr);
	}
	uint64_t mmu_load_pte32(uint64_t addr) override {
		return _raw_load_data<uint32_t>(addr);
	}
	void mmu_store_pte32(uint64_t addr, uint32_t value) override {
		_raw_store_data(addr, value);
	}

	void flush_tlb() override {
		mmu->flush_tlb();
	}

	uint32_t load_instr(uint64_t addr) override {
		/*
		 * We have support for RISC-V Compressed C instructions.
		 * As a consequence we might have misaligned 32 bit fetches.
		 *
		 * This is a problem if
		 *  1. the fetch happens at a page boundary AND
		 *  2. virtual memory management is used AND
		 *  3. the two involved pages are not stored sequentially in physical memory.
		 *
		 * e.g.
		 * <pc>		<instr>		<pc_increment>
		 * 0x1FF8	fmul ...	4
		 * 0x1FFC	c.lw ...	2 (compressed)
		 * 0x1FFE	fmax ...	4				<-- not 32 bit aligned @ page boundary
		 * 0x2002	fmin ...	4				<-- not 32 bit aligned
		 *
		 * -> A non-aligned 32 bit fetch at a 4KiB page boundary has to be split!
		 *
		 * TODO: Maybe it would be more realistic to split all misaligned 32 bit accesses
		 * in two 16 bit (-> no misaligned accesses on the bus) -> Future work
		 *
		 */
		if ((addr & 0xFFF) == 0xFFE) {
			return (_raw_load_data<uint16_t>(v2p(addr + 2, FETCH)) << 16) |
			       (_raw_load_data<uint16_t>(v2p(addr + 0, FETCH)) << 0);
		}

		return _raw_load_data<uint32_t>(v2p(addr, FETCH));
	}

	template <typename T>
	T _atomic_load_data(uint64_t addr) {
		bus_lock->lock(iss.get_hart_id());
		return _load_data<T>(addr);
	}
	template <typename T>
	void _atomic_store_data(uint64_t addr, T value) {
		assert(bus_lock->is_locked(iss.get_hart_id()));
		_store_data(addr, value);
	}
	template <typename T>
	T _atomic_load_reserved_data(uint64_t addr) {
		bus_lock->lock(iss.get_hart_id());
		lr_addr = addr;
		return _load_data<T>(addr);
	}
	template <typename T>
	bool _atomic_store_conditional_data(uint64_t addr, T value) {
		/* According to the RISC-V ISA, an implementation can fail each LR/SC sequence that does not satisfy the forward
		 * progress semantic.
		 * The lock is established by the LR instruction and the lock is kept while forward progress is maintained. */
		if (bus_lock->is_locked(iss.get_hart_id())) {
			if (addr == lr_addr) {
				_store_data(addr, value);
				return true;
			}
			atomic_unlock();
		}
		return false;
	}

	int64_t load_double(uint64_t addr) override {
		return _load_data<int64_t>(addr);
	}
	T_sxlen_t load_word(uint64_t addr) override {
		return _load_data<int32_t>(addr);
	}
	T_sxlen_t load_half(uint64_t addr) override {
		return _load_data<int16_t>(addr);
	}
	T_sxlen_t load_byte(uint64_t addr) override {
		return _load_data<int8_t>(addr);
	}

	/* unused on RV32 */
	T_uxlen_t load_uword(uint64_t addr) override {
		return _load_data<uint32_t>(addr);
	}

	T_uxlen_t load_uhalf(uint64_t addr) override {
		return _load_data<uint16_t>(addr);
	}
	T_uxlen_t load_ubyte(uint64_t addr) override {
		return _load_data<uint8_t>(addr);
	}

	void store_double(uint64_t addr, uint64_t value) override {
		_store_data(addr, value);
	}
	void store_word(uint64_t addr, uint32_t value) override {
		_store_data(addr, value);
	}
	void store_half(uint64_t addr, uint16_t value) override {
		_store_data(addr, value);
	}
	void store_byte(uint64_t addr, uint8_t value) override {
		_store_data(addr, value);
	}

	T_sxlen_t atomic_load_word(uint64_t addr) override {
		return _atomic_load_data<int32_t>(addr);
	}
	void atomic_store_word(uint64_t addr, uint32_t value) override {
		_atomic_store_data(addr, value);
	}
	T_sxlen_t atomic_load_reserved_word(uint64_t addr) override {
		return _atomic_load_reserved_data<int32_t>(addr);
	}
	bool atomic_store_conditional_word(uint64_t addr, uint32_t value) override {
		return _atomic_store_conditional_data(addr, value);
	}

	/* unused on RV32 */
	int64_t atomic_load_double(uint64_t addr) override {
		return _atomic_load_data<int64_t>(addr);
	}
	void atomic_store_double(uint64_t addr, uint64_t value) override {
		_atomic_store_data(addr, value);
	}
	int64_t atomic_load_reserved_double(uint64_t addr) override {
		return _atomic_load_reserved_data<int64_t>(addr);
	}
	bool atomic_store_conditional_double(uint64_t addr, uint64_t value) override {
		return _atomic_store_conditional_data(addr, value);
	}

	void atomic_unlock() override {
		bus_lock->unlock(iss.get_hart_id());
	}

	inline bool is_bus_locked() override {
		return bus_lock->is_locked();
	}

	/* see comment in data_memory_if_T */
	void *get_last_dmi_page_host_addr() override {
		if (!last_access_was_dmi) {
			return nullptr;
		}
		return last_dmi_page_host_addr;
	}
};

#endif /* RISCV_ISA_MEM_H */
