#ifndef FRG_INTERVAL_TREE_HPP
#define FRG_INTERVAL_TREE_HPP

#include <frg/macros.hpp>
#include <frg/rbtree.hpp>

namespace frg FRG_VISIBILITY {

template<typename P>
struct interval_hook {
	P subtree_max;
};

// Reference: Cormen et al., 'Introduction to Algorithms', 3rd edition.
template<
	typename T,
	typename P,
	P T:: *L,
	P T:: *U,
	rbtree_hook T ::*R,
	interval_hook<P> T:: *H
>
struct interval_tree {
	static P lower(const T *node) {
		return node->*L;
	}
	static P upper(const T *node) {
		return node->*U;
	}

	static interval_hook<P> *h(T *node) {
		return &(node->*H);
	}

	struct lb_less {
		bool operator() (const T &x, const T &y) const {
			return lower(&x) < lower(&y);
		}
	};

	struct aggregator;

	using binary_tree = rbtree<
		T,
		R,
		lb_less,
		aggregator
	>;

	struct aggregator {
		static bool aggregate(T *node) {
			auto left = binary_tree::get_left(node);
			auto right = binary_tree::get_right(node);

			P new_max = upper(node);
			if (left && new_max < h(left)->subtree_max)
				new_max = h(left)->subtree_max;
			if (right && new_max < h(right)->subtree_max)
				new_max = h(right)->subtree_max;

			if(new_max == h(node)->subtree_max)
				return false;
			h(node)->subtree_max = new_max;
			return true;
		}

		static bool check_invariant(binary_tree &tree, T *node) {
			return true;
		}
	};

	void insert(T *node) {
		FRG_ASSERT(lower(node) <= upper(node));
		h(node)->subtree_max = upper(node);
		_rbtree.insert(node);
	}

	void remove(T *node) {
		_rbtree.remove(node);
	}

	template<typename F>
	void for_overlaps(F fn, P lb, P ub) {
		auto root = _rbtree.get_root();
		if(!root)
			return;
		_for_overlaps_in_subtree(fn, lb, ub, root);
	}

	template<typename F>
	void for_overlaps(F fn, P singleton) {
		for_overlaps(std::forward<F>(fn), singleton, singleton);
	}

	template<typename F>
	bool _for_overlaps_in_subtree(F &fn, P lb, P ub, T *node) {
		FRG_ASSERT(node);

		auto left = binary_tree::get_left(node);
		auto right = binary_tree::get_right(node);

		if((lower(node) <= lb && lb <= upper(node))
				|| (lb <= lower(node) && lower(node) <= ub)) {
			fn(node);

			if(left)
				_for_overlaps_in_subtree(fn, lb, ub, left);
			if(right)
				_for_overlaps_in_subtree(fn, lb, ub, right);
			return true;
		}

		if(left && lb <= h(left)->subtree_max) {
			// If the preceeding if guarantees the following property:
			// If an overlapping interval exists, such an interval is in the left subtree
			// (but not *all* overlapping intervals need to be in the left subtree).
			// Thus, our strategy is to check the left subtree first and *only* if an interval
			// exists in the left subtree, we *also* check the right subtree.
			if(_for_overlaps_in_subtree(fn, lb, ub, left)) {
				if(right)
					_for_overlaps_in_subtree(fn, lb, ub, right);
				return true;
			}
		}else if(right) {
			if(_for_overlaps_in_subtree(fn, lb, ub, right))
				return true;
		}

		return false;
	}

private:
	binary_tree _rbtree;
};

} // namespace frg

#endif // FRG_INTERVAL_TREE_HPP
