#include <frg/string.hpp>
#include <frg/std_compat.hpp>
#include <frg/random.hpp>

#include <gtest/gtest.h>

using string = frg::string<frg::stl_allocator>;

namespace {
void common_startsends_tests(auto ts1) {
	EXPECT_TRUE(ts1.starts_with("abc"));
	EXPECT_TRUE(ts1.ends_with("def"));

	EXPECT_FALSE(ts1.starts_with("def"));
	EXPECT_FALSE(ts1.ends_with("abc"));

	EXPECT_FALSE(ts1.ends_with("this long string should not crash"));

	EXPECT_TRUE(ts1.starts_with(ts1));
	EXPECT_TRUE(ts1.ends_with(ts1));
}
}

TEST(strings, string_starts_ends_with) {
	string ts1 { "abc123def" };

	common_startsends_tests(ts1);
}

TEST(strings, view_starts_ends_with) {
	frg::string_view ts1 { "abc123def" };

	common_startsends_tests(ts1);
}

TEST(strings, operator_equals_comparison) {
	string s1 { "Hello" }, s2 { "World" }, s3 { "Hello" };

	EXPECT_NE(s1, s2);
	EXPECT_NE(s2, s3);
	EXPECT_EQ(s1, s3);
}

TEST(strings, compare_method_comparison) {
	string s1 { "AAA" }, s2 { "AAB" }, s3 { "AA" }, s4 { "AAA" };

	EXPECT_EQ(s1.compare(s2), -1);
	EXPECT_EQ(s2.compare(s1), 1);
	EXPECT_EQ(s1.compare(s3), 1);
	EXPECT_EQ(s3.compare(s1), -1);
	EXPECT_EQ(s1.compare(s4), 0);
}

TEST(pcg32, pcg32_brief_test) {
	frg::pcg_basic32 x { 12345, 6 };

	EXPECT_EQ(x(), 1985316396);
	EXPECT_EQ(x(), 1977560913);
	EXPECT_EQ(x(), 3056590845);
	EXPECT_EQ(x(), 1569990246);
	EXPECT_EQ(x(), 1699592177);
	EXPECT_EQ(x(), 1974316228);
	EXPECT_EQ(x(), 4283859071);
	EXPECT_EQ(x(), 3435412947);
	EXPECT_EQ(x(), 821999472);
	EXPECT_EQ(x(), 3498119420);

	EXPECT_EQ(x(10), 5);
	EXPECT_EQ(x(20), 12);
	EXPECT_EQ(x(30), 29);
	EXPECT_EQ(x(40), 6);
	EXPECT_EQ(x(50), 35);
	EXPECT_EQ(x(60), 46);
	EXPECT_EQ(x(70), 36);
	EXPECT_EQ(x(80), 69);
	EXPECT_EQ(x(90), 76);
	EXPECT_EQ(x(100), 68);
}

#include <frg/tuple.hpp>
#include <tuple> /* std::tuple_size and std::tuple_element */

TEST(tuples, basic_test) {
	int x = 5;
	int y = 7;

	frg::tuple<int, int> t { x, y };
	EXPECT_EQ(x, t.get<0>());
	EXPECT_EQ(y, t.get<1>());

	auto t2 = frg::make_tuple(x, y);
	EXPECT_EQ(x, t2.get<0>());
	EXPECT_EQ(y, t2.get<1>());
	t2.get<0>() = 1;
	t2.get<1>() = 2;
	EXPECT_EQ(t2.get<0>(), 1);
	EXPECT_EQ(t2.get<1>(), 2);
	EXPECT_NE(x, t2.get<0>());
	EXPECT_NE(y, t2.get<1>());

	static_assert(
		std::tuple_size_v<decltype(t)> == 2,
		"tuple_size produces wrong result"
	);
	static_assert(
		std::is_same_v<std::tuple_element_t<1, decltype(t)>, int>,
		"tuple_element produces wrong result"
	);
}

struct uncopyable {
	uncopyable() {}
	uncopyable(const uncopyable &) = delete;
	uncopyable(uncopyable &&) {}
	uncopyable &operator =(const uncopyable &) = delete;
	uncopyable &operator =(uncopyable &&other) { return *this; }
};

struct immovable {
	immovable() {}
	immovable(const immovable &) {}
	immovable(immovable &&) = delete;
	immovable &operator=(const immovable &) { return *this; }
	immovable &operator=(immovable &&) = delete;
};

TEST(tuples, reference_test) {
	int x = 5;
	uncopyable y {};
	immovable z {};

	frg::tuple<int&, uncopyable&, immovable&> t { x, y, z };
	EXPECT_EQ(&x, &t.get<0>());
	EXPECT_EQ(&y, &t.get<1>());
	EXPECT_EQ(&z, &t.get<2>());

	auto t2 = std::move(t);
	EXPECT_EQ(&x, &t2.get<0>());
	EXPECT_EQ(&y, &t2.get<1>());
	EXPECT_EQ(&z, &t2.get<2>());
}
