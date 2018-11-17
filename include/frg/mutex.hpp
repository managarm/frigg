#ifndef FRG_MUTEX_HPP
#define FRG_MUTEX_HPP

#include <utility>
#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

struct dont_lock_t { };

constexpr dont_lock_t dont_lock = dont_lock_t();

template<typename Mutex>
class unique_lock {
public:
	friend void swap(unique_lock &u, unique_lock &v) {
		using std::swap;
		swap(u._mutex, v._mutex);
		swap(u._is_locked, v._is_locked);
	}

	unique_lock()
	: _mutex{nullptr}, _is_locked{false} { }

	unique_lock(dont_lock_t, Mutex &mutex)
	: _mutex{&mutex}, _is_locked{false} { }

	unique_lock(Mutex &mutex)
	: _mutex{&mutex}, _is_locked{false} {
		lock();
	}

	unique_lock(const unique_lock &other) = delete;
	
	unique_lock(unique_lock &&other)
	: unique_lock() {
		swap(*this, other);
	}

	~unique_lock() {
		if(_is_locked)
			unlock();
	}

	unique_lock &operator= (unique_lock other) {
		swap(*this, other);
		return *this;
	}

	void lock() {
		FRG_ASSERT(!_is_locked);
		_mutex->lock();
		_is_locked = true;
	}

	void unlock() {
		FRG_ASSERT(_is_locked);
		_mutex->unlock();
		_is_locked = false;
	}

	bool is_locked() {
		return _is_locked;
	}

	bool protects(Mutex *mutex) {
		return _is_locked && mutex == _mutex;
	}

private:
	Mutex *_mutex;
	bool _is_locked;
};

template<typename Mutex>
unique_lock<Mutex> guard(Mutex *mutex) {
	return unique_lock<Mutex>(mutex);
}

template<typename Mutex>
unique_lock<Mutex> guard(dont_lock_t, Mutex *mutex) {
	return unique_lock<Mutex>(dont_lock, mutex);
}

} // namespace frg

#endif // FRG_MUTEX_HPP
