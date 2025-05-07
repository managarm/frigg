#pragma once

#include <stddef.h>

#include <frg/macros.hpp>
#include <frg/array.hpp>

namespace frg FRG_VISIBILITY {

template<typename T>
struct span {
	span()
	: p_{nullptr}, n_{0} { }

	span(T *p, size_t n)
	: p_{p}, n_{n} { }

	template<typename U, size_t N>
	span(const array<U, N> &arr)
	: p_{arr.data()}, n_{N} { }

	template<typename U, size_t N>
	span(array<U, N> &arr)
	: p_{arr.data()}, n_{N} { }

	T *data() const {
		return p_;
	}

	T *begin() const {
		return p_;
	}

	T *end() const {
		return p_ + n_;
	}

	size_t size() const {
		return n_;
	}

	T &operator[] (size_t i) const {
		return p_[i];
	}

	span subspan(size_t disp) {
		FRG_ASSERT(disp <= n_);
		return {p_ + disp, n_ - disp};
	}

	span subspan(size_t disp, size_t length) {
		FRG_ASSERT(disp + length <= n_);
		return {p_ + disp, length};
	}

private:
	T *p_;
	size_t n_;
};

} // namespace frg
