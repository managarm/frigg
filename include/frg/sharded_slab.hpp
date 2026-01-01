#pragma once

#include <stddef.h>
#include <stdint.h>
#include <atomic>
#include <concepts>
#include <new>

#include <frg/bitops.hpp>
#include <frg/expected.hpp>
#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

namespace sharded_slab {

enum class error {
	success = 0,
	allocation_failed,
};

// TODO: We probably want to support customization features of frg::slab_pool in the future,
//       in particular:
//       * Slab size and page size overrides
//       * Bucket customizations
//       * (K)ASAN integration
//       * Aligned page mapping functions
//       * Allocation tracing
template<typename P>
concept Policy = requires(P policy, void *p, size_t size) {
	// The map() and unmap() functions can be used to allocate and free memory at page granularity.
	{ policy.map(size) } -> std::same_as<void *>;
	{ policy.unmap(p, size) } -> std::same_as<void>;
};

// Thread-aware slab allocator.
// The pool struct itself is not thread-safe; however, objects allocated
// from one pool instance can be freed by another pool instance
// as long as the policy is identical.
template<Policy P>
struct pool {
	static constexpr size_t page_size = 4096;
	static constexpr size_t chunk_boundary = 1 << 18;
	static constexpr size_t chunk_size = chunk_boundary;

	// TODO: We may want to make this dependent on the number of objects in the chunk.
	static constexpr size_t reactivate_threshold = 8;

	// Size classes are powers of two from 16 to 4096.
	// TODO: Adopt the same size classes as the original slab code.
	static constexpr size_t min_shift = 4;
	static constexpr size_t max_shift = 12;
	static constexpr size_t min_size_class = size_t{1} << min_shift;
	static constexpr size_t max_size_class = size_t{1} << max_shift;
	static constexpr size_t num_size_classes = max_shift - min_shift + 1;

	// Map a size to a bucket index. Returns the index of the smallest size class >= size.
	static constexpr size_t bucket_index(size_t size) {
		FRG_ASSERT(size <= max_size_class);
		if (size < min_size_class)
			return 0;
		return ceil_log2(size) - min_shift;
	}

	static_assert(bucket_index(16) == 0);
	static_assert(bucket_index(4096) == 8);

	// Inverse of bucket_index().
	static constexpr size_t size_of_bucket(size_t index) {
		return min_size_class << index;
	}

	// Stores the address of an object as the object's offset vs. its chunk_header.
	// This is needed to be able to compress the chunk_state struct below to a size that can be manipulated by a single CAS.
	// Note that zero is an invalid compressed_address (since the chunk_header is at offset zero).
	using compressed_address = uint32_t;

	// State for chunks.
	// Chunks can be in several states:
	// - Chunks are said to be INACTIVE if chunk_state::inactive is set.
	// - Chunks are said to be PENDING if:
	//   * chunk_state::inactive is clear
	//   * and the chunk is in bucket::owner_pending_list or bucket::threaded_pending_list.
	// - Chunks are said to be ACTIVE if:
	//   * chunk_state::inactive is clear
	//   * and the chunk is in bucket::active_list or bucket::head_chunk.
	//
	// State transitions follow the following invariants:
	// - Any pool instance (i.e., every thread) can transition a chunk from INACTIVE to PENDING.
	//   This is done by first clearing chunk_state::inactive followed by pushing the chunk onto
	//   bucket::owner_pending_list or bucket::threaded_pending_list.
	//   Note that no locking is done during this transition;
	//   hence, it is possible for chunks with inactive clear to not be in any list.
	// - No other transition is allowed from INACTIVE state.
	// - Only the owner can transition chunks from PENDING or ACTIVE state into other states.
	//   As a result, only the owner can transition a chunk to INACTIVE.
	struct alignas(sizeof(uint64_t)) chunk_state {
		// Head of the threaded free list.
		compressed_address threaded_free;
		// Size of the threaded free list.
		uint32_t threaded_count : 31;
		// True if chunk is INACTIVE (not on any list).
		bool inactive : 1;
	};
	static_assert(sizeof(chunk_state) == 8);
	static_assert(alignof(chunk_state) == 8);
	static_assert(std::atomic<chunk_state>::is_always_lock_free);

	// Limit on the number of objects due to number of bits of threaded_count.
	static constexpr size_t max_objects_in_chunk = (size_t{1} << 31) - 1;

	// Free list of objects.
	struct free_object {
		compressed_address next{0};
	};

	struct chunk_header;

