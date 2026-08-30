#ifndef FRG_ETERNAL_HPP
#define FRG_ETERNAL_HPP

#include <new>
#include <cstddef>
#include <utility>

#include <frg/aligned_storage.hpp>
#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

// Container for an object that deletes the object's destructor.
// eternal<T> always has a trivial destructor.
template<typename T>
class eternal {
public:
	static_assert(
#if defined(__clang__) && __clang_major__ >= 15
		__is_trivially_destructible(aligned_storage<sizeof(T), alignof(T)>),
#else
		__has_trivial_destructor(aligned_storage<sizeof(T), alignof(T)>),
#endif
			"eternal<T> should have a trivial destructor");

	template<typename... Args>
	eternal(Args &&... args) {
		new (&_storage) T(std::forward<Args>(args)...);
	}

	T &get() {
		return *reinterpret_cast<T *>(&_storage);
	}

	T &operator*() {
		return *reinterpret_cast<T *>(&_storage);
	}
	T *operator->() {
		return reinterpret_cast<T *>(&_storage);
	}

private:
	aligned_storage<sizeof(T), alignof(T)> _storage;
};

} // namespace frg

#endif // FRG_ETERNAL_HPP

