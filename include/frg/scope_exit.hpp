#pragma once

#include <utility>

#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

template<typename Fn>
struct scope_exit {
	constexpr explicit scope_exit(Fn fn)
	: fn_{std::move(fn)}, active_{true} {}

	scope_exit(const scope_exit &) = delete;
	scope_exit operator=(const scope_exit &) = delete;

	constexpr ~scope_exit() {
		if (active_)
			fn_();
	}

	constexpr void release() {
		active_ = false;
	}

private:
	Fn fn_;
	bool active_;
};

} // namespace frg
