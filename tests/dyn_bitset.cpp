#include <frg/dyn_bitset.hpp>
#include <frg/std_compat.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <initializer_list>
#include <ranges>
#include <vector>

namespace {

using bitset = frg::dyn_bitset<frg::stl_allocator>;

bitset make(size_t size, std::initializer_list<size_t> bits) {
	bitset bs{size};
	for(auto bit : bits)
		bs.set(bit);
	return bs;
}

std::vector<size_t> collect(const bitset &bs) {
	std::vector<size_t> v;
	for(auto bit : bs.set_bits())
		v.push_back(bit);
	return v;
}

} // anonymous namespace

TEST(dyn_bitset, set_reset_test) {
	bitset bs{130};
	EXPECT_EQ(bs.size(), 130u);
	EXPECT_TRUE(bs.none());

	for(size_t bit : {0, 63, 64, 129})
		bs.set(bit);
	EXPECT_EQ(bs.count(), 4u);
	EXPECT_TRUE(bs.test(0));
	EXPECT_TRUE(bs.test(63));
	EXPECT_TRUE(bs[64]);
	EXPECT_TRUE(bs[129]);
	EXPECT_FALSE(bs.test(1));
	EXPECT_FALSE(bs.test(65));

	bs.reset(63);
	bs.flip(64);
	bs.flip(65);
	EXPECT_EQ(collect(bs), (std::vector<size_t>{0, 65, 129}));

	bs.reset_all();
	EXPECT_TRUE(bs.none());
}

TEST(dyn_bitset, out_of_range_is_unset) {
	bitset empty;
	EXPECT_EQ(empty.size(), 0u);
	EXPECT_FALSE(empty.test(0));
	EXPECT_EQ(empty.count(), 0u);
	EXPECT_EQ(empty.find_first(), bitset::npos);
	EXPECT_TRUE(collect(empty).empty());

	bitset bs{70};
	bs.set_all();
	EXPECT_FALSE(bs.test(70));
	EXPECT_FALSE(bs.test(127));
	EXPECT_FALSE(bs.test(128));
}

// set_all() and flip_all() must not set the unused bits of the last word.
TEST(dyn_bitset, bulk_operations_respect_size) {
	for(size_t size : {1, 63, 64, 65, 128, 130}) {
		bitset bs{size};
		EXPECT_FALSE(bs.all());

		bs.set_all();
		EXPECT_EQ(bs.count(), size);
		EXPECT_TRUE(bs.all());
		EXPECT_EQ(collect(bs).back(), size - 1);

		bs.reset(0);
		bs.flip_all();
		EXPECT_EQ(collect(bs), (std::vector<size_t>{0}));

		bs.flip_all();
		EXPECT_EQ(bs.count(), size - 1);
	}
}

TEST(dyn_bitset, find) {
	auto bs = make(200, {3, 63, 64, 128, 199});
	EXPECT_EQ(bs.find_first(), 3u);
	EXPECT_EQ(bs.find_next(3), 63u);
	EXPECT_EQ(bs.find_next(62), 63u);
	EXPECT_EQ(bs.find_next(63), 64u);
	EXPECT_EQ(bs.find_next(64), 128u);
	EXPECT_EQ(bs.find_next(128), 199u);
	EXPECT_EQ(bs.find_next(199), bitset::npos);
	EXPECT_EQ(bs.find_next(1000), bitset::npos);
	EXPECT_EQ(bs.find_next(bitset::npos), bitset::npos);

	EXPECT_EQ(bitset{200}.find_first(), bitset::npos);
}

