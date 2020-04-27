#ifndef FRG_ARRAY_HPP
#define FRG_ARRAY_HPP

#include <stddef.h>
#include <utility>
#include <type_traits>

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
