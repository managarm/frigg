#pragma once

#include <stddef.h>

#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

template<typename T>
struct span {
	span(T *p, size_t n)
	: p_{p}, n_{n} { }

	T *data() const {
		return p_;
	}

	size_t size() const {
		return n_;
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
