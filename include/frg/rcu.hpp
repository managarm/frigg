#pragma once

namespace frg FRG_VISIBILITY {

template<typename B, typename D>
concept is_rcu_obj_base = requires(B node, D d) {
	{ node.retire(d) } -> std::same_as<void>;
};

template<typename Rcu>
struct rcu_obj_archetype;

// Helper for rcu_policy concept.
template<typename Rcu>
struct rcu_deleter_archetype {
	void operator() (rcu_obj_archetype<Rcu> *) { }
};

// Helper for rcu_policy concept.
template<typename Rcu>
struct rcu_obj_archetype
: Rcu::template obj_base<rcu_obj_archetype, rcu_deleter_archetype<Rcu>> {};

// rcu_policy allows us to abstract over the RCU implementation.
// In the future, we will be able to provide an std:: based implementation.
// However, freestanding users may want to use their own implementations.
// rcu_policy requires a nested obj_base<T, D> template that satisfies is_rcu_base<D>.
template<typename Rcu>
concept rcu_policy = is_rcu_obj_base<
	typename Rcu::template obj_base<rcu_obj_archetype<Rcu>, rcu_deleter_archetype<Rcu>>,
	rcu_deleter_archetype<Rcu>
>;

} // namespace frg
