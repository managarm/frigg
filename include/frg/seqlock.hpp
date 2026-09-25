#pragma once

#include <stddef.h>
#include <stdint.h>
#include <array>
#include <atomic>
#include <bit>
#include <type_traits>
#include <utility>

#include <frg/macros.hpp>
#include <frg/spinlock.hpp>

namespace frg FRG_VISIBILITY {

// Value that is written by serialized writers and read by lock-free readers (i.e., a seqlock).
// Readers retry while a write is in progress. The value is copied through integer words,
// hence T must be trivially copyable and must not contain padding bits.
template<typename T>
class seqlock_cell {
	using word_type = uintptr_t;

	static_assert(std::is_trivially_copyable_v<T>);
	static_assert(std::has_unique_object_representations_v<T>,
			"padding bits of T cannot be copied through integer words");
	static_assert(!(sizeof(T) % sizeof(word_type)));

	static constexpr size_t num_words = sizeof(T) / sizeof(word_type);
	using word_array = std::array<word_type, num_words>;

public:
	constexpr seqlock_cell()
	: seqlock_cell{T{}} { }

	constexpr explicit seqlock_cell(const T &value)
	: seqlock_cell{std::bit_cast<word_array>(value), std::make_index_sequence<num_words>{}} { }

	seqlock_cell(const seqlock_cell &) = delete;
	seqlock_cell &operator= (const seqlock_cell &) = delete;

	T load() const {
		word_array words;
		while(true) {
			auto seq = seq_.load(std::memory_order_acquire);
			if(seq & 1) {
				detail::loophint();
				continue;
			}
			for(size_t i = 0; i < num_words; ++i)
				words[i] = words_[i].load(std::memory_order_relaxed);
			// Orders the loads from words_ before the re-check of seq_.
			std::atomic_thread_fence(std::memory_order_acquire);
			if(seq_.load(std::memory_order_relaxed) == seq)
				return std::bit_cast<T>(words);
		}
	}

	// Calls must be serialized by the caller (e.g., by holding a lock).
	void store(const T &value) {
		auto words = std::bit_cast<word_array>(value);
		auto seq = seq_.load(std::memory_order_relaxed);
		FRG_ASSERT(!(seq & 1));
		seq_.store(seq + 1, std::memory_order_relaxed);
		// Orders the store of the odd sequence number before the stores to words_.
		std::atomic_thread_fence(std::memory_order_release);
		for(size_t i = 0; i < num_words; ++i)
			words_[i].store(words[i], std::memory_order_relaxed);
		seq_.store(seq + 2, std::memory_order_release);
	}

private:
	template<size_t... Is>
	constexpr seqlock_cell(const word_array &words, std::index_sequence<Is...>)
	: words_{words[Is]...} { }

	std::atomic<word_type> seq_{0};
	std::atomic<word_type> words_[num_words];
};

} // namespace frg
