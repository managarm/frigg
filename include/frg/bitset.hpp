#ifndef FRG_BITSET_HPP
#define FRG_BITSET_HPP
#include <cstddef>
#include <cstdint>

namespace frg
{
    namespace detail
    {
        constexpr auto div_roundup(auto v1, auto v2) { return (v1 + v2 - 1) / v2; }
    } // namespace detail
    template <size_t N>
    class bitset
    {
        inline static constexpr auto buffer_size = detail::div_roundup(N, 64);
        uint64_t buffer[buffer_size];

    public:
        // bit reference
        class reference
        {
            friend class bitset;
            reference() noexcept;

            size_t index;
            bitset& s;

            constexpr reference(size_t index, bitset& r) : index(index), s(r) {}

        public:
            reference(const reference&) = default;
            ~reference() = default;

            reference& operator=(bool x) noexcept
            {
                s.set(index, x);
                return *this;
            }

            reference& operator=(const reference& x) noexcept
            {
                s.set(index, x);
                return *this;
            }

            bool operator~() const noexcept { return ~s[index]; }

            operator bool() const noexcept { return s[index]; }

            reference& flip() noexcept
            {
                s.flip(index);
                return *this;
            }
        };

        constexpr bitset() noexcept = default;
        constexpr bitset(unsigned long long val) noexcept { buffer[0] = val; }

        bitset& operator&=(const bitset& rhs) noexcept
        {
            for (size_t i = 0; i < buffer_size; i++)
                buffer[i] &= rhs.buffer[i];
            return *this;
        }

        bitset& operator|=(const bitset& rhs) noexcept
        {
            for (size_t i = 0; i < buffer_size; i++)
                buffer[i] |= rhs.buffer[i];
            return *this;
        }

        bitset& operator^=(const bitset& rhs) noexcept
        {
            for (size_t i = 0; i < buffer_size; i++)
                buffer[i] ^= rhs.buffer[i];
            return *this;
        }
        bitset& operator<<=(size_t pos) noexcept
        {
            if (pos != 0)
            {
                size_t wshift = pos / 64;
                size_t offset = pos % 64;
                if (offset == 0)
                    for (size_t i = N - 1; i >= wshift; --i)
                        buffer[i] = buffer[i - wshift];
                else
                {
                    const size_t soffset = (64 - offset);
                    for (size_t i = N - 1; i > wshift; --i)
                        buffer[i] = ((buffer[i - wshift] << offset) | (buffer[i - wshift - 1] >> soffset));
                    buffer[wshift] = buffer[0] << offset;
                }

                for (auto i = buffer + 0; i < buffer + wshift; i++)
                    *i = 0;
            }

            return *this;
        }

        bitset& operator>>=(size_t pos) noexcept
        {
            if (pos != 0)
            {
                const size_t wshift = pos / 64;
                const size_t offset = pos % 64;
                const size_t s = N - wshift - 1;

                if (offset == 0)
                    for (size_t i = 0; i <= s; ++i)
                        buffer[i] = buffer[i + wshift];
                else
                {
                    const size_t off = (64 - offset);
                    for (size_t i = 0; i < s; ++i)
                        buffer[i] = ((buffer[i + wshift] >> offset) | (buffer[i + wshift + 1] << off));
                    buffer[s] = buffer[N - 1] >> offset;
                }

                for (auto i = buffer + s + 1; i < buffer + N; i++)
                    *i = 0;
            }

            return *this;
        }

        bitset& set() noexcept
        {
            for (auto& i : buffer)
                i = ~0;
            return *this;
        }

        bitset& set(size_t pos, bool val = true)
        {
            set_bit(buffer[pos / 64], val, pos % 64);
            return *this;
        }

        bitset& reset() noexcept
        {
            for (auto& i : buffer)
                i = 0;
            return *this;
        }

        bitset& reset(size_t pos) { return set(pos, false); }

        bitset operator~() const noexcept
        {
            auto copy = *this;
            copy.flip();
            return copy;
        }

        bitset& flip() noexcept
        {
            for (auto& i : buffer)
                i = ~i;
            return *this;
        }

        bitset& flip(size_t pos) { return set(pos, !this->operator[](pos)); }

        // element access
        constexpr bool operator[](size_t pos) const { return get_bit(buffer[pos / 64], pos % 64); }

        reference operator[](size_t pos) { return reference(pos, *this); }

        size_t count() const noexcept
        {
            size_t n = 0;
            for (size_t i = 0; i < buffer_size - 1; i++)
                n += __builtin_popcountll(buffer[i]);
            return n + __builtin_popcountll(get_bits(buffer[buffer_size - 1], 0, N % 64));
        }

        constexpr size_t size() const noexcept { return N; }
        bool operator==(const bitset& rhs) const noexcept
        {
            for (size_t i = 0; i < buffer_size - 1; i++)
                if (buffer[i] != rhs.buffer[i])
                    return false;
            return get_bits(buffer[buffer_size - 1], 0, N % 64) == get_bits(rhs.buffer[buffer_size - 1], 0, N % 64);
        }

        bool test(size_t pos) const { return this->operator[](pos); }

        bool all() const noexcept
        {
            bool value = true;
            for (size_t i = 0; i < N / 64; i++)
                value &= (buffer[i] == 0xffffffffffffffff);
            if (N % 64)
                value &= (buffer[N / 64] & ((1 << (N % 64)) - 1)) == (1 << (N % 64)) - 1;
            return value;
        }

        bool any() const noexcept
        {
            bool value = true;
            for (size_t i = 0; i < N / 64; i++)
                value |= (buffer[i] == 0xffffffffffffffff);
            if (N % 64)
                value |= (buffer[N / 64] & ((1 << (N % 64)) - 1));
            return value;
        }

        bool none() const noexcept
        {
            bool value = true;
            for (size_t i = 0; i < N / 64; i++)
                value &= !(buffer[i]);
            if (N % 64)
                value &= !(buffer[N / 64] & ((1 << (N % 64)) - 1));

            return value;
        }

        bitset operator<<(size_t pos) const noexcept
        {
            bitset bs = *this;
            bs <<= pos;
            return bs;
        }

        bitset operator>>(size_t pos) const noexcept
        {
            bitset bs = *this;
            bs >>= pos;
            return bs;
        }
    };

    template <size_t N>
    bitset<N> operator&(const bitset<N>& lhs, const bitset<N>& rhs) noexcept
    {
        bitset<N> b = lhs;
        b &= rhs;
        return b;
    }

    template <size_t N>
    bitset<N> operator|(const bitset<N>& lhs, const bitset<N>& rhs)
    {
        bitset<N> b = lhs;
        b |= rhs;
        return b;
    }

    template <size_t N>
    bitset<N> operator^(const bitset<N>& lhs, const bitset<N>& rhs)
    {
        bitset<N> b = lhs;
        b ^= rhs;
        return b;
    }
} // namespace frg

#endif
