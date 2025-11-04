#ifndef RISCV_CHERIV9_ISA64_CHERI_MEM_H
#define RISCV_CHERIV9_ISA64_CHERI_MEM_H

#include "core/common_cheriv9/cheri_cap_common.h"
#include "core/common_cheriv9/cheri_exceptions.h"
#include "core/common_cheriv9/dmi.h"
#include "iss.h"
#include "mmu.h"
#include "util/propertymap.h"
#include "util/tlm_ext_initiator.h"
#include "util/tlm_ext_tag.h"

namespace cheriv9::rv64 {
struct CombinedTaggedMemoryInterface : public sc_core::sc_module,
                                       public instr_memory_if,
                                       public data_memory_if,
                                       public mmu_memory_if {
	/* config properties */
	sc_core::sc_time prop_clock_cycle_period = sc_core::sc_time(10, sc_core::SC_NS);
	unsigned int prop_dmi_access_clock_cycles = 4;

	ISS &iss;
	std::shared_ptr<bus_lock_if> bus_lock;
	uint64_t lr_addr = 0;

	tlm_utils::simple_initiator_socket<CombinedTaggedMemoryInterface> isock;
	tlm_utils::tlm_quantumkeeper &quantum_keeper;

	// optionally add DMI ranges for optimization
	sc_core::sc_time dmi_access_delay;
	std::vector<MemoryDMI> dmi_ranges;

	uint64_t mem_start_addr;
	uint64_t mem_end_addr;

	tlm::tlm_generic_payload trans;
	tlm_ext_initiator *trans_ext_initiator;
	tlm_ext_tag *trans_ext_tag;

	MMU *mmu;

	bool last_access_was_dmi = false;
	void *last_dmi_page_host_addr = nullptr;

	CombinedTaggedMemoryInterface(sc_core::sc_module_name, ISS &owner, MMU *mmu = nullptr, uint64_t mem_start_addr = 0,
	                              uint64_t mem_end_addr = 0)
	    : iss(owner),
	      quantum_keeper(iss.quantum_keeper),
	      mem_start_addr(mem_start_addr),
	      mem_end_addr(mem_end_addr),
	      mmu(mmu) {
		/*
		 * get config properties from global property tree (or use default)
		 * Note: Instance has no name -> use the owners name is used as instance identifier
		 */
		VPPP_PROPERTY_GET("CombinedTaggedMemoryInterface." + owner.name(), "clock_cycle_period", sc_core::sc_time,
		                  prop_clock_cycle_period);
		VPPP_PROPERTY_GET("CombinedTaggedMemoryInterface." + owner.name(), "dmi_access_clock_cycles", uint64_t,
		                  prop_dmi_access_clock_cycles);

		dmi_access_delay = prop_clock_cycle_period * prop_dmi_access_clock_cycles;

		/*
		 * Note: tlm_generic_payload frees all extension objects in destructor, therefore dynamic allocation is needed
		 */
		trans_ext_tag = new tlm_ext_tag(false);
		trans_ext_initiator = new tlm_ext_initiator(&owner);
		trans.set_extension(trans_ext_tag);
		trans.set_extension(trans_ext_initiator);
	}

	// default v2p for non-tagged data
	uint64_t v2p(uint64_t vaddr, MemoryAccessType type) override {
		bool trap_if_cap = false;
		bool strip_tag = false;
		return v2p(vaddr, type, false, &trap_if_cap, &strip_tag);
	}

	// v2p for storing tagged data
	uint64_t v2p(uint64_t vaddr, MemoryAccessType type, bool tag) {
		bool trap_if_cap = false;
		bool strip_tag = false;
		return v2p(vaddr, type, tag, &trap_if_cap, &strip_tag);
	}

	// v2p for loading tagged data
	uint64_t v2p(uint64_t vaddr, MemoryAccessType type, bool *trap_if_cap, bool *strip_tag) {
		return v2p(vaddr, type, false, trap_if_cap, strip_tag);
	}

	uint64_t v2p(uint64_t vaddr, MemoryAccessType type, bool tag, bool *trap_if_cap, bool *strip_tag) {
		if (mmu == nullptr)
			return vaddr;
		return mmu->translate_virtual_to_physical_addr(vaddr, type, tag, trap_if_cap, strip_tag);
	}

	inline void _do_transaction(tlm::tlm_command cmd, uint64_t addr, uint8_t *data, unsigned num_bytes) {
		bool tag = false;
		return _do_transaction(cmd, addr, data, &tag, num_bytes);
	}

	inline void _do_transaction(tlm::tlm_command cmd, uint64_t addr, uint8_t *data, bool *p_tag, unsigned num_bytes) {
		if (unlikely(iss.rvfi_dii_enabled())) {
			// Additional address validity check required, because riscv-sail-cheri does handle mapping to CLINT
			// differently This means that at this point it must be checked, if the given address is within memory
			// bounds
			if (addr < mem_start_addr ||
			    (addr + num_bytes - 1) >
			        mem_end_addr) {  // -1 because if addr = mem_end_addr, and num_bytes = 1, the value should be
				                     // written to the last byte in memory, this is allowed --> -1
				if (unlikely((iss.trace_enabled())))
					std::cout << "WARNING: core memory transaction failed for address 0x" << std::hex << addr
					          << std::dec << " -> raise trap" << std::endl;
				if (cmd == tlm::TLM_READ_COMMAND)
					raise_trap(EXC_LOAD_ACCESS_FAULT, addr, &iss.rvfi_dii_output);
				else if (cmd == tlm::TLM_WRITE_COMMAND)
					raise_trap(EXC_STORE_AMO_ACCESS_FAULT, addr, &iss.rvfi_dii_output);
				else
					throw std::runtime_error("TLM command must be read or write");
			}
		}
		trans.set_command(cmd);
		trans.set_address(addr);
		trans.set_data_ptr(data);
		trans.set_data_length(num_bytes);
		trans.set_response_status(tlm::TLM_OK_RESPONSE);
		trans_ext_tag->tag = *p_tag;

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
				// TODO: ACCESS EXC_LOAD_ACCESS_FAULT vs. EXC_LOAD_PAGE_FAULT in commmon/mem.h
				raise_trap(EXC_LOAD_ACCESS_FAULT, addr, &iss.rvfi_dii_output);
			else if (cmd == tlm::TLM_WRITE_COMMAND)
				// TODO: EXC_STORE_AMO_ACCESS_FAULT vs. EXC_STORE_AMO_PAGE_FAULT in commmon/mem.h
				raise_trap(EXC_STORE_AMO_ACCESS_FAULT, addr, &iss.rvfi_dii_output);
			else
				throw std::runtime_error("TLM command must be read or write");
		}
		*p_tag = trans_ext_tag->tag;
	}