	// Each bucket manages allocations for a specific size class.
	struct bucket {
		// Size of the objects stored in the slab.
		size_t object_size{0};
		// Current chunk we allocate from. If null, pop from active_list.
		chunk_header *head_chunk{nullptr};
		// List of other ACTIVE chunks (with non-empty owner_free).
		// TODO: It may make sense to use a rbtree tree here to get first-fit behavior.
		//       This should reduce fragmentation and we only need to look at this
		//       data structure if there is no head_chunk anyway.
		chunk_header *active_list{nullptr};
		// Lists of PENDING chunks.
		chunk_header *owner_pending_list{nullptr};
		std::atomic<chunk_header *> threaded_pending_list{nullptr};
	};

	enum class chunk_type {
		none,
		// Chunks that consist of many objects.
		slab,
		// Chunks that consist of only a single object.
		large,
	};

	// A chunk is a contiguous memory range that consists of a header
	// followed by or one multiple memory objects of a uniform size.
	// The header is aligned on chunk_boundary.
	// The header is followed by memory objects such that the total size of the chunk is chunk_size.
	struct chunk_header {
		chunk_type type{chunk_type::none};
		pool *owner{nullptr};
		bucket *bkt{nullptr};
		// Head of the free list that the owner uses for allocations.
		compressed_address owner_free{0};
		// Number of items on the owner_free list.
		uint32_t owner_count{0};
		std::atomic<chunk_state> state{};
		// Next chunk in either bucket::active_list, bucket::owner_pending_list and bucket::threaded_pending_list.
		chunk_header *next_in_list{nullptr};
		// Pointer to the chunk's extent.
		// This is the chunk's memory range including padding that is in front of chunk_header.
		void *extent_ptr{nullptr};
		// Size of the chunk's extent.
		size_t extent_size{0};
	};

	constexpr pool() {
		for (size_t i = 0; i < num_size_classes; i++) {
			buckets_[i].object_size = size_of_bucket(i);
		}
	}

	void *allocate(size_t size) {
		if (size > max_size_class) {
			auto result = large_allocate(size);
			if (!result)
				return nullptr;
			return result.value();
		} else {
			auto idx = bucket_index(size);
			auto result = slab_allocate(&buckets_[idx]);
			if (!result)
				return nullptr;
			return result.value();
		}
	}

	void deallocate(void *object) {
		if (!object)
			return;
		auto chunk = chunk_header_of(object);
		if (chunk->type == chunk_type::large) {
			large_free(chunk);
			return;
		}
		if (chunk->owner == this) {
			slab_deallocate_owned(chunk, object);
		} else {
			slab_deallocate_threaded(chunk, object);
		}
	}

private:
	// Find the chunk_header for an object by aligning the pointer down to chunk_boundary.
	chunk_header *chunk_header_of(void *object) {
		auto addr = reinterpret_cast<uintptr_t>(object);
		auto aligned = addr & ~(chunk_boundary - 1);
		return reinterpret_cast<chunk_header *>(aligned);
	}

	// Convert void * to compressed_address.
	compressed_address object_to_address(chunk_header *chunk, void *object) {
		return reinterpret_cast<uintptr_t>(object) - reinterpret_cast<uintptr_t>(chunk);
	}

	// Convert compressed_address to void *.
	void *object_from_address(chunk_header *chunk, compressed_address ca) {
		return reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(chunk) + ca);
	}

	frg::expected<error> slab_chunk_create(bucket *bkt) {
		FRG_ASSERT(!bkt->head_chunk);

		// Over-allocate to ensure we can align to chunk_boundary.
		auto extent_size = (chunk_size + chunk_boundary - 1 + page_size - 1) & ~(page_size - 1);
		auto extent_ptr = policy_.map(extent_size);
		if (!extent_ptr)
			return error::allocation_failed;

		// Align up to chunk_boundary.
		auto raw_addr = reinterpret_cast<uintptr_t>(extent_ptr);
		auto aligned_addr = (raw_addr + chunk_boundary - 1) & ~(chunk_boundary - 1);
		auto chunk = reinterpret_cast<chunk_header *>(aligned_addr);

		new (chunk) chunk_header{
			.type{chunk_type::slab},
			.owner{this},
			.bkt{bkt},
			.state{
				chunk_state{
					.threaded_free{0},
					.threaded_count{0},
					.inactive{false},
				}
			},
			.extent_ptr{extent_ptr},
			.extent_size{extent_size},
		};

		// Build free list of all objects in the chunk.
		size_t object_size = bkt->object_size;
		size_t first_offset = (sizeof(chunk_header) + object_size - 1) & ~(object_size - 1);

		compressed_address prev = 0;
		size_t count = 0;
		for (size_t offset = first_offset; offset + object_size <= chunk_size; offset += object_size) {
			auto obj = new (object_from_address(chunk, offset)) free_object{};
			obj->next = prev;
			prev = static_cast<compressed_address>(offset);
			count++;
		}
		FRG_ASSERT(count <= max_objects_in_chunk);
		chunk->owner_free = prev;
		chunk->owner_count = count;

		bkt->head_chunk = chunk;
		return {};
	}

