#include <cstdint>
#include <limits>
#include <map>
#include <random>
#include <string>
#include <utility>

#include <frg/rcu_btree.hpp>
#include <frg/std_compat.hpp>

#include <gtest/gtest.h>

struct fake_rcu_policy {
	template<typename T, typename D>
	struct obj_base {
		void retire(D d = D()) {
			auto derived = static_cast<T *>(this);
			d(derived);
		}
	};
};

using tree_type = frg::rcu_btree<int, int, frg::stl_allocator, fake_rcu_policy>;

TEST(rcu_btree, insert_and_find) {
	tree_type tree;

	EXPECT_EQ(tree.find(5), tree.end());
	ASSERT_TRUE(tree.check_invariant());

	EXPECT_EQ(*tree.insert(5, 50), std::pair(5, 50));
	EXPECT_EQ(*tree.insert(9, 90), std::pair(9, 90));
	ASSERT_TRUE(tree.check_invariant());
	EXPECT_EQ(*tree.find(5), std::pair(5, 50));
	EXPECT_EQ(*tree.find(9), std::pair(9, 90));
	EXPECT_EQ(tree.find(7), tree.end());

	// replace() returns the previous value.
	EXPECT_EQ(tree.replace(tree.find(5), 51), 50);
	EXPECT_EQ(*tree.find(5), std::pair(5, 51));
	ASSERT_TRUE(tree.check_invariant());
}

TEST(rcu_btree, value_types) {
	struct point {
		uint32_t x, y;
		bool operator==(const point &) const = default;
	};

	int a, b;
	frg::rcu_btree<int, int *, frg::stl_allocator, fake_rcu_policy> pointers;
	pointers.insert(1, &a);
	EXPECT_EQ(pointers.replace(pointers.find(1), &b), &a);
	EXPECT_EQ(*pointers.find(1), std::pair(1, &b));

	frg::rcu_btree<int, point, frg::stl_allocator, fake_rcu_policy> points;
	points.insert(1, {2, 3});
	EXPECT_EQ(points.erase(points.find(1)), (point{2, 3}));
	EXPECT_EQ(points.begin(), points.end());
}

TEST(rcu_btree, split) {
	// Enough entries to split leaves and to cascade splits through inner nodes.
	constexpr int n = 200;

	tree_type ascending;
	tree_type descending;
	for (int i = 0; i < n; ++i) {
		EXPECT_EQ(*ascending.insert(i, i), std::pair(i, i));
		EXPECT_EQ(*descending.insert(n - 1 - i, n - 1 - i), std::pair(n - 1 - i, n - 1 - i));
		ASSERT_TRUE(ascending.check_invariant());
		ASSERT_TRUE(descending.check_invariant());
	}

	for (int i = 0; i < n; ++i) {
		EXPECT_EQ(*ascending.find(i), std::pair(i, i));
		EXPECT_EQ(*descending.find(i), std::pair(i, i));
	}
	EXPECT_EQ(ascending.find(n), ascending.end());
}

TEST(rcu_btree, lower_bound) {
	tree_type tree;

	EXPECT_EQ(tree.lower_bound(0), tree.end());

	// Even keys 2, 4, ..., 400, i.e., enough for multiple levels of inner nodes.
	constexpr int n = 200;
	for (int i = 0; i < n; ++i)
		tree.insert(2 * (i + 1), i);
	ASSERT_TRUE(tree.check_invariant());

	// Exact matches and keys that fall into the gaps (including gaps between leaves).
	for (int i = 0; i < n; ++i) {
		EXPECT_EQ(*tree.lower_bound(2 * (i + 1)), std::pair(2 * (i + 1), i));
		EXPECT_EQ(*tree.lower_bound(2 * i + 1), std::pair(2 * (i + 1), i));
	}
	EXPECT_EQ(tree.lower_bound(2 * n + 1), tree.end());
}

TEST(rcu_btree, iterate) {
	tree_type tree;
	EXPECT_EQ(tree.begin(), tree.end());

	// Enough entries such that iteration crosses many leaves.
	constexpr int n = 200;
	for (int i = 0; i < n; ++i)
		tree.insert(2 * i, i);

	int i = 0;
	for (auto [k, v] : tree) {
		EXPECT_EQ(k, 2 * i);
		EXPECT_EQ(v, i);
		++i;
	}
	EXPECT_EQ(i, n);

	// Incrementing past the maximal key yields end().
	auto last = tree.insert(std::numeric_limits<int>::max(), n);
	EXPECT_EQ(++last, tree.end());
}

TEST(rcu_btree, erase) {
	tree_type tree;

	for (int i = 0; i < 8; ++i)
		tree.insert(i, i);

	// Erase from a leaf that keeps other keys.
	EXPECT_EQ(tree.erase(tree.find(2)), 2);
	ASSERT_TRUE(tree.check_invariant());
	EXPECT_EQ(tree.find(2), tree.end());
	EXPECT_EQ(*tree.lower_bound(2), std::pair(3, 3));

	// Erase everything else; this borrows from and merges with sibling leaves.
	for (int i = 0; i < 8; ++i) {
		if (i == 2)
			continue;
		EXPECT_EQ(tree.erase(tree.find(i)), i);
		ASSERT_TRUE(tree.check_invariant());
		for (int j = 0; j < 8; ++j)
			EXPECT_EQ(tree.find(j) != tree.end(), j > i && j != 2);
	}

	// The emptied tree accepts insertions again.
	tree.insert(0, 0);
	ASSERT_TRUE(tree.check_invariant());
	EXPECT_EQ(*tree.find(0), std::pair(0, 0));
}

