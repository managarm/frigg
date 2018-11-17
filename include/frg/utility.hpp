#ifndef FRG_UTILITY_HPP
#define FRG_UTILITY_HPP

#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

template<typename T>
const T &min(const T &a, const T &b) {
	return (b < a) ? b : a;
}

template<typename T>
const T &max(const T &a, const T &b) {
	return (a < b) ? b : a;
}

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