	// Pop a single chunk from one of the pending lists and add it to active_list.
	// This needs to be called regularly for maintenance of the data structure.
	// We call it on each allocation.
	void slab_chunk_update(bucket *bkt) {
		// If owner_pending_list becomes empty, steal the entire threaded_pending_list.
		if (!bkt->owner_pending_list) {
			if (!bkt->threaded_pending_list.load(std::memory_order_relaxed))
				return;
			bkt->owner_pending_list = bkt->threaded_pending_list.exchange(nullptr, std::memory_order_acquire);
			FRG_ASSERT(bkt->owner_pending_list);
		}

		// Pop from owner_pending_list.
		chunk_header *chunk = bkt->owner_pending_list;
		bkt->owner_pending_list = chunk->next_in_list;

		// Add to active_list.
		chunk->next_in_list = bkt->active_list;
		bkt->active_list = chunk;
	}

	// Pop a chunk from active_list into head_chunk.
	// This is called when no head_chunk exists.
	// We also merge threaded_free into owner_free here.
	frg::expected<error> slab_chunk_refresh(bucket *bkt) {
		FRG_ASSERT(!bkt->head_chunk);

		// If there is no active_list, create a new chunk.
		if (!bkt->active_list)
			return slab_chunk_create(bkt);

		// Pop from active_list.
		chunk_header *chunk = bkt->active_list;
		bkt->active_list = chunk->next_in_list;

		// Obtain threaded_free and append it to owner_free.
		chunk_state current_state = chunk->state.exchange(
			chunk_state{
				.threaded_free{0},
				.threaded_count{0},
				.inactive{false},
			},
			std::memory_order_acquire
		);
		FRG_ASSERT(!current_state.inactive);

		if (current_state.threaded_free) {
			// Find the end of the threaded_free list.
			auto tail = static_cast<free_object *>(object_from_address(chunk, current_state.threaded_free));
			size_t objs_seen = 1;
			while (tail->next) {
				tail = static_cast<free_object *>(object_from_address(chunk, tail->next));
				++objs_seen;
			}
			FRG_ASSERT(objs_seen == current_state.threaded_count);

			tail->next = chunk->owner_free;
			chunk->owner_free = current_state.threaded_free;
			chunk->owner_count += current_state.threaded_count;
		}
		FRG_ASSERT(chunk->owner_free);
		FRG_ASSERT(chunk->owner_count);

		bkt->head_chunk = chunk;
		return {};
	}

	void slab_chunk_retire(bucket *bkt) {
		FRG_ASSERT(bkt->head_chunk);

		auto chunk = bkt->head_chunk;
		bkt->head_chunk = nullptr;

		chunk_state current_state = chunk->state.load(std::memory_order_relaxed);
		chunk_state new_state;
		do {
			// Too many objects in threaded_free, keep as ACTIVE.
			if (current_state.threaded_count >= reactivate_threshold) {
				chunk->next_in_list = bkt->active_list;
				bkt->active_list = chunk;
				return;
			}

			// Transition to INACTIVE.
			new_state = chunk_state{
				.threaded_free{current_state.threaded_free},
				.threaded_count{current_state.threaded_count},
				.inactive{true},
			};
		} while (!chunk->state.compare_exchange_weak(
			current_state, new_state,
			std::memory_order_release,
			std::memory_order_relaxed));
	}

	frg::expected<error, void *> slab_allocate(bucket *bkt) {
		slab_chunk_update(bkt);

		// Ensure that we have a chunk to allocate from.
		if (!bkt->head_chunk) [[unlikely]] {
			auto result = slab_chunk_refresh(bkt);
			if (!result)
				return result.error();
		}
		FRG_ASSERT(bkt->head_chunk);

		// Pop an object from head_chunk's owner_free list.
		auto chunk = bkt->head_chunk;
		FRG_ASSERT(chunk->owner_free);
		FRG_ASSERT(chunk->owner_count);
		auto ca = chunk->owner_free;
		auto obj = static_cast<free_object *>(object_from_address(chunk, ca));
		chunk->owner_free = obj->next;
		chunk->owner_count--;

		// Retire chunks once the free list becomes empty.
		if (!chunk->owner_free)
			slab_chunk_retire(bkt);

		return object_from_address(chunk, ca);
	}

