#ifndef FRG_ALLOCATION_HPP
#define FRG_ALLOCATION_HPP

#include <new>
#include <utility>

#include <frg/macros.hpp>

#include <stddef.h>

namespace frg FRG_VISIBILITY {

template<typename T, typename Allocator, typename... Args>
T *construct(Allocator &allocator, Args &&... args) {
	void *pointer = allocator.allocate(sizeof(T));
	return new(pointer) T(std::forward<Args>(args)...);
}
template<typename T, typename Allocator, typename... Args>
T *construct_n(Allocator &allocator, size_t n, Args &&... args) {
	T *pointer = (T *)allocator.allocate(sizeof(T) * n);
	for(size_t i = 0; i < n; i++)
		new(&pointer[i]) T(std::forward<Args>(args)...);
	return pointer;
}

template<typename T, typename Allocator>
void destruct(Allocator &allocator, T *pointer) {
	if(!pointer)
		return;
	pointer->~T();
	allocator.deallocate(pointer, sizeof(T));
}

template<typename T, typename Allocator>
void destruct_n(Allocator &allocator, T *pointer, size_t n) {
	if(!pointer)
		return;
	for(size_t i = 0; i < n; i++)
		pointer[i].~T();
	allocator.deallocate(pointer, sizeof(T) * n);
}

template<typename Allocator>
struct unique_memory {
	friend void swap(unique_memory &a, unique_memory &b) {
		using std::swap;
		swap(a.pointer_, b.pointer_);
		swap(a.size_, b.size_);
		swap(a.allocator_, b.allocator_);
	}

	unique_memory()
	: pointer_{nullptr}, size_{0}, allocator_{nullptr} { }

	explicit unique_memory(Allocator &allocator, size_t size)
	: size_{size}, allocator_{&allocator} {
		pointer_ = allocator_->allocate(size);
	}

	unique_memory(unique_memory &&other)
	: unique_memory{} {
		swap(*this, other);
	}

	unique_memory(const unique_memory &other) = delete;

	~unique_memory() {
		if(pointer_)
			allocator_->free(pointer_);
	}

	explicit operator bool () {
		return pointer_;
	}

	unique_memory &operator= (unique_memory other) {
		swap(*this, other);
		return *this;
	}

	void *data() const {
		return pointer_;
	}

	size_t size() const {
		return size_;
	}

private:
	void *pointer_;
	size_t size_;
	Allocator *allocator_;
};

} // namespace frg

#endif // FRG_ALLOCATION_HPP
