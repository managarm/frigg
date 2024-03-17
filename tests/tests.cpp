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

TEST(string_view, find) {
	frg::string_view s1 { "ABC" };

	EXPECT_EQ(s1.find_first('B'), 1);
	EXPECT_EQ(s1.find_first('D'), -1);
	EXPECT_EQ(s1.find_first_of("CB"), 1);
	EXPECT_EQ(s1.find_first_of("DE"), -1);
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

#include <frg/formatting.hpp>
#include <frg/logging.hpp>
#include <string> // std::string
#include <vector> // std::vector

TEST(formatting, basic_output_to) {
	std::string std_str;
	frg::output_to(std_str) << 10;
	EXPECT_EQ(std_str, "10");

	string frg_str;
	frg::output_to(frg_str) << 10;
	std::cout << frg_str.size() << std::endl;
	EXPECT_EQ(frg_str, "10");

	std::vector<char> std_vec;
	std::vector<char> expected_vec{'1', '0'};
	frg::output_to(std_vec) << 10;
	EXPECT_EQ(std_vec, expected_vec);
}

TEST(formatting, fmt) {
	std::string str;
	frg::output_to(str) << frg::fmt("Hello {}!", "world");
	EXPECT_EQ(str, "Hello world!");
	str.clear();

	frg::output_to(str) << frg::fmt("{} {:x}", 1234, 0x3456);
	EXPECT_EQ(str, "1234 3456");
	str.clear();

	int x = 10;
	frg::output_to(str) << frg::fmt("{} {}", x, x + 20);
	EXPECT_EQ(str, "10 30");
	str.clear();

	x = 20;
	frg::output_to(str) << frg::fmt("{:d} {:i}", x, x + 20);
	EXPECT_EQ(str, "20 40");
	str.clear();

	frg::output_to(str) << frg::fmt("{:08X}", 0xAAABBB);
	EXPECT_EQ(str, "00AAABBB");
	str.clear();

	frg::output_to(str) << frg::fmt("{:b}", 0b101010);
	EXPECT_EQ(str, "101010");
	str.clear();

	frg::output_to(str) << frg::fmt("{:08b}", 0b101010);
	EXPECT_EQ(str, "00101010");
	str.clear();

	frg::output_to(str) << frg::fmt("{:o}", 0777);
	EXPECT_EQ(str, "777");
	str.clear();

	frg::output_to(str) << frg::fmt("{:03o}", 077);
	EXPECT_EQ(str, "077");
	str.clear();

	frg::output_to(str) << frg::fmt("{1} {0}", 3, 4);
	EXPECT_EQ(str, "4 3");
	str.clear();

	frg::output_to(str) << frg::fmt("{1}", 1);
	EXPECT_EQ(str, "{1}");
	str.clear();

	frg::output_to(str) << frg::fmt("{{}", 1);
	EXPECT_EQ(str, "{}");
	str.clear();

	frg::output_to(str) << frg::fmt("{:h}", 1);
	EXPECT_EQ(str, "{:h}");
	str.clear();

	std::string abc_def { "abc def" };
	std::vector<char> abc_def_v { abc_def.cbegin(), abc_def.cend() };

	frg::output_to(str) << frg::fmt("testing! {}", abc_def);
	EXPECT_EQ(str, "testing! abc def");
	str.clear();

	frg::output_to(str) << frg::fmt("testing2! {}", abc_def_v);
	EXPECT_EQ(str, "testing2! abc def");
	str.clear();
}

#include <frg/printf.hpp>

TEST(formatting, printf) {
	std::string buf{};
	frg::container_logger sink{buf};

	struct test_agent {
		frg::expected<frg::format_error> operator() (char c) {
			sink_->append(c);
			return frg::success;
		}

		frg::expected<frg::format_error> operator() (const char *c, size_t n) {
			sink_->append(c, n);
			return frg::success;
		}

		frg::expected<frg::format_error> operator() (char t, frg::format_options opts,
				frg::printf_size_mod szmod) {
			switch(t) {
				case 'c':
					frg::do_printf_chars(*sink_, t, opts, szmod, vsp_);
					break;
				case 'p': case 's':
					frg::do_printf_chars(*sink_, t, opts, szmod, vsp_);
					break;
				case 'd': case 'i': case 'o': case 'x': case 'X': case 'u':
					frg::do_printf_ints(*sink_, t, opts, szmod, vsp_);
					break;
				default:
					// Should not be reached
					ADD_FAILURE();
			}

			return frg::success;
		}

		frg::container_logger<std::string> *sink_;
		frg::va_struct *vsp_;
	};

	auto do_test = [] (const char *expected, const char *format, ...) {
		va_list args;
		va_start(args, format);

		frg::va_struct vs;
		frg::arg arg_list[NL_ARGMAX + 1];
		vs.arg_list = arg_list;
		va_copy(vs.args, args);

		std::string buf;
		frg::container_logger<std::string> sink{buf};

		auto res = frg::printf_format(test_agent{&sink, &vs}, format, &vs);
		ASSERT_TRUE(res);

		ASSERT_STREQ(expected, buf.data());
	};

	do_test("12", "%d", 12);

	// Test %c right padding.
	do_test("a ", "%-2c", 'a');

	// Test %c left padding.
	do_test(" a", "%2c", 'a');

	// Test %d right padding.
	do_test("1 ", "%-2d", 1);
	do_test("01", "%-2.2d", 1);
	do_test("01 ", "%-3.2d", 1);
	do_test("12 ", "%-3.2d", 12);
	do_test("123", "%-3.2d", 123);
	do_test("12 ", "%-3.2u", 12);

	// Test %d left padding.
	do_test(" 1", "%2d", 1);
	do_test(" 01", "%3.2d", 1);
	do_test(" 12", "%3.2d", 12);
	do_test("123", "%3.2d", 123);
	do_test(" 12", "%3.2u", 12);

	// Test '+' and ' ' flags.
	do_test("+12", "%+d", 12);
	do_test(" 12", "% d", 12);
	do_test("+12", "% +d", 12);
	do_test("+12", "%+ d", 12);
	do_test("-12", "%+d", -12);
	do_test("-12", "% d", -12);
	do_test("-12", "% +d", -12);
	do_test("-12", "%+ d", -12);

	// Test '#' flag.
	do_test("0xc", "%#x", 12);
	do_test("0XC", "%#X", 12);
	do_test("014", "%#o", 12);
	do_test("0", "%#x", 0);
	do_test("0", "%#X", 0);
	do_test("0", "%#o", 0);

	// Test 'd' with different size mods to see
	// if they work
	do_test("12", "%d", 12);
	do_test("12", "%ld", 12L);
	do_test("12", "%lld", 12LL);
	do_test("12", "%zd", (size_t)12);
	do_test("12", "%hd", 12);
	do_test("12", "%hhd", 12);

	// Test 'x' with different size mods to see
	// if they work
	do_test("c", "%x", 12);
	do_test("c", "%lx", 12L);
	do_test("c", "%llx", 12LL);
	do_test("c", "%zx", (size_t)12);
	do_test("c", "%hx", 12);
	do_test("c", "%hhx", 12);

	// Test 'X' with different size mods to see
	// if they work
	do_test("C", "%X", 12);
	do_test("C", "%lX", 12L);
	do_test("C", "%llX", 12LL);
	do_test("C", "%zX", (size_t)12);
	do_test("C", "%hX", 12);
	do_test("C", "%hhX", 12);

	// Test 'o' with different size mods to see
	// if they work
	do_test("14", "%o", 12);
	do_test("14", "%lo", 12L);
	do_test("14", "%llo", 12LL);
	do_test("14", "%zo", (size_t)12);
	do_test("14", "%ho", 12);
	do_test("14", "%hho", 12);

	// Test 's' with precision.
	do_test("hello world", "%s", "hello world");
	do_test("hello", "%.5s", "hello world");
	do_test("hello", "%.*s", 5, "hello world");
	do_test("hello", "%.10s", "hello\0!!!!");
}

#include <frg/bitset.hpp>

TEST(bitset, bitwise) {
	constexpr auto A = 12346;
	constexpr auto B = 56789;
	constexpr auto C = 957929475;
	constexpr auto D = 9393939;
	frg::bitset<45> a = A;
	frg::bitset<45> b = B;

	a |= b;

	EXPECT_TRUE(a == frg::bitset<45>(A | B));

	frg::bitset<45> c = C;
	a &= c;

	EXPECT_TRUE(a == frg::bitset<45>(C & (A | B)));

	b ^= D;
	a |= b;

	EXPECT_TRUE(a == frg::bitset<45>((C & (A | B)) | (B ^ D)));
}

TEST(bitset, shift_left) {
	constexpr auto A = 1234;
	frg::bitset<64> sample = A;
	EXPECT_TRUE((sample >> 23) == frg::bitset<64>(A >> 23));
	frg::bitset<253> bs1;
	bs1.set(23).set(124).set(32).set(123).set(1).set(252);

	frg::bitset<253> bs2;
	bs2.set(23 + 12).set(124 + 12).set(32 + 12).set(123 + 12).set(1 + 12);

	auto bs3 = bs1 << 12;

	EXPECT_TRUE(bs3 == bs2);
}

TEST(bitset, setters_and_getters) {
	frg::bitset<12> a;
	a.set(13);
	EXPECT_TRUE(a.test(13));
	a.set(15);
	EXPECT_TRUE(a.test(15));
	a.reset(15);
	EXPECT_FALSE(a.test(15));

	a.set();
	for (std::size_t i = 0; i < 12; i++)
		EXPECT_TRUE(a.test(i));
	a.reset();
	for (std::size_t i = 0; i < 12; i++)
		EXPECT_FALSE(a.test(i));

	decltype(a)::reference r = a[4];
	r = true;

	EXPECT_TRUE(a.test(4));

	a.flip();
	for (std::size_t i = 0; i < 12; i++) {
		if (i == 4)
			EXPECT_FALSE(a.test(i));
		else
			EXPECT_TRUE(a[i]);
	}

	r.flip();
	for (std::size_t i = 0; i < 12; i++)
		EXPECT_TRUE(a.test(i));
}

TEST(bitset, count) {
	frg::bitset<24> a;
	EXPECT_TRUE(a.count() == 0);
	EXPECT_TRUE(a.none());
	EXPECT_FALSE(a.any());
	EXPECT_FALSE(a.all());
	a.flip();
	EXPECT_TRUE(a.count() == 24);
	EXPECT_TRUE(a.any());
	EXPECT_TRUE(a.all());
	a.reset();
	EXPECT_TRUE(a.set(13).set(4).count() == 2);
	EXPECT_TRUE(a.any());
	EXPECT_FALSE(a.all());
	EXPECT_FALSE(a.none());
}
