#pragma once

#include <concepts>
#include <stdint.h>
#include <limits.h>
#include <type_traits>

#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

template<typename T>
struct bitops_impl { };

template<>
struct bitops_impl<unsigned long long> {
	static constexpr int clz(unsigned long long x) {
		return __builtin_clzll(x);
	}
};

template<>
struct bitops_impl<unsigned long> {
	static constexpr int clz(unsigned long x) {
		return __builtin_clzl(x);
	}
};

template<>
struct bitops_impl<unsigned int> {
	static constexpr int clz(unsigned int x) {
		return __builtin_clz(x);
	}
};

template<typename T>
constexpr int clz(T x) {
	using U = std::make_unsigned_t<T>;
	return bitops_impl<U>::clz(static_cast<U>(x));
}

template<typename T>
constexpr int floor_log2(T x) {
	FRG_ASSERT(x > 0);
	return sizeof(T) * CHAR_BIT - 1 - clz(x);
}

static_assert(floor_log2(7) == 2);
static_assert(floor_log2(8) == 3);
static_assert(floor_log2(9) == 3);

template<typename T>
constexpr int ceil_log2(T x) {
	FRG_ASSERT(x > 0);
	return sizeof(T) * CHAR_BIT - clz(x - 1);
}

static_assert(ceil_log2(7) == 3);
static_assert(ceil_log2(8) == 3);
static_assert(ceil_log2(9) == 4);

// `alignment` must be a power of 2.
template <std::integral T>
constexpr T align_down(T value, auto alignment) {
	return static_cast<T>(value & ~(static_cast<T>(alignment) - 1));
}

// `alignment` must be a power of 2.
template <std::integral T>
constexpr T align_up(T value, auto alignment) {
	return align_down(static_cast<T>(value + alignment - 1), alignment);
}

} // namespace frg
