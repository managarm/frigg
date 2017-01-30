#ifndef FRG_INTRUSIVE_HPP
#define FRG_INTRUSIVE_HPP

#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

template<typename T, typename OwnerPointer, typename BorrowPointer>
struct intrusive_traits;

template<typename T>
struct intrusive_traits<T, T *, T *> {
	static T *decay(T *owner) {
		return owner;
	}
};

template<typename T, typename H, H T::*Member>
struct locate_member {
	H &operator() (T &x) {
		return x.*Member;
	}
};

} // namespace frg

#endif // FRG_INTRUSIVE_HPP
