#ifndef FRG_ARRAY_HPP
#define FRG_ARRAY_HPP

#include <stddef.h>
#include <utility>
#include <type_traits>
#include <tuple>

namespace frg {

template<class T, size_t N>
class array {
public:
	using value_type = T;
	using size_type = size_t;
	using reference = value_type&;
	using const_reference = const value_type&;
	using pointer = value_type*;
	using const_pointer = const value_type*;
	using iterator = pointer;
	using const_iterator = const_pointer;
	// the actual array
	value_type _stor[N];

	friend void swap(array &a, array &b) {
	    for (size_t i = 0; i < N; i++)
		std::swap(a._stor[i], b._stor[i]);
	}

	constexpr reference operator[](size_type pos) {
		return _stor[pos];
	}
	constexpr const_reference operator[](size_type pos) const {
		return _stor[pos];
	}

	constexpr reference front() {
		return _stor[0];
	}
	constexpr const_reference front() const {
		return _stor[0];
	}

	constexpr reference back() {
		return _stor[N];
	}
	constexpr const_reference back() const {
		return _stor[N];
	}

	constexpr iterator begin() noexcept {
		return &_stor[0];
	}
	constexpr const_iterator begin() const noexcept {
		return &_stor[0];
	}
	constexpr const_iterator cbegin() const noexcept {
		return &_stor[0];
	}

	constexpr iterator end() noexcept {
		return &_stor[N];
	}
	constexpr const_iterator end() const noexcept {
		return &_stor[N];
	}
	constexpr const_iterator cend() const noexcept {
		return &_stor[N];
	}

	constexpr T* data() noexcept {
		return &_stor[0];
	}
	constexpr T* data() const noexcept {
		return &_stor[0];
	}

	constexpr bool empty() const noexcept {
		return N == 0;
	}

	constexpr size_type size() const noexcept {
		return N;
	}

	constexpr size_type max_size() const noexcept {
		// for arrays it is equal to size()
		return N;
	}
};

namespace details {
	template<typename ...Ts>
	struct concat_size;

	template<typename ...Ts>
	inline constexpr size_t concat_size_v = concat_size<Ts...>::value;

	template<typename T, typename ...Ts>
	struct concat_size<T, Ts...>
	: std::integral_constant<size_t, std::tuple_size_v<T> + concat_size_v<Ts...>> { };

	template<>
	struct concat_size<>
	: std::integral_constant<size_t, 0> { };

	template<typename X, size_t N>
	constexpr void concat_insert(frg::array<X, N> &, size_t) { }

	template<typename X, size_t N, typename T, typename... Ts>
	constexpr void concat_insert(frg::array<X, N> &res, size_t at, const T &other, const Ts &...tail) {
		size_t n = std::tuple_size_v<T>;
		for(size_t i = 0; i < n; ++i)
			res[at + i] = other[i];
		concat_insert(res, at + n, tail...);
	}
} // namespace details

template<typename X, typename ...Ts>
constexpr auto array_concat(const Ts &...arrays) {
	frg::array<X, details::concat_size_v<Ts...>> res{};
	details::concat_insert(res, 0, arrays...);
	return res;
}

} // namespace frg

namespace std {

template<size_t I, class T, size_t N>
constexpr T &get(frg::array<T, N> &a) noexcept {
	static_assert(I < N, "array index is not within bounds");
	return a[I];
};

template<size_t I, class T, size_t N>
constexpr T &&get(frg::array<T, N> &&a) noexcept {
	static_assert(I < N, "array index is not within bounds");
	return std::move(a[I]);
};

template<size_t I, class T, size_t N>
constexpr const T &get(const frg::array<T, N> &a) noexcept {
	static_assert(I < N, "array index is not within bounds");
	return a[I];
};

template<size_t I, class T, size_t N>
constexpr const T &&get(const frg::array<T, N> &&a) noexcept {
	static_assert(I < N, "array index is not within bounds");
	return std::move(a[I]);
};

template<class T, size_t N>
struct tuple_size<frg::array<T, N>> :
    integral_constant<size_t, N>
{ };

template<size_t I, class T, size_t N>
struct tuple_element<I, frg::array<T, N>> {
	using type = T;
};

} // namespace std

#endif // FRG_ARRAY_HPP
