#pragma once

struct bus_lock_if {
	virtual ~bus_lock_if() {}

	virtual void lock(unsigned hart_id) = 0;

	virtual void unlock(unsigned hart_id) = 0;

	virtual bool is_locked() = 0;  // by any hart

	virtual bool is_locked(unsigned hart_id) = 0;

	virtual void wait_until_unlocked() = 0;

	inline void wait_for_access_rights(unsigned hart_id) {
		if (is_locked() && !is_locked(hart_id))
			wait_until_unlocked();
	}
};