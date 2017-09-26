#ifndef FRG_RCU_RADIXTREE_HPP
#define FRG_RCU_RADIXTREE_HPP

#include <stdint.h>
#include <atomic>
#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

template<typename T, typename Allocator>
T *construct(Allocator *allocator) {
	auto p = allocator->allocate(sizeof(T));
	return new (p) T;
}

template<typename T, typename Allocator>
struct rcu_radixtree {
private:
	static constexpr unsigned int ll = 15;

	uint64_t pfx_of(uint64_t k, unsigned int d) {
		return k & (uint64_t(-1) << (64 - d * 4));
	}

	unsigned int idx_of(uint64_t k, unsigned int d) {
		return (k >> (64 - (d + 1) * 4) & 0xF);
	}

	struct node {
		uint64_t prefix;
		unsigned int depth;
	};

	struct link_node : node {
		std::atomic<node *> links[16];
	};

	struct entry_node : node {
		std::atomic<uint16_t> mask;
		T entries[16];
	};

public:
	rcu_radixtree(Allocator *allocator)
	: _allocator{allocator}, _root{nullptr} {}

	T *find(uint64_t k) {
		auto n = _root.load(std::memory_order_acquire);
		while(true) {
			if(!n)
				return nullptr;
			if(pfx_of(k, n->depth) != n->prefix)
				return nullptr;

			auto idx = idx_of(k, n->depth);
			if(n->depth == ll) {
				auto cn = static_cast<entry_node *>(n);
				if(!(cn->mask.load(std::memory_order_relaxed) & (uint16_t(1) << idx)))
					return nullptr;
				return &cn->entries[idx];
			}else{
				auto cn = static_cast<link_node *>(n); 
				n = cn->links[idx].load(std::memory_order_acquire);
			}
		}
	}

	template<typename X>
	T *insert(uint64_t k, X &&initializer) {
		// p will be the node that we insert into.
		node *p = nullptr;
		node *s = _root.load(std::memory_order_acquire);
		while(true) {
			// First case: We insert a last-level node into an inner node.
			if(!s) {
//				std::cout << "Case 1" << std::endl;
				auto n = construct<entry_node>(_allocator);
				n->prefix = pfx_of(k, ll);
				n->depth = ll;
				n->mask.store(uint16_t(1) << idx_of(k, ll), std::memory_order_relaxed);
				for(int i = 0; i < 16; i++)
					n->entries[i] = initializer;

				if(p) {
					auto cp = static_cast<link_node *>(p);
					cp->links[idx_of(k, p->depth)].store(n, std::memory_order_release);
				}else{
					_root.store(n, std::memory_order_release);
				}
				return &n->entries[idx_of(k, ll)];
			}

			// Second case: We insert a new inner node and a last-level node.
			// s is the sibling of the new last-level node.
			if(pfx_of(k, s->depth) != s->prefix) {
//				std::cout << "Case 2" << std::endl;
				auto n = construct<entry_node>(_allocator);
				n->prefix = pfx_of(k, ll);
				n->depth = ll;
				n->mask.store(uint16_t(1) << idx_of(k, ll), std::memory_order_relaxed);
				for(int i = 0; i < 16; i++)
					n->entries[i] = initializer;
		
				// Determine the common prefix of k and s->prefix.
				unsigned int d = 0;
				while(pfx_of(k, d + 1) == pfx_of(s->prefix, d + 1))
					d++;
				FRG_ASSERT(!p || d > p->depth);
				FRG_ASSERT(d < s->depth);
				FRG_ASSERT(idx_of(k, d) != idx_of(s->prefix, d));
//				std::cout << "d = " << d << std::endl;

				auto r = construct<link_node>(_allocator);
				
				r->prefix = pfx_of(k, d);
				r->depth = d;
				for(int i = 0; i < 16; ++i)
					r->links[i].store(nullptr, std::memory_order_relaxed);
				r->links[idx_of(k, d)].store(n, std::memory_order_relaxed);
				r->links[idx_of(s->prefix, d)].store(s, std::memory_order_relaxed);
//				std::cout << "idx: " << idx_of(k, d) << " and " << idx_of(s->prefix, d) << std::endl;

				if(p) {
					auto cp = static_cast<link_node *>(p);
					cp->links[idx_of(k, p->depth)].store(r, std::memory_order_release);
				}else{
					_root.store(r, std::memory_order_release);
				}
				return &n->entries[idx_of(k, ll)];
			}

			// Third case: We directly insert into a last-level node.
			auto idx = idx_of(k, s->depth);
			if(s->depth == ll) {
//				std::cout << "Case 3" << std::endl;
				auto cs = static_cast<entry_node *>(s);
				auto mask = cs->mask.load(std::memory_order_relaxed);
				FRG_ASSERT(!(mask & (uint16_t(1) << idx)));
				cs->mask.store(mask | (uint16_t(1) << idx), std::memory_order_relaxed);
				return &cs->entries[idx];
			}else{
				auto cs = static_cast<link_node *>(s);
				p = s;
				s = static_cast<node *>(cs->links[idx].load(std::memory_order_acquire));
			}
		}
	}
	
	void erase(uint64_t k) {
		auto n = _root.load(std::memory_order_acquire);
		while(true) {
			FRG_ASSERT(n);
			FRG_ASSERT(pfx_of(k, n->depth) == n->prefix);

			auto idx = idx_of(k, n->depth);
			if(n->depth == ll) {
				auto cn = static_cast<entry_node *>(n);
				auto mask = cn->mask.load(std::memory_order_relaxed);
				FRG_ASSERT(mask & (uint16_t(1) << idx));
				cn->mask.store(mask & ~(uint16_t(1) << idx), std::memory_order_relaxed);
				return;
			}else{
				auto cn = static_cast<link_node *>(n);
				n = static_cast<node *>(cn->links[idx].load(std::memory_order_acquire));
			}
		}
	}

private:
	Allocator *_allocator;
	std::atomic<node *> _root;
};

} // namespace frg

#endif // FRG_RCU_RADIXTREE_HPP
