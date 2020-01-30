#ifndef FRG_RCU_RADIXTREE_HPP
#define FRG_RCU_RADIXTREE_HPP

#include <stdint.h>
#include <atomic>
#include <new>
#include <frg/allocation.hpp>
#include <frg/eternal.hpp>
#include <frg/macros.hpp>
#include <frg/tuple.hpp>

namespace frg FRG_VISIBILITY {

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

	struct link_node;
	struct entry_node;

	struct node {
		uint64_t prefix;
		unsigned int depth;
		link_node *parent;
	};

	struct link_node : node {
		std::atomic<node *> links[16];
	};

	struct entry_node : node {
		std::atomic<uint16_t> mask;
		aligned_storage<sizeof(T), alignof(T)> entries[16];
	};

public:
	rcu_radixtree(Allocator allocator = Allocator())
	: _allocator{std::move(allocator)}, _root{nullptr} {}

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
				if(!(cn->mask.load(std::memory_order_acquire) & (uint16_t(1) << idx)))
					return nullptr;
				return std::launder(reinterpret_cast<T *>(cn->entries[idx].buffer));
			}else{
				auto cn = static_cast<link_node *>(n); 
				n = cn->links[idx].load(std::memory_order_acquire);
			}
		}
	}

	template<typename... Args>
	tuple<T *, bool> find_or_insert(uint64_t k, Args &&... args) {
		// p will be the node that we insert into.
		link_node *p = nullptr;
		node *s = _root.load(std::memory_order_acquire);
		while(true) {
			// First case: We insert a last-level node into an inner node.
			if(!s) {
//				std::cout << "Case 1" << std::endl;
				auto n = construct<entry_node>(_allocator);
				n->prefix = pfx_of(k, ll);
				n->depth = ll;
				n->parent = p;
				n->mask.store(uint16_t(1) << idx_of(k, ll), std::memory_order_relaxed);

				auto entry = new (n->entries[idx_of(k, ll)].buffer) T{std::forward<Args>(args)...};

				if(p) {
					auto cp = static_cast<link_node *>(p);
					cp->links[idx_of(k, p->depth)].store(n, std::memory_order_release);
				}else{
					_root.store(n, std::memory_order_release);
				}
				return {entry, true};
			}

			// Second case: We insert a new inner node and a last-level node.
			// s is the sibling of the new last-level node.
			if(pfx_of(k, s->depth) != s->prefix) {
//				std::cout << "Case 2" << std::endl;
				auto n = construct<entry_node>(_allocator);
				auto r = construct<link_node>(_allocator);

				n->prefix = pfx_of(k, ll);
				n->depth = ll;
				n->parent = r;
				n->mask.store(uint16_t(1) << idx_of(k, ll), std::memory_order_relaxed);

				auto entry = new (n->entries[idx_of(k, ll)].buffer) T{std::forward<Args>(args)...};

				s->parent = r;

				// Determine the common prefix of k and s->prefix.
				unsigned int d = 0;
				while(pfx_of(k, d + 1) == pfx_of(s->prefix, d + 1))
					d++;
				FRG_ASSERT(!p || d > p->depth);
				FRG_ASSERT(d < s->depth);
				FRG_ASSERT(idx_of(k, d) != idx_of(s->prefix, d));
//				std::cout << "d = " << d << std::endl;

				r->prefix = pfx_of(k, d);
				r->depth = d;
				r->parent = p;
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
				return {entry, true};
			}

			// Third case: We directly insert into a last-level node.
			auto idx = idx_of(k, s->depth);
			if(s->depth == ll) {
//				std::cout << "Case 3" << std::endl;
				auto cs = static_cast<entry_node *>(s);
				auto mask = cs->mask.load(std::memory_order_acquire);
				FRG_ASSERT(!(mask & (uint16_t(1) << idx)));

				auto entry = new (cs->entries[idx].buffer) T{std::forward<Args>(args)...};

				cs->mask.store(mask | (uint16_t(1) << idx), std::memory_order_release);
				return {entry, true};
			}else{
				auto cs = static_cast<link_node *>(s);
				p = cs;
				s = static_cast<node *>(cs->links[idx].load(std::memory_order_acquire));
			}
		}
	}

	template<typename... Args>
	T *insert(uint64_t k, Args &&... args) {
		auto ins = find_or_insert(k, std::forward<Args>(args)...);
		FRG_ASSERT(ins.template get<1>());
		return ins.template get<0>();
	}

	void erase(uint64_t k) {
		auto n = _root.load(std::memory_order_acquire);
		while(true) {
			FRG_ASSERT(n);
			FRG_ASSERT(pfx_of(k, n->depth) == n->prefix);

			auto idx = idx_of(k, n->depth);
			if(n->depth == ll) {
				auto cn = static_cast<entry_node *>(n);
				auto mask = cn->mask.load(std::memory_order_acquire);
				FRG_ASSERT(mask & (uint16_t(1) << idx));

				cn->mask.store(mask & ~(uint16_t(1) << idx), std::memory_order_release);
				return;
			}else{
				auto cn = static_cast<link_node *>(n);
				n = static_cast<node *>(cn->links[idx].load(std::memory_order_acquire));
			}
		}
	}

