#ifndef FRG_BITSET_HPP
#define FRG_BITSET_HPP
#include <cstddef>
#include <cstdint>
#include <climits>

namespace frg {
namespace detail {
constexpr std::size_t div_roundup(std::size_t v1, std::size_t v2) { return (v1 + v2 - 1) / v2; }
} // namespace detail

static_assert(CHAR_BIT == 8, "bits per byte must be 8");

template <size_t N>
class bitset {
	inline static constexpr auto buffer_size = detail::div_roundup(N, 64);
	inline static constexpr auto MASK_LAST_BIT = (1ull << (N % 64)) - 1;
	uint64_t buffer[buffer_size];

	constexpr void mask_last_bit() {
		if constexpr (N % 64)
			buffer[N / 64] &= MASK_LAST_BIT;
	}

public:
	// bit reference
	class reference {
		friend class bitset;
		constexpr reference() noexcept;

		size_t index;
		bitset &s;

		constexpr reference(size_t index, bitset &r) : index(index), s(r) {}

	public:
		constexpr reference(const reference &) = default;
		constexpr ~reference() = default;

		constexpr reference &operator=(bool x) noexcept {
			s.set(index, x);
			return *this;
		}

		constexpr reference &operator=(const reference &x) noexcept {
			s.set(index, x);
			return *this;
		}

		constexpr bool operator~() const noexcept { return ~s[index]; }

		constexpr operator bool() const noexcept { return s.test(index); }

		constexpr reference &flip() noexcept {
			s.flip(index);
			return *this;
		}
	};

	constexpr bitset() noexcept {
		for (auto &i : buffer)
			i = 0;
	}
	constexpr bitset(unsigned long long val) noexcept { buffer[0] = val; }

	constexpr bitset &operator&=(const bitset &rhs) noexcept {
		for (size_t i = 0; i < buffer_size; i++)
			buffer[i] &= rhs.buffer[i];
		return *this;
	}

	constexpr bitset &operator|=(const bitset &rhs) noexcept {
		for (size_t i = 0; i < buffer_size; i++)
			buffer[i] |= rhs.buffer[i];
		return *this;
	}

	constexpr bitset &operator^=(const bitset &rhs) noexcept {
		for (size_t i = 0; i < buffer_size; i++)
			buffer[i] ^= rhs.buffer[i];
		return *this;
	}
	constexpr bitset &operator<<=(size_t pos) noexcept {
		if (pos != 0) {
			size_t wshift = pos / 64;
			size_t offset = pos % 64;
			if (offset == 0)
				for (size_t i = buffer_size - 1; i >= wshift; --i)
					buffer[i] = buffer[i - wshift];
			else {
				const size_t soffset = (64 - offset);
				for (size_t i = buffer_size - 1; i > wshift; --i)
					buffer[i] = ((buffer[i - wshift] << offset) | (buffer[i - wshift - 1] >> soffset));
				buffer[wshift] = buffer[0] << offset;
			}

			for (auto i = buffer + 0; i < buffer + wshift; i++)
				*i = 0;
		}

		mask_last_bit();

		return *this;
	}

	constexpr bitset &operator>>=(size_t pos) noexcept {
		if (pos != 0) {
			const size_t wshift = pos / 64;
			const size_t offset = pos % 64;
			const size_t s = buffer_size - wshift - 1;

			if (offset == 0)
				for (size_t i = 0; i <= s; ++i)
					buffer[i] = buffer[i + wshift];
			else {
				const size_t off = (64 - offset);
				for (size_t i = 0; i < s; ++i)
					buffer[i] = ((buffer[i + wshift] >> offset) | (buffer[i + wshift + 1] << off));
				buffer[s] = buffer[buffer_size - 1] >> offset;
			}

			for (auto i = buffer + s + 1; i < buffer + buffer_size; i++)
				*i = 0;
		}

		mask_last_bit();

		return *this;
	}

	constexpr bitset &set() noexcept {
		for (auto &i : buffer)
			i = ~0;
		mask_last_bit();
		return *this;
	}

	constexpr bitset &set(size_t pos, bool val = true) {
		buffer[pos / 64] = (buffer[pos / 64] & (~(1ull << (pos % 64)))) | ((uint64_t)val << (pos % 64));
		return *this;
	}

	constexpr bitset &reset() noexcept {
		for (auto &i : buffer)
			i = 0;
		return *this;
	}

	constexpr bitset &reset(size_t pos) { return set(pos, false); }

	constexpr bitset operator~() const noexcept {
		auto copy = *this;
		copy.flip();
		return copy;
	}

	constexpr bitset &flip() noexcept {
		for (auto &i : buffer)
			i = ~i;
		mask_last_bit();
		return *this;
	}

	constexpr bitset &flip(size_t pos) { return set(pos, !test(pos)); }

	// element access
	constexpr bool operator[](size_t pos) const { return buffer[pos / 64] & (1ull << pos % 64); }

	constexpr reference operator[](size_t pos) { return reference(pos, *this); }

	constexpr size_t count() const noexcept {
		size_t n = 0;
		for (size_t i = 0; i < buffer_size - 1; i++)
			n += __builtin_popcountll(buffer[i]);
		return n + __builtin_popcountll(buffer[buffer_size - 1]);
	}

	constexpr size_t size() const noexcept { return N; }
	constexpr bool operator==(const bitset &rhs) const noexcept {
		for (size_t i = 0; i < buffer_size - 1; i++)
			if (buffer[i] != rhs.buffer[i])
				return false;
		return buffer[buffer_size - 1] == rhs.buffer[buffer_size - 1];
	}

	constexpr bool test(size_t pos) const { return this->operator[](pos); }

	constexpr bool all() const noexcept {
		bool value = true;
		for (size_t i = 0; i < N / 64; i++)
			value &= (buffer[i] == 0xffffffffffffffff);
		if (N % 64)
			value &= (buffer[N / 64] == MASK_LAST_BIT);
		return value;
	}

	constexpr bool any() const noexcept {
		bool value = false;
		for (size_t i = 0; i < N / 64; i++)
			value |= buffer[i];
		if (N % 64)
			value |= buffer[N / 64];
		return value;
	}

	constexpr bool none() const noexcept {
		bool value = true;
		for (size_t i = 0; i < N / 64; i++)
			value &= !(buffer[i]);
		if (N % 64)
			value &= !(buffer[N / 64] & ((1ull << (N % 64)) - 1));

		return value;
	}

	constexpr bitset operator<<(size_t pos) const noexcept {
		bitset bs = *this;
		bs <<= pos;
		return bs;
	}

	constexpr bitset operator>>(size_t pos) const noexcept {
		bitset bs = *this;
		bs >>= pos;
		return bs;
	}
};

template <size_t N>
constexpr bitset<N> operator&(const bitset<N> &lhs, const bitset<N> &rhs) noexcept {
	bitset<N> b = lhs;
	b &= rhs;
	return b;
}

template <size_t N>
constexpr bitset<N> operator|(const bitset<N> &lhs, const bitset<N> &rhs) {
	bitset<N> b = lhs;
	b |= rhs;
	return b;
}

template <size_t N>
constexpr bitset<N> operator^(const bitset<N> &lhs, const bitset<N> &rhs) {
	bitset<N> b = lhs;
	b ^= rhs;
	return b;
}
} // namespace frg

#endif
