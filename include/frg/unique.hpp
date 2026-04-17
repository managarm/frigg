#ifndef FRG_UNIQUE_HPP
#define FRG_UNIQUE_HPP

#include <utility>

#include <frg/allocation.hpp>

namespace frg {

template <typename T, typename Allocator>
struct unique_ptr {
	friend constexpr void swap(unique_ptr &a, unique_ptr &b) {
		using std::swap;
		swap(a._ptr, b._ptr);
		swap(a._allocator, b._allocator);
	}

	constexpr unique_ptr(Allocator allocator)
	:_ptr{nullptr}, _allocator(std::move(allocator)) {}

	constexpr unique_ptr(Allocator allocator, T *ptr)
	:_ptr{ptr}, _allocator(std::move(allocator)) {}

	~unique_ptr() {
		frg::destruct(_allocator, _ptr);
	}

	unique_ptr(const unique_ptr &) = delete;
	unique_ptr &operator=(const unique_ptr &) = delete;

	constexpr unique_ptr(unique_ptr &&p)
	:_ptr{nullptr}, _allocator(p._allocator) {
		swap(*this, p);
	}

	constexpr unique_ptr &operator=(unique_ptr &&p) {
		swap(*this, p);
		return *this;
	}

	constexpr T *get() {
		return _ptr;
	}

	constexpr T &operator*() {
		return *_ptr;
	}

	constexpr T *operator->() {
		return _ptr;
	}

	constexpr T *release() {
		T *old = _ptr;
		_ptr = nullptr;

		return old;
	}

	constexpr explicit operator bool() {
		return _ptr;
	}

	constexpr void reset(T *ptr) {
		T *old = _ptr;
		_ptr = ptr;

		frg::destruct(_allocator, old);
	}

private:
	T *_ptr;
	Allocator _allocator;
};

template <typename T, typename Allocator, typename ...Args>
unique_ptr<T, Allocator> make_unique(Allocator allocator, Args &&...args) {
	T *ptr = frg::construct<T>(allocator, std::forward<Args>(args)...);
	return unique_ptr<T, Allocator>{std::move(allocator), ptr};
}

}

#endif //FRG_UNIQUE_HPP
