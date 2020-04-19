#pragma once

#include <utility>

#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

template<auto Ptr>
struct bound_mem_fn;

template<typename C, typename R, typename... Args, R (C:: *Ptr) (Args...)>
struct bound_mem_fn<Ptr> {
	bound_mem_fn(C *object)
	: object_{object} { }

	template<typename... X>
	R operator() (X &&... args) {
		return (object_->*Ptr)(std::forward<X>(args)...);
	}

private:
	C *object_;
};

} // namespace frg
