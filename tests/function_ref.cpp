#include <frg/functional.hpp>

#include <gtest/gtest.h>

#include <memory>

namespace {

int increment(int &x) {
	return ++x;
}

} // anonymous namespace

TEST(function_ref, refers_to_callable) {
	struct counting {
		int operator() () { return ++calls; }
		int calls = 0;
	};

	counting c;
	frg::function_ref<int()> fn = c;
	fn();
	fn();
	EXPECT_EQ(c.calls, 2);
}

TEST(function_ref, const_callable) {
	struct constant {
		int operator() () const { return 42; }
	};

	const constant c;
	frg::function_ref<int()> fn = c;
	EXPECT_EQ(fn(), 42);
}

TEST(function_ref, void_return_discards_result) {
	int calls = 0;
	auto lambda = [&] (int x) { calls++; return x; };
	frg::function_ref<void(int)> fn = lambda;
	fn(1);
	fn(2);
	EXPECT_EQ(calls, 2);
}

TEST(function_ref, function_pointer) {
	int x = 0;

	frg::function_ref<int(int &)> fn = increment;
	EXPECT_EQ(fn(x), 1);

	frg::function_ref<void(int &)> void_fn = increment;
	void_fn(x);
	EXPECT_EQ(x, 2);
}

TEST(function_ref, reference_and_move_only_args) {
	auto lambda = [] (int &out, std::unique_ptr<int> p) { out = *p; };
	frg::function_ref<void(int &, std::unique_ptr<int>)> fn = lambda;

	int out = 0;
	fn(out, std::make_unique<int>(7));
	EXPECT_EQ(out, 7);
}

namespace {

struct member_callable {
	int get() { return 0; }
};

struct void_callable {
	void operator() () const { }
};

} // anonymous namespace

static_assert(std::is_trivially_copyable_v<frg::function_ref<void()>>);
static_assert(!std::is_constructible_v<frg::function_ref<int(member_callable &)>,
		int (member_callable::*)()>);
static_assert(!std::is_assignable_v<frg::function_ref<void()> &, void_callable>);
static_assert(std::is_assignable_v<frg::function_ref<void()> &, void_callable &>);
static_assert(std::is_assignable_v<frg::function_ref<void()> &, void (*)()>);
static_assert(std::is_assignable_v<frg::function_ref<void()> &, const frg::function_ref<void()>>);
static_assert(!std::is_constructible_v<frg::function_ref<void(int)>, int>);
static_assert(!std::is_constructible_v<frg::function_ref<int()>, void (*)()>);
