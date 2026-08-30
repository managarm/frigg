#ifndef FRG_ALIGNED_STORAGE_HPP
#define FRG_ALIGNED_STORAGE_HPP

#include <cstddef>
#include <algorithm>

#include <frg/macros.hpp>

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

} // namespace frg

#endif // FRG_ALIGNED_STORAGE_HPP
