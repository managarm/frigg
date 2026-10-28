#include <cstdint>
#include <cstring>
#include <sys/mman.h>

#include <frg/slab.hpp>
#include <frg/spinlock.hpp>
#include <gtest/gtest.h>

#include "poisoning.hpp"

namespace {

struct slab_policy : asan_poisoning {
	uintptr_t map(size_t length) {
		if (fail_maps)
			return 0;
		void *p = mmap(nullptr, length, PROT_READ | PROT_WRITE,
		               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (p == MAP_FAILED)
			return 0;
		poison(p, length);
		return reinterpret_cast<uintptr_t>(p);
	}

	void unmap(uintptr_t address, size_t length) {
		unpoison(reinterpret_cast<void *>(address), length);
		munmap(reinterpret_cast<void *>(address), length);
	}

	bool fail_maps = false;
};

using pool = frg::slab_pool<slab_policy, frg::simple_spinlock>;
using allocator = frg::slab_allocator<slab_policy, frg::simple_spinlock>;

} // namespace

TEST(slab, allocation_failure) {
	slab_policy plcy;
	pool p{plcy};
	allocator alloc{&p};

	void *small = alloc.allocate(64);
	EXPECT_NE(small, nullptr);
	alloc.free(small);

	EXPECT_EQ(alloc.allocate(SIZE_MAX / 4), nullptr);

	plcy.fail_maps = true;
	EXPECT_EQ(alloc.allocate(1024 * 1024), nullptr);
	plcy.fail_maps = false;

	void *after = alloc.allocate(64);
	EXPECT_NE(after, nullptr);
	alloc.free(after);
}

TEST(slab, oversized_allocations_do_not_wrap) {
	slab_policy plcy;
	pool p{plcy};
	allocator alloc{&p};

	EXPECT_EQ(alloc.allocate(SIZE_MAX), nullptr);
	EXPECT_EQ(alloc.allocate(SIZE_MAX - 4095), nullptr);

	EXPECT_EQ(alloc.allocate(SIZE_MAX & ~size_t{4095}), nullptr);
}

TEST(slab, reallocate) {
	slab_policy plcy;
	pool p{plcy};
	allocator alloc{&p};

	// Shrink and grow within the same slot.
	auto obj = static_cast<char *>(alloc.allocate(10));
	ASSERT_NE(obj, nullptr);
	memset(obj, 'a', 10);
	EXPECT_EQ(alloc.reallocate(obj, 5), obj);
	EXPECT_EQ(alloc.reallocate(obj, 10), obj);

	// Grow into a different slot and then into a large allocation.
	for (size_t new_size : {size_t{1000}, size_t{1024 * 1024}}) {
		auto moved = static_cast<char *>(alloc.reallocate(obj, new_size));
		ASSERT_NE(moved, nullptr);
		for (size_t i = 0; i < 5; i++)
			EXPECT_EQ(moved[i], 'a');
		obj = moved;
	}
	alloc.free(obj);
}
