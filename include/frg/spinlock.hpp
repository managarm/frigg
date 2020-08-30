#pragma once

#include <stdint.h>

namespace frg {

struct ticket_spinlock {
	constexpr ticket_spinlock()
	: next_ticket_{0}, serving_ticket_{0} { }

	ticket_spinlock(const ticket_spinlock &) = delete;
	ticket_spinlock &operator= (const ticket_spinlock &) = delete;

	void lock() {
		auto ticket = __atomic_fetch_add(&next_ticket_, 1, __ATOMIC_RELAXED);
		while(__atomic_load_n(&serving_ticket_, __ATOMIC_ACQUIRE) != ticket)
			;
	}

	void unlock() {
		auto current = __atomic_load_n(&serving_ticket_, __ATOMIC_RELAXED);
		__atomic_store_n(&serving_ticket_, current + 1, __ATOMIC_RELEASE);
	}

private:
	uint32_t next_ticket_;
	uint32_t serving_ticket_;
};

} // namespace frg