	template <bool isLoad>
	inline void _do_cheri_checks(uint64_t auth_idx, Capability auth_val, uint64_t addr, uint8_t width) const {
		if (!auth_val.cap.fields.tag)
			handle_cheri_cap_exception(CapEx_TagViolation, auth_idx, &iss.rvfi_dii_output);

		if (auth_val.isSealed())
			handle_cheri_cap_exception(CapEx_SealViolation, auth_idx, &iss.rvfi_dii_output);

		if (isLoad && !auth_val.cap.fields.permit_load)
			handle_cheri_cap_exception(CapEx_PermitLoadViolation, auth_idx, &iss.rvfi_dii_output);

		if (!isLoad && !auth_val.cap.fields.permit_store)
			handle_cheri_cap_exception(CapEx_PermitStoreViolation, auth_idx, &iss.rvfi_dii_output);
		if (!auth_val.inCapBounds(addr, width))
			handle_cheri_cap_exception(CapEx_LengthViolation, auth_idx, &iss.rvfi_dii_output);

		iss.trap_check_addr_alignment<isLoad>(addr, width);
	}
	template <bool isLoad>
	inline void _do_cheri_checks(uint64_t auth_idx, Capability auth_val, uint64_t addr, uint8_t width,
	                             Capability cs2) const {
		_do_cheri_checks<isLoad>(auth_idx, auth_val, addr, width);
		if (!isLoad && !auth_val.cap.fields.permit_store_cap && cs2.cap.fields.tag)
			handle_cheri_cap_exception(CapEx_PermitStoreCapViolation, auth_idx, &iss.rvfi_dii_output);
		if (!isLoad && !auth_val.cap.fields.permit_store_local_cap && cs2.cap.fields.tag && !cs2.cap.fields.global)
			handle_cheri_cap_exception(CapEx_PermitStoreLocalCapViolation, auth_idx, &iss.rvfi_dii_output);
	}

