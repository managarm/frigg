#ifndef FRG_HASH_HPP
#define FRG_HASH_HPP

#include <stdint.h>
#include <cstddef>
#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

constexpr uint64_t MurmurHash2_64A(const void *key, uint64_t len, uint64_t seed = 0xC70F6907UL)
{
    const uint64_t m = 0xC6A4A7935BD1E995;
    const int r = 47;

    uint64_t h = seed ^ (len * m);

    const uint64_t *data = static_cast<const uint64_t*>(key);
    const uint64_t *end = data + (len / 8);

    while (data != end)
    {
        uint64_t k = 0;
        memcpy(&k, data++, sizeof(uint64_t));

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    auto data2 = static_cast<const uint8_t*>(static_cast<const void*>(data));

    switch (len & 7)
    {
        case 7: h ^= static_cast<uint64_t>(data2[6]) << 48;
        case 6: h ^= static_cast<uint64_t>(data2[5]) << 40;
        case 5: h ^= static_cast<uint64_t>(data2[4]) << 32;
        case 4: h ^= static_cast<uint64_t>(data2[3]) << 24;
        case 3: h ^= static_cast<uint64_t>(data2[2]) << 16;
        case 2: h ^= static_cast<uint64_t>(data2[1]) << 8;
        case 1: h ^= static_cast<uint64_t>(data2[0]);
            h *= m;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

template<typename type>
struct hash;

template<>
struct hash<const char*>
{
	size_t operator()(const char *str) const
	{
		return MurmurHash2_64A(str, strlen(str));
	}
};

template<>
struct hash<char*>
{
	size_t operator()(char *str) const
	{
		return MurmurHash2_64A(str, strlen(str));
	}
};

template<typename type>
struct hash<type*>
{
	size_t operator()(type *ptr) const
	{
		return reinterpret_cast<size_t>(ptr);
	}
};

template<>
struct hash<std::nullptr_t>
{
	size_t operator()(std::nullptr_t) const
	{
		return 0;
	}
};

#define DEFINE_HASH(type)                    \
	template<>                               \
	struct hash<type>                        \
	{                                        \
		size_t operator()(type val) const    \
		{                                    \
			return static_cast<size_t>(val); \
		}                                    \
	};

DEFINE_HASH(bool)
DEFINE_HASH(char)
DEFINE_HASH(signed char)
DEFINE_HASH(unsigned char)
DEFINE_HASH(char16_t)
DEFINE_HASH(char32_t)
DEFINE_HASH(short)
DEFINE_HASH(int)
DEFINE_HASH(long)
DEFINE_HASH(long long)
DEFINE_HASH(unsigned short)
DEFINE_HASH(unsigned int)
DEFINE_HASH(unsigned long)
DEFINE_HASH(unsigned long long)

#undef DEFINE_HASH

} // namespace frg

#endif // FRG_HASH_HPP
