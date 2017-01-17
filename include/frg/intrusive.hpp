#ifndef FRG_INTRUSIVE_HPP
#define FRG_INTRUSIVE_HPP

#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

template<typename T, typename H, H T::*Member>
struct locate_member {
	H &operator() (T *x) {
		return x->*Member;
	}
};

} // namespace frg

#endif // FRG_INTRUSIVE_HPP
