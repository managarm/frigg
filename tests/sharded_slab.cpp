#include <atomic>
#include <sys/mman.h>
#include <thread>
#include <vector>

#include <frg/sharded_slab.hpp>
#include <gtest/gtest.h>

struct sharded_slab_policy {
	void *map(size_t size) {
		void *p = mmap(nullptr, size, PROT_READ | PROT_WRITE,
		               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (p == MAP_FAILED)
			return nullptr;
		return p;
	}

	void unmap(void *p, size_t size) {
		munmap(p, size);
	}
};

using pool_type = frg::sharded_slab::pool<sharded_slab_policy>;

// Check all powers of two from 2^0 to 2^30 to check whether all size classes and large allocations work.
TEST(sharded_slab, multiple_sizes) {
	pool_type pool;

	for (int s = 0; s <= 30; s++) {
		size_t size = size_t{1} << s;
		void *obj = pool.allocate(size);
		ASSERT_NE(obj, nullptr);
		memset(obj, 0xFF, size);
		pool.deallocate(obj);
	}
}

// Allocate enough objects to exhaust one chunk.
TEST(sharded_slab, exhaust_chunk) {
	constexpr size_t count = 20000;

	pool_type pool;
	std::vector<void *> objs(count);

	for (size_t i = 0; i < 5; ++i) {
		for (size_t i = 0; i < count; i++) {
			objs[i] = pool.allocate(128);
			ASSERT_NE(objs[i], nullptr);
			memset(objs[i], 0xFF, 128);
		}
		for (size_t i = 0; i < count; i++)
			pool.deallocate(objs[i]);
	}
}

TEST(sharded_slab, pointer_uniqueness) {
	constexpr size_t count = 1000;

	pool_type pool;
	std::vector<void *> objs(count);

	for (size_t i = 0; i < 5; ++i) {
		for (size_t i = 0; i < count; i++) {
			objs[i] = pool.allocate(128);
			ASSERT_NE(objs[i], nullptr);
			memset(objs[i], 0xFF, 128);
		}
		for (size_t i = 0; i < count; i++) {
			for (size_t j = i + 1; j < count; j++) {
				EXPECT_NE(objs[i], objs[j]);
			}
		}
		for (size_t i = 0; i < count; i++)
			pool.deallocate(objs[i]);
	}
}

TEST(sharded_slab, cross_thread_deallocation) {
	constexpr size_t count = 20000;

	pool_type main_pool;
	std::vector<void *> objs(count);

	for (size_t i = 0; i < 5; ++i) {
		// Allocate in main thread + free in another thread.
		for (size_t i = 0; i < count; i++) {
			objs[i] = main_pool.allocate(128);
			ASSERT_NE(objs[i], nullptr);
			memset(objs[i], 0xFF, 128);
		}
		std::thread t([&] {
			pool_type thread_pool;
			for (size_t i = 0; i < count; i++)
				thread_pool.deallocate(objs[i]);
		});
		t.join();

		// Allocate in main thread + free in main thread.
		for (size_t i = 0; i < count; i++) {
			objs[i] = main_pool.allocate(128);
			ASSERT_NE(objs[i], nullptr);
			memset(objs[i], 0xFF, 128);
		}
		for (size_t i = 0; i < count; i++)
			main_pool.deallocate(objs[i]);
	}
}
