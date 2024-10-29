#ifndef FRG_UNIQUE_HPP
#define FRG_UNIQUE_HPP

#include <type_traits>
#include <utility>
#include <concepts>

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

	/** Converts a unique_ptr of a different type to the type of this
	    unique_ptr.

	    The allocator is taken from the old unique_ptr.  It must be
	    convertible to our allocator type.

	    The pointer type also, naturally, has to be convertible to the
	    current pointer type.
	 */
	template<typename U, typename UAlloc>
	requires (
		std::is_convertible_v<U*, T*>
		&& std::is_constructible_v<Allocator, UAlloc&&>
	)
	unique_ptr(unique_ptr<U, UAlloc>&& o)
	: _ptr { nullptr }, _allocator { std::move(o._allocator) } {
		reset(o.release());
	}

	/** \sa unique_ptr(unique_ptr<U, UAlloc>&&)  */
	template<typename U, typename UAlloc>
	requires std::constructible_from<unique_ptr, unique_ptr<U, UAlloc>&&>
	unique_ptr& operator=(unique_ptr<U, UAlloc>&& o) {
		auto optr = o.release();
		reset(optr);
		_allocator = std::forward<UAlloc>(o._allocator);
		return *this;
	}


	~unique_ptr() {
		reset();
	}

	unique_ptr(const unique_ptr &) = delete;
	unique_ptr &operator=(const unique_ptr &) = delete;

	unique_ptr(unique_ptr &&p)
	:_ptr{nullptr}, _allocator(p._allocator) {
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

	explicit operator bool() {
		return _ptr;
	}

	void reset(T *ptr = nullptr) {
		T *old = _ptr;
		_ptr = ptr;

		if (old)
			_allocator.free(old);
	}

private:
	T *_ptr;
	Allocator _allocator;

	template<typename U, typename UA>
	friend struct unique_ptr;
};

template <typename T, typename Allocator, typename ...Args>
unique_ptr<T, Allocator> make_unique(Allocator allocator, Args &&...args) {
	T *ptr = new (allocator.allocate(sizeof(T))) T{
			std::forward<Args>(args)...};
	return unique_ptr<T, Allocator>{std::move(allocator), ptr};
}

}

#endif //FRG_UNIQUE_HPP