TEST(rcu_btree, erase_rebalancing) {
	// Erasing from either end exercises borrowing from left/right siblings and
	// merging, both for leaves and for inner nodes.
	constexpr int n = 200;

	tree_type ascending;
	tree_type descending;
	for (int i = 0; i < n; ++i) {
		ascending.insert(i, i);
		descending.insert(i, i);
	}

	for (int i = 0; i < n; ++i) {
		EXPECT_EQ(ascending.erase(ascending.find(i)), i);
		EXPECT_EQ(descending.erase(descending.find(n - 1 - i)), n - 1 - i);
		ASSERT_TRUE(ascending.check_invariant());
		ASSERT_TRUE(descending.check_invariant());
		for (int j = 0; j < n; ++j) {
			EXPECT_EQ(ascending.find(j) != ascending.end(), j > i);
			EXPECT_EQ(descending.find(j) != descending.end(), j < n - 1 - i);
		}
	}
}

// Only supports the operations that rcu_btree requires of keys (in particular, no operator==).
// It wraps a string, hence it is not trivially copyable and its order differs from the numeric one.
struct minimal_key {
	minimal_key() = default;
	minimal_key(const minimal_key &) = default;
	minimal_key &operator=(const minimal_key &) = default;

	// Only used by the test itself.
	explicit minimal_key(std::string s)
	: s{std::move(s)} { }

	friend bool operator<(const minimal_key &a, const minimal_key &b) {
		return a.s < b.s;
	}

	std::string s;
};

TEST(rcu_btree, non_integer_keys) {
	frg::rcu_btree<minimal_key, int, frg::stl_allocator, fake_rcu_policy> tree;
	std::map<std::string, int> ref;
	EXPECT_EQ(tree.begin(), tree.end());

	// minimal_key has no operator==, hence compare via the wrapped string.
	auto expect_entry = [] (auto it, const std::string &k, int v) {
		auto [key, value] = *it;
		EXPECT_EQ(key.s, k);
		EXPECT_EQ(value, v);
	};

	// Enough entries for multiple levels of inner nodes.
	constexpr int n = 200;
	for (int i = 0; i < n; ++i) {
		auto k = std::to_string(i);
		expect_entry(tree.insert(minimal_key{k}, i), k, i);
		ref[k] = i;
	}
	ASSERT_TRUE(tree.check_invariant());

	for (int i = 0; i < n; ++i)
		expect_entry(tree.find(minimal_key{std::to_string(i)}), std::to_string(i), i);
	EXPECT_EQ(tree.find(minimal_key{"1a"}), tree.end());

	// Both present keys and keys that fall into the gaps (including before the first and after the last key).
	for (std::string q : {"", "0", "1", "10", "100", "1a", "5", "55", "99", "990", "a"}) {
		auto lb = tree.lower_bound(minimal_key{q});
		auto ub = tree.upper_bound(minimal_key{q});
		auto ref_lb = ref.lower_bound(q);
		auto ref_ub = ref.upper_bound(q);
		EXPECT_EQ(lb == tree.end(), ref_lb == ref.end());
		EXPECT_EQ(ub == tree.end(), ref_ub == ref.end());
		if (ref_lb != ref.end())
			expect_entry(lb, ref_lb->first, ref_lb->second);
		if (ref_ub != ref.end())
			expect_entry(ub, ref_ub->first, ref_ub->second);
	}

	// Erase every other key (this rebalances), then iterate over the rest.
	for (int i = 0; i < n; i += 2) {
		EXPECT_EQ(tree.erase(tree.find(minimal_key{std::to_string(i)})), i);
		ref.erase(std::to_string(i));
		ASSERT_TRUE(tree.check_invariant());
	}
	auto ref_it = ref.begin();
	for (auto it = tree.begin(); it != tree.end(); ++it, ++ref_it) {
		ASSERT_TRUE(ref_it != ref.end());
		expect_entry(it, ref_it->first, ref_it->second);
	}
	EXPECT_TRUE(ref_it == ref.end());
}

TEST(rcu_btree, random) {
	// Random operations in phases that alternately (mostly) insert and only erase.
	// The phases are long enough to fill the tree to multiple levels and to empty it again,
	// such that all cases of splitting, borrowing and merging are covered.
	constexpr int n = 256;
	constexpr int phase = 4096;

	tree_type tree;
	std::map<int, int> ref;
	std::mt19937 rng{42};
	for (int i = 0; i < 4 * phase; ++i) {
		int k = rng() % n;
		auto it = tree.find(k);
		ASSERT_EQ(it != tree.end(), ref.contains(k));
		if ((i / phase) % 2 == 0 && rng() % 8) {
			if (it == tree.end()) {
				EXPECT_EQ(*tree.insert(k, i), std::pair(k, i));
			} else {
				EXPECT_EQ(tree.replace(it, i), ref[k]);
			}
			ref[k] = i;
		} else if (it != tree.end()) {
			EXPECT_EQ(tree.erase(it), ref[k]);
			ref.erase(k);
		}
		ASSERT_TRUE(tree.check_invariant());

		// Iterate from a random key to the end.
		int q = rng() % (n + 1);
		auto ref_it = ref.lower_bound(q);
		for (auto it = tree.lower_bound(q); it != tree.end(); ++it, ++ref_it) {
			ASSERT_TRUE(ref_it != ref.end());
			EXPECT_EQ(*it, (std::pair<int, int>(*ref_it)));
		}
		EXPECT_TRUE(ref_it == ref.end());
	}
	EXPECT_TRUE(ref.empty());
}
