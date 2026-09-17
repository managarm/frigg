#include <frg/eternal.hpp>
#include <frg/spinlock.hpp>

#include <gtest/gtest.h>

#include <type_traits>

namespace {

int constructions;
int destructions;

struct counted {
	counted()
	: value{42} {
		++constructions;
	}

	~counted() {
		++destructions;
	}

	int value;
};

// simple_spinlock is trivially destructible by construction, unlike
// std::mutex, whose destructor is only trivial on some implementations.
template<typename T>
using lazy = frg::lazy_eternal<T, frg::simple_spinlock>;

constinit lazy<counted> global_object;

} // anonymous namespace

static_assert(std::is_trivially_destructible_v<lazy<counted>>);

TEST(eternal, lazy_constructs_on_first_access) {
	constructions = 0;
	destructions = 0;

	{
		lazy<counted> object;
		EXPECT_EQ(constructions, 0);

		EXPECT_EQ(object.get().value, 42);
		EXPECT_EQ(constructions, 1);
	}

	// lazy_eternal never runs T's destructor.
	EXPECT_EQ(destructions, 0);
}

TEST(eternal, lazy_constructs_only_once) {
	constructions = 0;

	lazy<counted> object;
	auto *first = &object.get();
	for(int i = 0; i < 10; ++i) {
		EXPECT_EQ(&object.get(), first);
		EXPECT_EQ(&*object, first);
		EXPECT_EQ(object.operator-> (), first);
	}

	EXPECT_EQ(constructions, 1);
}

TEST(eternal, lazy_is_usable_as_constinit_global) {
	EXPECT_EQ(global_object->value, 42);
}
