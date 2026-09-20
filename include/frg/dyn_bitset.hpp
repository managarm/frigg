#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <iterator>
#include <ranges>
#include <utility>

#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

// Set of indices in [0, size()). The size is fixed at construction.
// Only construction and copying allocate; all other operations work in-place.
template<typename Allocator>
class dyn_bitset {
	using word_type = uint64_t;
	static constexpr size_t word_bits = 64;

public:
	static constexpr size_t npos = static_cast<size_t>(-1);

	class set_bits_range;

	// Reads each word only once, such that bits of the current word can be modified during iteration.
	class set_bits_iterator {
		friend class set_bits_range;

		constexpr set_bits_iterator(const word_type *words, size_t num_words)
		: words_{words}, num_words_{num_words} {
			if(num_words_) {
				rest_ = words_[0];
				skip_();
			}
		}

	public:
		using iterator_category = std::forward_iterator_tag;
		using iterator_concept = std::forward_iterator_tag;
		using difference_type = ptrdiff_t;
		using value_type = size_t;

		constexpr set_bits_iterator() = default;

		constexpr size_t operator* () const {
			return index_ * word_bits + __builtin_ctzll(rest_);
		}

		constexpr set_bits_iterator &operator++ () {
			rest_ &= rest_ - 1;
			skip_();
			return *this;
		}

		constexpr set_bits_iterator operator++ (int) {
			auto copy = *this;
			++(*this);
			return copy;
		}

		constexpr bool operator== (const set_bits_iterator &other) const {
			return index_ == other.index_ && rest_ == other.rest_;
		}

		constexpr bool operator== (std::default_sentinel_t) const {
			return !rest_;
		}

	private:
		constexpr void skip_() {
			while(!rest_ && ++index_ < num_words_)
				rest_ = words_[index_];
		}

		const word_type *words_ = nullptr;
		size_t num_words_ = 0;
		size_t index_ = 0;
		word_type rest_ = 0;
	};

	class set_bits_range : public std::ranges::view_interface<set_bits_range> {
		friend class dyn_bitset;

		constexpr set_bits_range(const word_type *words, size_t num_words)
		: words_{words}, num_words_{num_words} { }

	public:
		constexpr set_bits_range() = default;

		constexpr set_bits_iterator begin() const {
			return set_bits_iterator{words_, num_words_};
		}

		constexpr std::default_sentinel_t end() const {
			return {};
		}

	private:
		const word_type *words_ = nullptr;
		size_t num_words_ = 0;
	};

	friend constexpr void swap(dyn_bitset &a, dyn_bitset &b) {
		using std::swap;
		swap(a.allocator_, b.allocator_);
		swap(a.words_, b.words_);
		swap(a.size_, b.size_);
	}

	constexpr dyn_bitset() = default;

	explicit constexpr dyn_bitset(Allocator allocator)
	: allocator_{std::move(allocator)} { }

	// All bits are initially unset.
	explicit dyn_bitset(size_t size, Allocator allocator = Allocator{})
	: allocator_{std::move(allocator)}, size_{size} {
		allocate_();
		if(size_)
			memset(words_, 0, num_words_() * sizeof(word_type));
	}

	dyn_bitset(const dyn_bitset &other)
	: allocator_{other.allocator_}, size_{other.size_} {
		allocate_();
		if(size_)
			memcpy(words_, other.words_, num_words_() * sizeof(word_type));
	}

	constexpr dyn_bitset(dyn_bitset &&other)
	: dyn_bitset(other.allocator_) {
		swap(*this, other);
	}

	~dyn_bitset() {
		if(words_)
			allocator_.deallocate(words_, num_words_() * sizeof(word_type));
	}

	constexpr dyn_bitset &operator= (dyn_bitset other) {
		swap(*this, other);
		return *this;
	}

	// Copies the bits of a bitset of the same size. In contrast to operator=, this never allocates.
	void assign(const dyn_bitset &other) {
		FRG_ASSERT(size_ == other.size_);
		if(size_)
			memcpy(words_, other.words_, num_words_() * sizeof(word_type));
	}

	constexpr size_t size() const {
		return size_;
	}

	constexpr void set(size_t pos) {
		FRG_ASSERT(pos < size_);
		words_[pos / word_bits] |= word_type{1} << (pos % word_bits);
	}

	constexpr void reset(size_t pos) {
		FRG_ASSERT(pos < size_);
		words_[pos / word_bits] &= ~(word_type{1} << (pos % word_bits));
	}

	constexpr void flip(size_t pos) {
		FRG_ASSERT(pos < size_);
		words_[pos / word_bits] ^= word_type{1} << (pos % word_bits);
	}

