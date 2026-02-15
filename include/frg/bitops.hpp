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
	static constexpr int ffs(unsigned long long x) {
		return __builtin_ffsll(x);
	}

	static constexpr int clz(unsigned long long x) {
		return __builtin_clzll(x);
	}
};

template<>
struct bitops_impl<unsigned long> {
	static constexpr int ffs(unsigned long x) {
		return __builtin_ffsl(x);
	}

	static constexpr int clz(unsigned long x) {
		return __builtin_clzl(x);
	}
};

template<>
struct bitops_impl<unsigned int> {
	static constexpr int ffs(unsigned int x) {
		return __builtin_ffs(x);
	}

	static constexpr int clz(unsigned int x) {
		return __builtin_clz(x);
	}
};

template<typename T>
requires std::is_unsigned_v<T>
constexpr int ffs(T x) {
	return bitops_impl<T>::ffs(x);
}

template<typename T>
requires std::is_unsigned_v<T>
constexpr int clz(T x) {
	return bitops_impl<T>::clz(x);
}

template<typename T>
constexpr int floor_log2(T x) {
	using U = std::make_unsigned_t<T>;
	FRG_ASSERT(x > 0);
	return sizeof(U) * CHAR_BIT - 1 - clz<U>(static_cast<U>(x));
}

static_assert(floor_log2(7) == 2);
static_assert(floor_log2(8) == 3);
static_assert(floor_log2(9) == 3);

static_assert(floor_log2(1) == 0);
static_assert(floor_log2(2) == 1);
static_assert(floor_log2(3) == 1);
static_assert(floor_log2(uint32_t{1} << 31) == 31);
static_assert(floor_log2((uint32_t{1} << 31) + 1) == 31);
static_assert(floor_log2(~uint32_t{1}) == 31);

template<typename T>
constexpr int ceil_log2(T x) {
	using U = std::make_unsigned_t<T>;
	FRG_ASSERT(x > 0);
	if (x == 1)
		return 0;
	return sizeof(U) * CHAR_BIT - clz<U>(static_cast<U>(x - 1));
}

static_assert(ceil_log2(7) == 3);
static_assert(ceil_log2(8) == 3);
static_assert(ceil_log2(9) == 4);

static_assert(ceil_log2(1) == 0);
static_assert(ceil_log2(2) == 1);
static_assert(ceil_log2(3) == 2);
static_assert(ceil_log2(uint32_t{1} << 31) == 31);
static_assert(ceil_log2((uint32_t{1} << 31) + 1) == 32);
static_assert(ceil_log2(~uint32_t{1}) == 32);

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
