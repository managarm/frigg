#include <frg/safe_int.hpp>

#include <gtest/gtest.h>

#include <limits>

TEST(safe_int, arithmetic) {
	frg::safe_int<int> x{10};
	frg::safe_int<int> y{5};

	EXPECT_EQ((x + y).expected().value(), 15);
	EXPECT_EQ((x - y).expected().value(), 5);
	EXPECT_EQ((x * y).expected().value(), 50);
}

TEST(safe_int, errors) {
	frg::safe_int<int> max { std::numeric_limits<int>::max() };
	frg::safe_int<int> min { std::numeric_limits<int>::min() };

	EXPECT_FALSE((max + frg::safe_int<int>{1}).expected());
	EXPECT_FALSE((min - frg::safe_int<int>{1}).expected());
	EXPECT_FALSE((max * frg::safe_int<int>{2}).expected());

	auto invalid = max + frg::safe_int<int>::invalid();
	EXPECT_FALSE((invalid + frg::safe_int<int>{1}).expected());
}
