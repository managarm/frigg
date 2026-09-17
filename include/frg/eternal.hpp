#ifndef FRG_ETERNAL_HPP
#define FRG_ETERNAL_HPP

#include <atomic>
#include <new>
#include <cstddef>
#include <utility>
#include <algorithm>
#include <type_traits>

#include <frg/macros.hpp>
#include <frg/mutex.hpp>

namespace frg FRG_VISIBILITY {

template<std::size_t Size, std::size_t Align>
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
		std::is_trivially_destructible_v<aligned_storage<sizeof(T), alignof(T)>>,
		"eternal<T> should have a trivial destructor"
	);

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

// Container for a lazily constructed object that deletes the object's destructor.
// lazy_eternal<T, M> always has a trivial destructor.
// lazy_eternal<T, M> always has a constexpr constructor if M does.
// The object is default-constructed on the first call to get().
template<typename T, typename Mutex>
class lazy_eternal {
public:
	// TODO: When eternal<T> becomes constexpr-constructible with C++26,
	//       we can replace Mutex by eternal<Mutex> to get rid of the trivial destructor requirement below.
	static_assert(
		std::is_trivially_destructible_v<aligned_storage<sizeof(T), alignof(T)>>
		&& std::is_trivially_destructible_v<Mutex>,
		"lazy_eternal<T> should have a trivial destructor"
	);

	constexpr lazy_eternal() = default;

	lazy_eternal(const lazy_eternal &) = delete;
	lazy_eternal &operator= (const lazy_eternal &) = delete;

	T &get() {
		if(!initialized_.load(std::memory_order_acquire)) {
			unique_lock lock{mutex_};
			if(!initialized_.load(std::memory_order_relaxed)) {
				new (&storage_.buffer) T{};
				initialized_.store(true, std::memory_order_release);
			}
		}

		return *std::launder(reinterpret_cast<T *>(&storage_.buffer));
	}

	T &operator* () {
		return get();
	}
	T *operator-> () {
		return &get();
	}

private:
	aligned_storage<sizeof(T), alignof(T)> storage_;
	std::atomic<bool> initialized_{false};
	Mutex mutex_;
};

} // namespace frg

#endif // FRG_ETERNAL_HPP

