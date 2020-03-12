#ifndef FRG_UNIQUE_HPP
#define FRG_UNIQUE_HPP

#include <utility>

namespace frg {

template <typename T, typename Allocator>
struct unique_ptr {
	friend void swap(unique_ptr &a, unique_ptr &b) {
		using std::swap;
		swap(a._ptr, b._ptr);
		swap(a._allocator, b._allocator);
	}

	unique_ptr(Allocator allocator)
	:_ptr{nullptr}, _allocator(std::move(allocator)) {}

	unique_ptr(Allocator allocator, T *ptr)
	:_ptr{ptr}, _allocator(std::move(allocator)) {}

	~unique_ptr() {
		if (_ptr)
			_allocator.free(_ptr);
	}

	unique_ptr(const unique_ptr &) = delete;
	unique_ptr &operator=(const unique_ptr &) = delete;

	unique_ptr(unique_ptr &&p) {
		swap(*this, p);
	}

	unique_ptr &operator=(unique_ptr &&p) {
		swap(*this, p);
		return *this;
	}

	T *get() {
		return _ptr;
	}

	T &operator*() {
		return *_ptr;
	}

	T *operator->() {
		return _ptr;
	}

	T *release() {
		T *old = _ptr;
		_ptr = nullptr;

		return old;
	}

	operator bool() {
		return _ptr;
	}

	void reset(T *ptr) {
		T *old = _ptr;
		_ptr = ptr;

		if (old)
			_allocator.free(old);
	}

private:
	T *_ptr;
	Allocator _allocator;
};

template <typename T, typename Allocator, typename ...Args>
unique_ptr<T, Allocator> make_unique(Allocator allocator, Args &&...args) {
	T *ptr = new (allocator.allocate(sizeof(T))) T{
			std::forward<Args>(args)...};
	return unique_ptr<T, Allocator>{std::move(allocator), ptr};
}

}

#endif //FRG_UNIQUE_HPP
