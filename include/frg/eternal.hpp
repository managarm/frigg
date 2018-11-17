#ifndef FRG_ETERNAL_HPP
#define FRG_ETERNAL_HPP

#include <new>
#include <utility>

#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

template<size_t Size, size_t Align>
struct alignas(Align) aligned_storage {
	constexpr aligned_storage()
	: buffer{0} { }

	char buffer[Size];
};

// Container for an object that deletes the object's destructor.
// eternal<T> always has a trivial destructor.
template<typename T>
class eternal {
public:
	static_assert(__has_trivial_destructor(aligned_storage<sizeof(T), alignof(T)>),
			"eternal<T> should have a trivial destructor");

	template<typename... Args>
	eternal(Args &&... args) {
		new (&_storage) T(std::forward<Args>(args)...);
	}

	T &get() {
		return *reinterpret_cast<T *>(&_storage);
	}

private:
	aligned_storage<sizeof(T), alignof(T)> _storage;
};

} // namespace frg

#endif // FRG_ETERNAL_HPP

