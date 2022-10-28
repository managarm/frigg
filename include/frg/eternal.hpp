#ifndef FRG_ETERNAL_HPP
#define FRG_ETERNAL_HPP

#include <new>
#include <utility>
#include <algorithm>

#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

template<size_t Size, size_t Align>
struct alignas(Align) aligned_storage {
	constexpr aligned_storage()
	: buffer{0} { }

	char buffer[Size];
};

template <typename ...T>
using aligned_union = aligned_storage<
	std::max({sizeof(T)...}),
	std::max({alignof(T)...})
>;

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