TEST(dyn_bitset, iterate_set_bits) {
	EXPECT_TRUE(collect(bitset{200}).empty());
	EXPECT_EQ(collect(make(200, {0})), (std::vector<size_t>{0}));
	EXPECT_EQ(collect(make(200, {3, 63, 64, 128, 199})),
			(std::vector<size_t>{3, 63, 64, 128, 199}));

	// Iteration agrees with find_first() / find_next().
	auto bs = make(200, {5, 6, 70, 191});
	std::vector<size_t> found;
	for(auto bit = bs.find_first(); bit != bitset::npos; bit = bs.find_next(bit))
		found.push_back(bit);
	EXPECT_EQ(collect(bs), found);

	// Visited bits can be reset during iteration.
	for(auto bit : bs.set_bits())
		bs.reset(bit);
	EXPECT_TRUE(bs.none());
}

static_assert(std::forward_iterator<bitset::set_bits_iterator>);
static_assert(std::ranges::forward_range<bitset::set_bits_range>);
static_assert(std::ranges::view<bitset::set_bits_range>);
static_assert(!std::is_convertible_v<frg::stl_allocator, bitset>);

TEST(dyn_bitset, ranges) {
	auto bs = make(200, {3, 63, 64, 128, 199});
	auto bits = bs.set_bits();
	EXPECT_EQ(std::ranges::distance(bits), 5);
	EXPECT_EQ(*std::ranges::find_if(bits, [] (size_t bit) { return bit > 64; }), 128u);

	auto odd = bs.set_bits() | std::views::filter([] (size_t bit) { return bit % 2; });
	std::vector<size_t> found;
	for(auto bit : odd)
		found.push_back(bit);
	EXPECT_EQ(found, (std::vector<size_t>{3, 63, 199}));

	EXPECT_TRUE(bitset{200}.set_bits().empty());
	EXPECT_FALSE(bs.set_bits().empty());
	EXPECT_EQ(bs.set_bits().front(), 3u);
}

TEST(dyn_bitset, set_operations) {
	auto a = make(100, {1, 2, 70, 80});
	auto b = make(100, {2, 3, 80, 90});

	auto r = a;
	r |= b;
	EXPECT_EQ(r, make(100, {1, 2, 3, 70, 80, 90}));

	r = a;
	r &= b;
	EXPECT_EQ(r, make(100, {2, 80}));

	r = a;
	r ^= b;
	EXPECT_EQ(r, make(100, {1, 3, 70, 90}));

	r = a;
	r -= b;
	EXPECT_EQ(r, make(100, {1, 70}));
}

TEST(dyn_bitset, set_relations) {
	auto a = make(100, {1, 70});
	auto b = make(100, {1, 2, 70, 80});
	auto c = make(100, {3, 90});

	EXPECT_TRUE(a.intersects(b));
	EXPECT_FALSE(a.intersects(c));
	EXPECT_FALSE(bitset{100}.intersects(b));

	EXPECT_TRUE(a.is_subset_of(b));
	EXPECT_FALSE(b.is_subset_of(a));
	EXPECT_TRUE(a.is_subset_of(a));
	EXPECT_TRUE(bitset{100}.is_subset_of(c));

	EXPECT_TRUE(a == make(100, {1, 70}));
	EXPECT_FALSE(a == b);
	// Bitsets of different size are never equal.
	EXPECT_FALSE(bitset{100} == bitset{101});
}

TEST(dyn_bitset, copy_move_assign) {
	auto a = make(100, {1, 70});

	// Copies are independent of the original.
	bitset copy{a};
	copy.set(5);
	EXPECT_EQ(a, make(100, {1, 70}));
	EXPECT_EQ(copy, make(100, {1, 5, 70}));

	bitset moved{std::move(copy)};
	EXPECT_EQ(moved, make(100, {1, 5, 70}));
	EXPECT_EQ(copy.size(), 0u);
	EXPECT_FALSE(copy.test(1));

	// operator= adopts the size of the source.
	bitset target{10};
	target = a;
	EXPECT_EQ(target, a);

	// assign() overwrites all bits of a bitset of the same size.
	auto preallocated = make(100, {2, 99});
	preallocated.assign(a);
	EXPECT_EQ(preallocated, a);
}
