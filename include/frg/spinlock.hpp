#pragma once

#include <stdint.h>

namespace frg {

namespace detail {

[[gnu::always_inline]] inline void loophint() {
#if defined(__x86_64__) || defined(__i386__)
	// use the dedicated `pause` instruction
	__builtin_ia32_pause();
#elif defined(__aarch64__)
	//
	// `isb` seems more reliable for than `yield` for releasing
	// resources on modern aarch64 CPUs
	//
	// https://bugs.java.com/bugdatabase/view_bug.do?bug_id=8258604
	// https://bugs.mysql.com/bug.php?id=100664
	//
	asm volatile ("isb" ::: "memory");
#else
	// otherwise, use a standard memory barrier
	asm volatile ("" ::: "memory");
#endif
}

} // namespace detail

struct ticket_spinlock {
	constexpr ticket_spinlock()
	: next_ticket_{0}, serving_ticket_{0} { }

	ticket_spinlock(const ticket_spinlock &) = delete;
	ticket_spinlock &operator= (const ticket_spinlock &) = delete;

	void lock() {
		auto ticket = __atomic_fetch_add(&next_ticket_, 1, __ATOMIC_RELAXED);
		while(__atomic_load_n(&serving_ticket_, __ATOMIC_ACQUIRE) != ticket) {
			detail::loophint();
		}
	}

	bool is_locked() {
		return (__atomic_load_n(&serving_ticket_, __ATOMIC_RELAXED) + 1)
			== __atomic_load_n(&next_ticket_, __ATOMIC_RELAXED);
	}

	void unlock() {
		auto current = __atomic_load_n(&serving_ticket_, __ATOMIC_RELAXED);
		__atomic_store_n(&serving_ticket_, current + 1, __ATOMIC_RELEASE);
	}

private:
	uint32_t next_ticket_;
	uint32_t serving_ticket_;
};

struct simple_spinlock {
	constexpr simple_spinlock()
	: lock_{false} { }

	simple_spinlock(const simple_spinlock &) = delete;
	simple_spinlock &operator= (const simple_spinlock &) = delete;

	void lock() {
		while (true) {
			if (!__atomic_exchange_n(&lock_, true, __ATOMIC_ACQUIRE)) {
				return;
			}

			while (__atomic_load_n(&lock_, __ATOMIC_RELAXED)) {
				detail::loophint();
			}
		}
	}

	bool is_locked() {
		return __atomic_load_n(&lock_, __ATOMIC_RELAXED);
	}

	void unlock() {
		__atomic_store_n(&lock_, false, __ATOMIC_RELEASE);
	}

private:
	bool lock_;
};

} // namespace frg
