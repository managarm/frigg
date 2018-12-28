#ifndef FRG_INTERVAL_TREE_HPP
#define FRG_INTERVAL_TREE_HPP

#include <frg/macros.hpp>
#include <frg/rbtree.hpp>

namespace frg FRG_VISIBILITY {

template<typename P>
struct interval_hook {
	P subtree_max;
};

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

	template<typename F>
	void for_overlaps(F fn, P lb, P ub) {
		auto root = _rbtree.get_root();
		if(!root)
			return;
		_for_overlaps_in_subtree(fn, lb, ub, root);
	}

	template<typename F>
	bool _for_overlaps_in_subtree(F &fn, int lb, int ub, T *node) {
		FRG_ASSERT(node);

		auto left = binary_tree::get_left(node);
		auto right = binary_tree::get_right(node);

		if((lb >= lower(node) && lb <= upper(node))
				|| (lower(node) >= lb && lower(node) <= ub)) {
			fn(node);

			if(left)
				_for_overlaps_in_subtree(fn, lb, ub, left);
			if(right)
				_for_overlaps_in_subtree(fn, lb, ub, right);
			return true;
		}

		if(left && h(node)->subtree_max >= lb) {
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