	// Indices that are out of range are not part of the set (and this is not an error).
	constexpr bool test(size_t pos) const {
		if(pos >= size_)
			return false;
		return words_[pos / word_bits] & (word_type{1} << (pos % word_bits));
	}

	constexpr bool operator[] (size_t pos) const {
		return test(pos);
	}

	constexpr void set_all() {
		for(size_t i = 0, n = num_words_(); i < n; i++)
			words_[i] = ~word_type{0};
		trim_();
	}

	constexpr void reset_all() {
		for(size_t i = 0, n = num_words_(); i < n; i++)
			words_[i] = 0;
	}

	constexpr void flip_all() {
		for(size_t i = 0, n = num_words_(); i < n; i++)
			words_[i] = ~words_[i];
		trim_();
	}

	constexpr size_t count() const {
		size_t result = 0;
		for(size_t i = 0, n = num_words_(); i < n; i++)
			result += __builtin_popcountll(words_[i]);
		return result;
	}

	constexpr bool any() const {
		for(size_t i = 0, n = num_words_(); i < n; i++)
			if(words_[i])
				return true;
		return false;
	}

	constexpr bool none() const {
		return !any();
	}

	constexpr bool all() const {
		return count() == size_;
	}

	// Returns the smallest set index (or npos if there is none).
	constexpr size_t find_first() const {
		return find_from_(0);
	}

	// Returns the smallest set index that is greater than pos (or npos if there is none).
	constexpr size_t find_next(size_t pos) const {
		if(pos == npos)
			return npos;
		return find_from_(pos + 1);
	}

	// Range over all set indices in increasing order.
	constexpr set_bits_range set_bits() const {
		return set_bits_range{words_, num_words_()};
	}

	// Set union.
	constexpr dyn_bitset &operator|= (const dyn_bitset &other) {
		FRG_ASSERT(size_ == other.size_);
		for(size_t i = 0, n = num_words_(); i < n; i++)
			words_[i] |= other.words_[i];
		return *this;
	}

	// Set intersection.
	constexpr dyn_bitset &operator&= (const dyn_bitset &other) {
		FRG_ASSERT(size_ == other.size_);
		for(size_t i = 0, n = num_words_(); i < n; i++)
			words_[i] &= other.words_[i];
		return *this;
	}

	// Symmetric set difference.
	constexpr dyn_bitset &operator^= (const dyn_bitset &other) {
		FRG_ASSERT(size_ == other.size_);
		for(size_t i = 0, n = num_words_(); i < n; i++)
			words_[i] ^= other.words_[i];
		return *this;
	}

	// Set difference.
	constexpr dyn_bitset &operator-= (const dyn_bitset &other) {
		FRG_ASSERT(size_ == other.size_);
		for(size_t i = 0, n = num_words_(); i < n; i++)
			words_[i] &= ~other.words_[i];
		return *this;
	}

	constexpr bool intersects(const dyn_bitset &other) const {
		FRG_ASSERT(size_ == other.size_);
		for(size_t i = 0, n = num_words_(); i < n; i++)
			if(words_[i] & other.words_[i])
				return true;
		return false;
	}

	constexpr bool is_subset_of(const dyn_bitset &other) const {
		FRG_ASSERT(size_ == other.size_);
		for(size_t i = 0, n = num_words_(); i < n; i++)
			if(words_[i] & ~other.words_[i])
				return false;
		return true;
	}

	constexpr bool operator== (const dyn_bitset &other) const {
		if(size_ != other.size_)
			return false;
		for(size_t i = 0, n = num_words_(); i < n; i++)
			if(words_[i] != other.words_[i])
				return false;
		return true;
	}

private:
	constexpr size_t num_words_() const {
		return (size_ + word_bits - 1) / word_bits;
	}

	void allocate_() {
		if(size_)
			words_ = static_cast<word_type *>(allocator_.allocate(num_words_() * sizeof(word_type)));
	}

	// Bits of the last word that are beyond size_ are always zero; re-establish that invariant.
	constexpr void trim_() {
		if(size_ % word_bits)
			words_[size_ / word_bits] &= (word_type{1} << (size_ % word_bits)) - 1;
	}

	constexpr size_t find_from_(size_t pos) const {
		if(pos >= size_)
			return npos;
		size_t i = pos / word_bits;
		auto word = words_[i] & (~word_type{0} << (pos % word_bits));
		while(!word) {
			if(++i == num_words_())
				return npos;
			word = words_[i];
		}
		return i * word_bits + __builtin_ctzll(word);
	}

	FRG_NO_UNIQUE_ADDRESS Allocator allocator_{};
	word_type *words_ = nullptr;
	size_t size_ = 0;
};

} // namespace frg