private:
	// Helper function for iteration.
	static entry_node *first_leaf(node *n) {
		if(!n)
			return nullptr;
		while(true) {
			if(n->depth == ll)
				return static_cast<entry_node *>(n);

			auto cn = static_cast<link_node *>(n);

			node *m = nullptr;
			for(unsigned int idx = 0; idx < 16; idx++) {
				m = cn->links[idx].load(std::memory_order_relaxed);
				if(m)
					break;
			}
			FRG_ASSERT(m);
			n = m;
		}
	}

	// Helper function for iteration.
	static entry_node *next_leaf(node *n) {
		while(true) {
			auto p = n->parent;
			if(!p)
				return nullptr;

			// Find the index of n in its parent.
			unsigned int pidx;
			for(pidx = 0; pidx < 16; pidx++) {
				if(n == p->links[pidx].load(std::memory_order_relaxed))
					break;
			}
			FRG_ASSERT(pidx < 16);

			// Check if there is a sibling.
			for(unsigned int idx = pidx + 1; idx < 16; idx++) {
				auto m = p->links[idx].load(std::memory_order_relaxed);
				if(m)
					return first_leaf(m);
			}

			n = p;
		}
	}

public:
	// Note: The iterator interface is *not* safe in the presence of concurrent modification.
	//       Only use this interface while there are no concurrent writers.
	struct iterator {
		explicit iterator()
		: _n{nullptr}, _idx{16} { }

		explicit iterator(entry_node *n, unsigned int idx)
		: _n{n}, _idx{idx} { }

		void operator++ () {
			FRG_ASSERT(_idx < 16);
			_idx++;

			while(true) {
				// Try to find a present entry.
				auto mask = _n->mask.load(std::memory_order_relaxed);
				while(_idx < 16) {
					if(mask & (1 << _idx))
						return;
					_idx++;
				}

				// Inspect the next leaf instead.
				_n = next_leaf(_n);
				if(!_n)
					return;
				_idx = 0;
			}
		}

		T &operator* () {
			return *std::launder(reinterpret_cast<T *>(_n->entries[_idx].buffer));
		}
		T *operator-> () {
			return std::launder(reinterpret_cast<T *>(_n->entries[_idx].buffer));
		}

		bool operator== (const iterator &other) {
			return _n == other._n && _idx == other._idx;
		}
		bool operator!= (const iterator &other) {
			return !(*this == other);
		}

	private:
		entry_node *_n;
		unsigned int _idx;
	};

	iterator begin() {
		auto n = first_leaf(_root.load(std::memory_order_relaxed));
		while(true) {
			if(!n)
				return iterator{};

			// Try to find a present entry.
			for(unsigned int idx = 0; idx < 16; idx++) {
				if(n->mask.load(std::memory_order_relaxed) & (1 << idx))
					return iterator{n, idx};
			}

			n = next_leaf(n);
		}
	}

	iterator end() {
		return iterator{};
	}

private:
	Allocator _allocator;
	std::atomic<node *> _root;
};

} // namespace frg

#endif // FRG_RCU_RADIXTREE_HPP
