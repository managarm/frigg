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

private:
	T *p_;
	size_t n_;
};

} // namespace frg
