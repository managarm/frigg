#pragma once

#include <stddef.h>

#include <frg/macros.hpp>
#include <frg/array.hpp>

namespace frg FRG_VISIBILITY {

template<typename T>
struct span {
	constexpr span()
	: p_{nullptr}, n_{0} { }

	constexpr span(T *p, size_t n)
	: p_{p}, n_{n} { }

	template<typename U, size_t N>
	constexpr span(const array<U, N> &arr)
	: p_{arr.data()}, n_{N} { }

	template<typename U, size_t N>
	constexpr span(array<U, N> &arr)
	: p_{arr.data()}, n_{N} { }

	constexpr T *data() const {
		return p_;
	}

	constexpr T *begin() const {
		return p_;
	}

	constexpr T *end() const {
		return p_ + n_;
	}

	constexpr size_t size() const {
		return n_;
	}

	constexpr bool empty() const {
		return n_ == 0;
	}

	constexpr size_t size_bytes() const {
		return n_ * sizeof(T);
	}

	constexpr T &operator[] (size_t i) const {
		return p_[i];
	}

	template <typename Self>
	auto subspan(this Self&& self, size_t disp) {
		FRG_ASSERT(disp <= self.n_);
		return span{self.data() + disp, self.n_ - disp};
	}

	template <typename Self>
	auto subspan(this Self&& self, size_t disp, size_t length) {
		FRG_ASSERT(disp + length <= self.n_);
		return span{self.data() + disp, length};
	}

private:
	T *p_;
	size_t n_;
};

} // namespace frg