	void slab_deallocate_owned(chunk_header *chunk, void *object) {
		auto ca = object_to_address(chunk, object);

		// Owner deallocation: push onto owner_free.
		auto obj = new (object) free_object{};
		obj->next = chunk->owner_free;
		chunk->owner_free = ca;
		chunk->owner_count++;

		// If chunk is INACTIVE and owner_count exceeds threshold, transition to PENDING.
		if (!(chunk->owner_count >= reactivate_threshold))
			return;

		chunk_state current_state = chunk->state.load(std::memory_order_relaxed);
		chunk_state new_state;
		do {
			if (!current_state.inactive)
				return;
			new_state = chunk_state{
				.threaded_free{current_state.threaded_free},
				.threaded_count{current_state.threaded_count},
				.inactive{false},
			};
		} while (!chunk->state.compare_exchange_weak(
			current_state, new_state,
			std::memory_order_release,
			std::memory_order_relaxed));

		// Push chunk onto owner_pending_list.
		chunk->next_in_list = chunk->bkt->owner_pending_list;
		chunk->bkt->owner_pending_list = chunk;
	}

	void slab_deallocate_threaded(chunk_header *chunk, void *object) {
		auto ca = object_to_address(chunk, object);

		// Threaded deallocation: push onto threaded_free by using CAS.
		auto obj = new (object) free_object{};

		chunk_state current_state = chunk->state.load(std::memory_order_relaxed);
		chunk_state new_state;
		do {
			obj->next = current_state.threaded_free;
			new_state = {
				.threaded_free{ca},
				.threaded_count{current_state.threaded_count + 1u},
				.inactive{current_state.inactive},
			};
			// If INACTIVE and count exceeds threshold, transition to PENDING.
			if (current_state.inactive && new_state.threaded_count >= reactivate_threshold)
				new_state.inactive = false;
		} while (!chunk->state.compare_exchange_weak(
			current_state, new_state,
			std::memory_order_release,
			std::memory_order_relaxed));

		// If we transitioned from INACTIVE, push chunk onto threaded_pending_list.
		if (!(current_state.inactive && !new_state.inactive))
			return;

		chunk_header *current_list = chunk->bkt->threaded_pending_list.load(std::memory_order_relaxed);
		do {
			chunk->next_in_list = current_list;
		} while (!chunk->bkt->threaded_pending_list.compare_exchange_weak(
			current_list, chunk,
			std::memory_order_release,
			std::memory_order_relaxed));
	}

	frg::expected<error, void *> large_allocate(size_t size) {
		// Compute the space needed after alignment.
		// Object starts after chunk_header, aligned to page boundary for large objects.
		size_t object_alignment = 4096;
		size_t first_offset = (sizeof(chunk_header) + object_alignment - 1) & ~(object_alignment - 1);
		size_t data_size = first_offset + size;

		// Over-allocate to ensure we can align to chunk_boundary.
		auto extent_size = (data_size + chunk_boundary - 1 + page_size - 1) & ~(page_size - 1);
		auto extent_ptr = policy_.map(extent_size);
		if (!extent_ptr)
			return error::allocation_failed;

		// Align up to chunk_boundary.
		auto raw_addr = reinterpret_cast<uintptr_t>(extent_ptr);
		auto aligned_addr = (raw_addr + chunk_boundary - 1) & ~(chunk_boundary - 1);
		auto chunk = reinterpret_cast<chunk_header *>(aligned_addr);

		new (chunk) chunk_header{
			.type{chunk_type::large},
			.owner{this},
			.extent_ptr{extent_ptr},
			.extent_size{extent_size},
		};

		return reinterpret_cast<void *>(aligned_addr + first_offset);
	}

	void large_free(chunk_header *chunk) {
		policy_.unmap(chunk->extent_ptr, chunk->extent_size);
	}

	P policy_;
	bucket buckets_[num_size_classes];
};

} // namespace sharded_slab

template<sharded_slab::Policy P>
using sharded_slab_pool = sharded_slab::pool<P>;

} // namespace frg
