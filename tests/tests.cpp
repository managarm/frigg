#include <frg/string.hpp>
#include <frg/std_compat.hpp>

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
