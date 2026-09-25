#include <atomic>
#include <stdint.h>
#include <thread>
#include <vector>

#include <frg/seqlock.hpp>
#include <gtest/gtest.h>

namespace {

struct triple {
	uint64_t a{1};
	uint64_t b{2};
	uint64_t c{3};
};

} // anonymous namespace

TEST(seqlock_cell, default_constructs_value) {
	frg::seqlock_cell<triple> cell;
	auto v = cell.load();
	EXPECT_EQ(v.a, 1);
	EXPECT_EQ(v.b, 2);
	EXPECT_EQ(v.c, 3);
}

TEST(seqlock_cell, loads_stored_value) {
	frg::seqlock_cell<triple> cell{triple{4, 5, 6}};
	EXPECT_EQ(cell.load().b, 5);

	cell.store(triple{7, 8, 9});
	auto v = cell.load();
	EXPECT_EQ(v.a, 7);
	EXPECT_EQ(v.b, 8);
	EXPECT_EQ(v.c, 9);
}
