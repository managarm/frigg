#ifndef FRG_CONTAINER_OF_HPP
#define FRG_CONTAINER_OF_HPP

#include <stdint.h>
//#include <string.h>
#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

template<typename T, typename C>
C *container_of(T *p, T C::*mptr) {
	static_assert(sizeof(T C::*) == sizeof(uintptr_t), "Broken ABI");

	uintptr_t offset;
	memcpy(&offset, &mptr, sizeof(uintptr_t));
	auto r = reinterpret_cast<char *>(p);
	return reinterpret_cast<C *>(r - offset);
}

} // namespace frg

#endif // FRG_CONTAINER_OF_HPP
