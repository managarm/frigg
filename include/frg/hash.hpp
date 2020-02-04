#ifndef FRG_HASH_HPP
#define FRG_HASH_HPP

#include <stdint.h>
#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

template<typename T>
class hash;

template<>
class hash<uint64_t> {
public:
	unsigned int operator() (uint64_t v) const {
		static_assert(sizeof(unsigned int) == 4, "Expected sizeof(int) == 4");
		return (unsigned int)(v ^ (v >> 32));
	}
};

template<>
class hash<int64_t> {
public:
	unsigned int operator() (int64_t v) const {
		static_assert(sizeof(unsigned int) == 4, "Expected sizeof(int) == 4");
		return (unsigned int)(v ^ (v >> 32));
	}
};

template<>
class hash<int> {
public:
	unsigned int operator() (int v) const {
		return v;
	}
};

template<typename T>
class hash<T *> {
public:
	unsigned int operator() (T *p) const {
		return reinterpret_cast<uintptr_t>(p);
	}
};

class CStringHash {
public:
	unsigned int operator() (const char *str) const {
		unsigned int value = 0;
		while(*str != 0) {
			value = (value << 8) | (value >> 24);
			value += *str++;
		}
		return value;
	}
};

} // namespace frg

#endif // FRG_HASH_HPP
