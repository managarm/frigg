#pragma once

#include <utility>

namespace frg {

template <typename Iter, typename Comp>
constexpr void insertion_sort(Iter begin, Iter end, Comp comp) {
	for (auto i = begin; i < end; ++i) {
		auto j = i;
		++j;

		for (; j < end; ++j) {
			if (comp(*i, *j)) {
				std::swap(*i, *j);
			}
		}
	}
}

} // namespace frg