	bool _raw_load_tagged_data(uint64_t addr, __uint128_t *p_data) {
		// NOTE: a DMI load will not context switch (SystemC) and not modify the memory, hence should be able to
		// postpone the lock after the dmi access
		bus_lock->wait_for_access_rights(iss.get_hart_id());
		bool ans;
		for (auto &e : dmi_ranges) {
			if (e.contains(addr)) {
				quantum_keeper.inc(dmi_access_delay);
				ans = e.load(addr, p_data);

				/* save the host address of the start of the 4KiB page containing addr */
				last_access_was_dmi = true;
				last_dmi_page_host_addr = e.get_mem_ptr_to_global_addr<__uint128_t>(addr & ~0xFFF);

				return ans;
			}
		}
		_do_transaction(tlm::TLM_READ_COMMAND, addr, reinterpret_cast<uint8_t *>(p_data), &ans, sizeof(__uint128_t));
		/*
		 * A transaction may lead to a context switch. The other context may issue transaction handled via dmi.
		 * i.e.: When we come back to this context, it is possible that we end up with last_access_was_dmi set to true!
		 * Solution: set last_access_was_dmi to false AFTER _do_transaction
		 */
		last_access_was_dmi = false;
		return ans;
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

	inline void _raw_store_tagged_data(uint64_t addr, __uint128_t data, bool tag) {
		bus_lock->wait_for_access_rights(iss.get_hart_id());

		for (auto &e : dmi_ranges) {
			if (e.contains(addr)) {
				quantum_keeper.inc(dmi_access_delay);
				e.store(addr, data, tag);

				/* save the host address of the start of the 4KiB page containing addr */
				last_access_was_dmi = true;
				last_dmi_page_host_addr = e.get_mem_ptr_to_global_addr<__uint128_t>(addr & ~0xFFF);

				atomic_unlock();
				return;
			}
		}

		_do_transaction(tlm::TLM_WRITE_COMMAND, addr, reinterpret_cast<uint8_t *>(&data), &tag, sizeof(__uint128_t));
		atomic_unlock();
		/* see comment in _raw_load_data */
		last_access_was_dmi = false;
	}

	template <typename T>
	inline T _load_data(uint64_t addr) {
		T data = _raw_load_data<T>(v2p(addr, LOAD));
		if (unlikely(iss.rvfi_dii_enabled())) {
			iss.rvfi_dii_output.rvfi_dii_mem_addr = addr;
			iss.rvfi_dii_output.rvfi_dii_mem_rdata = static_cast<uint64_t>(data);
			iss.rvfi_dii_output.rvfi_dii_mem_rmask = (1ULL << (sizeof(T))) - 1;
		}
		return data;
	}

	template <typename T>
	inline void _store_data(uint64_t addr, T value) {
		if (unlikely(iss.rvfi_dii_enabled())) {
			iss.rvfi_dii_output.rvfi_dii_mem_addr = addr;
			iss.rvfi_dii_output.rvfi_dii_mem_wmask = (1ULL << (sizeof(T))) - 1;
			if (sizeof(T) >= 8) {
				iss.rvfi_dii_output.rvfi_dii_mem_wdata = value;
			} else {
				iss.rvfi_dii_output.rvfi_dii_mem_wdata = value & ((1ULL << (sizeof(T) * 8)) - 1);
			}
		}
		_raw_store_data(v2p(addr, STORE), value);
	}

	uint64_t mmu_load_pte64(uint64_t addr) override {
		if (unlikely(iss.rvfi_dii_enabled()))
			iss.rvfi_dii_output.rvfi_dii_mem_addr = addr;

		uint64_t data = _raw_load_data<uint64_t>(addr);
		if (unlikely(iss.rvfi_dii_enabled())) {
			iss.rvfi_dii_output.rvfi_dii_mem_rdata = data;
			iss.rvfi_dii_output.rvfi_dii_mem_rmask = (1 << 8) - 1;
		}
		return data;
	}

	uint64_t mmu_load_pte32(uint64_t addr) override {
		if (unlikely(iss.rvfi_dii_enabled()))
			iss.rvfi_dii_output.rvfi_dii_mem_addr = addr;
		uint32_t data = _raw_load_data<uint32_t>(addr);
		if (unlikely(iss.rvfi_dii_enabled())) {
			iss.rvfi_dii_output.rvfi_dii_mem_rdata = data;
			iss.rvfi_dii_output.rvfi_dii_mem_rmask = (1 << 4) - 1;
		}
		return data;
	}

	void mmu_store_pte32(uint64_t addr, uint32_t value) override {
		if (unlikely(iss.rvfi_dii_enabled())) {
			iss.rvfi_dii_output.rvfi_dii_mem_addr = addr;
			// Size of 8 bytes, because sail apparently writes 8 bytes, even though only 4 are used
			iss.rvfi_dii_output.rvfi_dii_mem_wmask = (1ULL << (8)) - 1;
			iss.rvfi_dii_output.rvfi_dii_mem_wdata = value;
		}
		_raw_store_data(addr, value);
	}

	void flush_tlb() override {
		mmu->flush_tlb();
	}

	uint32_t load_instr(uint64_t addr) override {
		return _raw_load_data<uint32_t>(v2p(addr, FETCH));
	}

	uint16_t load_instr_half(uint64_t addr) override {
		return _raw_load_data<uint16_t>(v2p(addr, FETCH));
	}

	// This function is used to ensure that the PC is translated correctly, even if no actual instruction is loaded
	uint64_t translate_pc(uint64_t pc) override {
		return v2p(pc, FETCH);
	}

	template <typename T>
	T _atomic_load_data(uint64_t addr) {
		bus_lock->lock(iss.get_hart_id());
		return _load_data<T>(addr);
	}

	bool _atomic_load_tagged_data(uint64_t addr, __uint128_t *p_data) {
		bus_lock->lock(iss.get_hart_id());
		bool strip_tag = false;
		bool trap = false;
		bool tag = _raw_load_tagged_data(v2p(addr, LOAD, &trap, &strip_tag), p_data);
		if (trap) {
			handle_cheri_exception(EXC_CHERI_LOAD_FAULT, addr, &iss.rvfi_dii_output);
		}
		if (strip_tag) {
			tag = false;
		}
		return tag;
	}

	template <typename T>
	void _atomic_store_data(uint64_t addr, T value) {
		/*
		 * This is sometimes triggerd when running RV64 debian and apt and when running CheriBSD
		 * TODO: check cause and fix
		 */
		if (unlikely(!bus_lock->is_locked(iss.get_hart_id()))) {
			std::cerr << "CombinedTaggedMemoryInterface: WARNING: bus not locked in _atomic_store_data (please report "
			             "to VP developers)"
			          << std::endl;
		}
		_store_data(addr, value);
	}

	void _atomic_store_tagged_data(uint64_t addr, __uint128_t data, bool tag) {
		/*
		 * This is sometimes triggerd when running RV64 debian and apt and when running CheriBSD
		 * TODO: check cause and fix
		 */
		if (unlikely(!bus_lock->is_locked(iss.get_hart_id()))) {
			std::cerr << "CombinedTaggedMemoryInterface: WARNING: bus not locked in _atomic_store_tagged_data (please "
			             "report to VP developers)"
			          << std::endl;
		}
		_raw_store_tagged_data(v2p(addr, STORE, tag), data, tag);
	}

	template <typename T>
	T _atomic_load_reserved_data(uint64_t addr) {
		bus_lock->lock(iss.get_hart_id());
		lr_addr = addr;
		return _load_data<T>(addr);
	}

	bool _atomic_load_reserved_tagged_data(uint64_t addr, bool *trap, bool *strip_tag, __uint128_t *p_data) {
		bus_lock->lock(iss.get_hart_id());
		lr_addr = addr;
		return _raw_load_tagged_data(v2p(addr, LOAD, trap, strip_tag), p_data);
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

	bool _atomic_store_conditional_tagged_data(uint64_t addr, __uint128_t data, bool tag) {
		if (bus_lock->is_locked(iss.get_hart_id())) {
			if (addr == lr_addr) {
				_raw_store_tagged_data(v2p(addr, STORE, tag), data, tag);
				return true;
			}
			atomic_unlock();
		}
		return false;
	}

	Capability load_cap(uint64_t addr) override {
		if (unlikely(iss.rvfi_dii_enabled()))
			iss.rvfi_dii_output.rvfi_dii_mem_addr = addr;

		__uint128_t cap_data;
		bool tag = _raw_load_tagged_data(addr, &cap_data);

		Capability cap = Capability(cap_data, tag);
		if (unlikely(iss.rvfi_dii_enabled())) {
			iss.rvfi_dii_output.rvfi_dii_mem_rdata = cap;
			iss.rvfi_dii_output.rvfi_dii_mem_rmask = (1 << 8) - 1;
		}

		return cap;
	}

	int64_t load_double(uint64_t addr) override {
		return _load_data<int64_t>(addr);
	}

	int64_t load_word(uint64_t addr) override {
		return _load_data<int32_t>(addr);
	}

	int64_t load_half(uint64_t addr) override {
		return _load_data<int16_t>(addr);
	}

	int64_t load_byte(uint64_t addr) override {
		return _load_data<int8_t>(addr);
	}

	uint64_t load_uword(uint64_t addr) override {
		return _load_data<uint32_t>(addr);
	}

	uint64_t load_uhalf(uint64_t addr) override {
		return _load_data<uint16_t>(addr);
	}

	uint64_t load_ubyte(uint64_t addr) override {
		return _load_data<uint8_t>(addr);
	}

	void atomic_store_cap(uint64_t addr, Capability value) override {
		_atomic_store_tagged_data(addr, value.toUint128(), value.cap.fields.tag);
	}

	void store_cap(uint64_t addr, Capability value) override {
		_raw_store_tagged_data(addr, value.toUint128(), value.cap.fields.tag);
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

	int64_t atomic_load_word(uint64_t addr) override {
		return _atomic_load_data<int32_t>(addr);
	}

	int64_t atomic_load_word_via_cap(uint64_t addr, Capability auth_val, uint64_t auth_idx) override {
		if (!auth_val.cap.fields.tag)
			handle_cheri_cap_exception(CapEx_TagViolation, auth_idx, &iss.rvfi_dii_output);
		if (auth_val.isSealed())
			handle_cheri_cap_exception(CapEx_SealViolation, auth_idx, &iss.rvfi_dii_output);
		if (!auth_val.cap.fields.permit_load)
			handle_cheri_cap_exception(CapEx_PermitLoadViolation, auth_idx, &iss.rvfi_dii_output);
		if (!auth_val.inCapBounds(addr, 4))
			handle_cheri_cap_exception(CapEx_LengthViolation, auth_idx, &iss.rvfi_dii_output);

		return atomic_load_word(addr);
	}

	void atomic_store_word(uint64_t addr, uint32_t value) override {
		_atomic_store_data(addr, value);
	}

	void atomic_store_word_via_cap(uint64_t addr, uint32_t value, Capability auth_val, uint64_t auth_idx) override {
		if (!auth_val.cap.fields.tag)
			handle_cheri_cap_exception(CapEx_TagViolation, auth_idx, &iss.rvfi_dii_output);
		if (auth_val.isSealed())
			handle_cheri_cap_exception(CapEx_SealViolation, auth_idx, &iss.rvfi_dii_output);
		if (!auth_val.cap.fields.permit_store)
			handle_cheri_cap_exception(CapEx_PermitStoreViolation, auth_idx, &iss.rvfi_dii_output);
		if (!auth_val.inCapBounds(addr, 4))
			handle_cheri_cap_exception(CapEx_LengthViolation, auth_idx, &iss.rvfi_dii_output);

		atomic_store_word(addr, value);
	}

	int64_t atomic_load_reserved_word(uint64_t addr) override {
		return _atomic_load_reserved_data<int32_t>(addr);
	}

	bool atomic_store_conditional_word(uint64_t addr, uint32_t value) override {
		return _atomic_store_conditional_data(addr, value);
	}

	int64_t atomic_load_double(uint64_t addr) override {
		return _atomic_load_data<int64_t>(addr);
	}

	int64_t atomic_load_double_via_cap(uint64_t addr, Capability auth_val, uint64_t auth_idx) override {
		if (!auth_val.cap.fields.tag)
			handle_cheri_cap_exception(CapEx_TagViolation, auth_idx, &iss.rvfi_dii_output);
		if (auth_val.isSealed())
			handle_cheri_cap_exception(CapEx_SealViolation, auth_idx, &iss.rvfi_dii_output);
		if (!auth_val.cap.fields.permit_load)
			handle_cheri_cap_exception(CapEx_PermitLoadViolation, auth_idx, &iss.rvfi_dii_output);
		if (!auth_val.inCapBounds(addr, 8))
			handle_cheri_cap_exception(CapEx_LengthViolation, auth_idx, &iss.rvfi_dii_output);

		return atomic_load_double(addr);
	}

	void atomic_store_double(uint64_t addr, uint64_t value) override {
		_atomic_store_data(addr, value);
	}

	void atomic_store_double_via_cap(uint64_t addr, uint64_t value, Capability auth_val, uint64_t auth_idx) override {
		if (!auth_val.cap.fields.tag)
			handle_cheri_cap_exception(CapEx_TagViolation, auth_idx, &iss.rvfi_dii_output);
		if (auth_val.isSealed())
			handle_cheri_cap_exception(CapEx_SealViolation, auth_idx, &iss.rvfi_dii_output);
		if (!auth_val.cap.fields.permit_store)
			handle_cheri_cap_exception(CapEx_PermitStoreViolation, auth_idx, &iss.rvfi_dii_output);
		if (!auth_val.inCapBounds(addr, 8))
			handle_cheri_cap_exception(CapEx_LengthViolation, auth_idx, &iss.rvfi_dii_output);

		atomic_store_double(addr, value);
	}

	int64_t atomic_load_reserved_double(uint64_t addr) override {
		return _atomic_load_reserved_data<int64_t>(addr);
	}

	bool atomic_store_conditional_double(uint64_t addr, uint64_t value) override {
		return _atomic_store_conditional_data(addr, value);
	}

	Capability atomic_load_cap(uint64_t addr) override {
		if (unlikely(iss.rvfi_dii_enabled()))
			iss.rvfi_dii_output.rvfi_dii_mem_addr = addr;
		__uint128_t cap_data;
		bool tag = _atomic_load_tagged_data(addr, &cap_data);
		Capability cap = Capability(cap_data, tag);

		if (unlikely(iss.rvfi_dii_enabled())) {
			iss.rvfi_dii_output.rvfi_dii_mem_rdata = cap;
			iss.rvfi_dii_output.rvfi_dii_mem_rmask = (1 << 8) - 1;
		}

		return cap;
	}

	void atomic_unlock() override {
		bus_lock->unlock(iss.get_hart_id());
	}

	void handle_store_data_via_cap(Capability rs2, uint64_t auth_idx, Capability auth_val, uint64_t addr,
	                               uint8_t width) override {
		if (width == CLEN) {
			_do_cheri_checks<false>(auth_idx, auth_val, addr, width, rs2);

		} else {
			_do_cheri_checks<false>(auth_idx, auth_val, addr, width);
		}
		uint64_t paddr = (width == 16) ? v2p(addr, STORE, rs2.cap.fields.tag) : v2p(addr, STORE);

		if (unlikely(iss.rvfi_dii_enabled())) {
			iss.rvfi_dii_output.rvfi_dii_mem_addr = paddr;
			iss.rvfi_dii_output.rvfi_dii_mem_wmask = (1ULL << (width)) - 1;
			if (width >= 8) {
				iss.rvfi_dii_output.rvfi_dii_mem_wdata = rs2;
			} else {
				iss.rvfi_dii_output.rvfi_dii_mem_wdata = rs2 & ((1ULL << (width * 8)) - 1);
			}
		}

		switch (width) {
			case 1: {
				uint8_t data = rs2 & 0xFF;
				_raw_store_data(paddr, data);
				break;
			}
			case 2: {
				uint16_t data = rs2 & 0xFFFF;
				_raw_store_data(paddr, data);
				break;
			}
			case 4: {
				uint32_t data = rs2 & 0xFFFFFFFF;
				_raw_store_data(paddr, data);
				break;
			}
			case 8: {
				uint64_t data = rs2;
				_raw_store_data(paddr, data);
				break;
			}
			case 16: {
				__uint128_t data = rs2.toUint128();
				_raw_store_tagged_data(paddr, data, rs2.cap.fields.tag);
				// cap_mem[paddr] = rs2; // Only for debugging purposes // TODO Remove before pushing to main
				break;
			}
			default:
				assert(false && "Invalid width");
		}
	}

	bool atomic_store_conditional_data_via_cap(Capability rs2, uint64_t auth_idx, Capability auth_val, uint64_t addr,
	                                           uint8_t width) override {
		if (width == CLEN) {
			_do_cheri_checks<false>(auth_idx, auth_val, addr, width, rs2);

		} else {
			_do_cheri_checks<false>(auth_idx, auth_val, addr, width);
		}
		bool success = false;
		switch (width) {
			case 1: {
				uint8_t data = rs2 & 0xFF;
				success = _atomic_store_conditional_data(addr, data);
				break;
			}
			case 2: {
				uint16_t data = rs2 & 0xFFFF;
				success = _atomic_store_conditional_data(addr, data);
				break;
			}
			case 4: {
				uint32_t data = rs2 & 0xFFFFFFFF;
				success = _atomic_store_conditional_data(addr, data);
				break;
			}
			case 8: {
				uint64_t data = rs2;
				success = _atomic_store_conditional_data(addr, data);
				break;
			}
			case 16: {
				__uint128_t data = rs2.toUint128();
				success = _atomic_store_conditional_tagged_data(addr, data, rs2.cap.fields.tag);
				break;
			}
			default:
				assert(false && "Invalid width");
		}
		if (unlikely(iss.rvfi_dii_enabled()) && success) {
			iss.rvfi_dii_output.rvfi_dii_mem_addr = addr;
			iss.rvfi_dii_output.rvfi_dii_mem_wmask = (1ULL << (width)) - 1;
			if (width >= 8) {
				iss.rvfi_dii_output.rvfi_dii_mem_wdata = rs2;
			} else {
				iss.rvfi_dii_output.rvfi_dii_mem_wdata = rs2 & ((1ULL << (width * 8)) - 1);
			}
		}
		return success;
	}

	Capability handle_load_cap_via_cap(uint64_t auth_idx, Capability auth_val, uint64_t addr) override {
		_do_cheri_checks<true>(auth_idx, auth_val, addr, cCapSize);
		__uint128_t data;
		bool strip_tag = false;
		bool trap = false;
		bool tag = _raw_load_tagged_data(v2p(addr, LOAD, &trap, &strip_tag), &data);
		if (trap) {
			handle_cheri_exception(EXC_CHERI_LOAD_FAULT, addr, &iss.rvfi_dii_output);
		}
		if (strip_tag) {
			tag = false;
		}
		Capability cap = Capability(data, tag);

		if (unlikely(iss.rvfi_dii_enabled())) {
			iss.rvfi_dii_output.rvfi_dii_mem_addr = addr;
			iss.rvfi_dii_output.rvfi_dii_mem_rdata = cap.cap.fields.address;
			iss.rvfi_dii_output.rvfi_dii_mem_rmask = (1ULL << (8)) - 1;  // 8 is max value for uint8_t
		}
		// std::cout << "Loading cap from addr: " <<  std::dec << addr << "(" << std::hex << addr << ")" << std::dec <<
		// std::endl;
		return cap;
	}

	uint64_t handle_load_data_via_cap(uint64_t auth_idx, Capability auth_val, uint64_t addr, bool is_unsigned,
	                                  uint8_t width) override {
		_do_cheri_checks<true>(auth_idx, auth_val, addr, width);
		uint64_t paddr = v2p(addr, LOAD);
		uint64_t data;
		switch (width) {
			case 1: {
				if (is_unsigned)
					data = _raw_load_data<uint8_t>(paddr);
				else
					data = _raw_load_data<int8_t>(paddr);
				break;
			}
			case 2: {
				if (is_unsigned)
					data = _raw_load_data<uint16_t>(paddr);
				else
					data = _raw_load_data<int16_t>(paddr);
				break;
			}
			case 4: {
				if (is_unsigned)
					data = _raw_load_data<uint32_t>(paddr);
				else
					data = _raw_load_data<int32_t>(paddr);
				break;
			}
			case 8: {
				if (is_unsigned)
					data = _raw_load_data<uint64_t>(paddr);
				else
					data = _raw_load_data<int64_t>(paddr);
				break;
			}
			default:
				assert(false && "Invalid width");
		}
		if (unlikely(iss.rvfi_dii_enabled())) {
			iss.rvfi_dii_output.rvfi_dii_mem_addr = paddr;
			iss.rvfi_dii_output.rvfi_dii_mem_rdata = data;
			iss.rvfi_dii_output.rvfi_dii_mem_rmask = (1ULL << (width)) - 1;
		}
		return data;
	}

	uint64_t atomic_load_reserved_data_via_cap(uint64_t auth_idx, Capability auth_val, uint64_t addr,
	                                           uint8_t width) override {
		_do_cheri_checks<true>(auth_idx, auth_val, addr, width);
		uint64_t data;
		switch (width) {
			case 1: {
				data = _atomic_load_reserved_data<int8_t>(addr);
				break;
			}
			case 2: {
				data = _atomic_load_reserved_data<int16_t>(addr);
				break;
			}
			case 4: {
				data = _atomic_load_reserved_data<int32_t>(addr);
				break;
			}
			case 8: {
				data = _atomic_load_reserved_data<int64_t>(addr);
				break;
			}
			default:
				assert(false && "Invalid width");
		}
		return data;
	}

	Capability atomic_load_reserved_cap_via_cap(uint64_t auth_idx, Capability auth_val, uint64_t addr) override {
		_do_cheri_checks<true>(auth_idx, auth_val, addr, cCapSize);
		// This loads an entire capability at once (this must include the tag)
		__uint128_t cap_data;
		bool strip_tag = false;
		bool trap = false;
		bool tag = _atomic_load_reserved_tagged_data(addr, &trap, &strip_tag, &cap_data);
		if (trap) {
			handle_cheri_exception(EXC_CHERI_LOAD_FAULT, addr, &iss.rvfi_dii_output);
		}
		if (strip_tag) {
			tag = false;
		}

		Capability cap = Capability(cap_data, tag);
		if (unlikely(iss.rvfi_dii_enabled())) {
			iss.rvfi_dii_output.rvfi_dii_mem_addr = addr;
			iss.rvfi_dii_output.rvfi_dii_mem_rdata = cap.cap.fields.address;
			iss.rvfi_dii_output.rvfi_dii_mem_rmask = (1ULL << (8)) - 1;  // 8 is max value for uint8_t
		}

		return cap;
	}

	uint8_t load_tags(uint64_t addr) override {
		// TODO This can be much more optimized
		// Access only tags
		// e.g use dmi, which has function to load only tags
		uint8_t tags = 0;
		uint64_t paddr = v2p(addr, LOAD);
		for (uint64_t i = 0; i < cCapsPerCacheLine; i++) {
			Capability cap = load_cap(paddr + i * cCapSize);
			tags |= cap.cap.fields.tag << i;
		}
		return tags;
	}

	void reset(uint64_t start, uint64_t end) override {
		// Write 0 to all memory locations in the range [start, end)
		for (uint64_t i = start; i < end; i += 16) {
			store_cap(i, cNullCap);
		}
	}

	bool is_bus_locked() override {
		return bus_lock->is_locked();
	}

	void *get_last_dmi_page_host_addr() override {
		if (!last_access_was_dmi) {
			return nullptr;
		}
		return last_dmi_page_host_addr;
	}
};
} /* namespace cheriv9::rv64 */

#endif /* RISCV_CHERIV9_ISA64_CHERI_MEM_H */
