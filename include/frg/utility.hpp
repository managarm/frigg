#ifndef FRG_UTILITY_HPP
#define FRG_UTILITY_HPP

#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

template<typename Tag, typename T>
struct composition : private T {
	static T &get(composition<Tag, T> *p) {
		return *static_cast<T *>(p);
	}
};

template<typename Tag, typename T>
T &get(composition<Tag, T> *p) {
	return composition<Tag, T>::get(p);
}

} // namespace frg

#endif // FRG_UTILITY_HPP
