#ifndef FRG_ALLOCATION_HPP
#define FRG_ALLOCATION_HPP

#include <new>
#include <utility>

#include <frg/macros.hpp>

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

} // namespace frg

#endif // FRG_ALLOCATION_HPP
