#pragma once

#include <stddef.h>
#include <stdint.h>

#include <frg/allocation.hpp>
#include <frg/bitops.hpp>
#include <frg/macros.hpp>
#include <frg/span.hpp>
#include <frg/utility.hpp>

namespace frg FRG_VISIBILITY {

template <typename Allocator>
struct byte_ring_buffer {
	byte_ring_buffer(size_t capacity, Allocator allocator = Allocator())
	: buffer_{allocator, capacity}, capacity_{capacity}, mask_{capacity - 1} {
		FRG_ASSERT(is_p2(capacity));
	}

	[[nodiscard]] bool empty() const { return tail_ == head_; }
	[[nodiscard]] size_t size() const { return head_ - tail_; }
	[[nodiscard]] size_t capacity() const { return buffer_.size(); }
	[[nodiscard]] size_t available_space() const { return capacity() - size(); }

	// returns the number of actually enqueued bytes
	[[nodiscard]] size_t enqueue(frg::span<const uint8_t> data) {
		const size_t chunk = frg::min(data.size_bytes(), available_space());
		const size_t head_index = head_ & mask_;
		const size_t bytes_until_wrap = capacity() - head_index;

		if (chunk <= bytes_until_wrap) {
			memcpy(static_cast<uint8_t *>(buffer_.data()) + head_index, data.data(), chunk);
		} else {
			memcpy(static_cast<uint8_t *>(buffer_.data()) + head_index, data.data(), bytes_until_wrap);
			memcpy(
			    buffer_.data(),
			    data.data() + bytes_until_wrap,
			    chunk - bytes_until_wrap
			);
		}

		head_ += chunk;
		return chunk;
	}

	// returns the number of actually dequeued bytes
	[[nodiscard]] size_t dequeue(frg::span<uint8_t> data) {
		const size_t chunk = frg::min(data.size_bytes(), size());
		const size_t tail_index = tail_ & mask_;
		const size_t bytes_until_wrap = capacity() - tail_index;

		if (chunk <= bytes_until_wrap) {
			memcpy(data.data(), static_cast<uint8_t *>(buffer_.data()) + tail_index, chunk);
		} else {
			memcpy(data.data(), static_cast<uint8_t *>(buffer_.data()) + tail_index, bytes_until_wrap);
			memcpy(
			    data.data() + bytes_until_wrap,
			    buffer_.data(),
			    chunk - bytes_until_wrap
			);
		}

		tail_ += chunk;
		return chunk;
	}

private:
	frg::unique_memory<Allocator> buffer_;

	size_t capacity_;
	size_t head_ = 0; // write index
	size_t tail_ = 0; // read index
	size_t mask_;
};

} // namespace frg
