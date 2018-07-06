#ifndef FRG_PAIRING_HEAP_HPP
#define FRG_PAIRING_HEAP_HPP

#include <frg/macros.hpp>
#include <frg/utility.hpp>

namespace frg FRG_VISIBILITY {

namespace _pairing {

// Each element of a pairing heap consists of three pointers:
// * child: Points to the first child.
// * backlink: Points to the previous sibling or (if the element is the first
//   child of its parent) to the element's parent.
// * sibling: Points to the next sibling.
// The backlink is required to support deletion of arbitrary elements.
template<typename T>
struct pairing_heap_hook {
	pairing_heap_hook()
	: child{nullptr}, backlink{nullptr}, sibling{nullptr} { }

	pairing_heap_hook(const pairing_heap_hook &) = delete;

	~pairing_heap_hook() {
		FRG_ASSERT(!child);
		FRG_ASSERT(!backlink && !sibling);
	}

	pairing_heap_hook &operator= (const pairing_heap_hook &) = delete;

	T *child;
	T *backlink;
	T *sibling;
};

// Tags for frg::composition.
struct locate { };
struct compare { };

template<typename T, typename Locate, typename Compare>
struct pairing_heap : private composition<locate, Locate>,
		private composition<compare, Compare> {
private:
	// ------------------------------------------------------------------------
	// Algorithm details.
	// ------------------------------------------------------------------------

	pairing_heap_hook<T> &h(T *p) {
		return get<locate>(this)(*p);
	}

	// Merges two heaps.
	T *_merge(T *a, T *b) {
		FRG_ASSERT(!h(a).backlink && !h(a).sibling);
		FRG_ASSERT(!h(b).backlink && !h(b).sibling);

		// Merge the smaller element into the greater one.
		if(get<compare>(this)(a, b)) {
			auto sibling = h(b).child;
			if(sibling) {
				FRG_ASSERT(h(sibling).backlink == b);
				h(sibling).backlink = a;
			}
			h(a).sibling = sibling;
			h(a).backlink = b;
			h(b).child = a;
			return b;
		}else{
			auto sibling = h(a).child;
			if(sibling) {
				FRG_ASSERT(h(sibling).backlink == a);
				h(sibling).backlink = b;
			}
			h(b).sibling = sibling;
			h(b).backlink = a;
			h(a).child = b;
			return a;
		}
	}

	// Takes a doubly linked list of heaps (via the backlink/sibling pointers)
	// and merges them all into a single heap.
	T *_collapse(T *head) {
		FRG_ASSERT(head);

		// First we build a singly linked list of merged pairs.
		// We store this list in the backlink pointer.
		T *paired = nullptr;

		T *element = head;
		while(element && h(element).sibling) {
			auto partner = h(element).sibling;
			auto next = h(partner).sibling;

			// Detach both elements from the heap.
			h(element).backlink = nullptr;
			h(element).sibling = nullptr;

			FRG_ASSERT(h(partner).backlink == element);
			h(partner).backlink = nullptr;
			h(partner).sibling = nullptr;

			// Actually merge the pairs and build the linked list.
			auto merged = _merge(element, partner);
			FRG_ASSERT(!h(merged).backlink);
			h(merged).backlink = paired;
			paired = merged;

			element = next;
		}

		// Now merge all elements of the linked list.
		// This should be done in the reserve order of inserting them.
		T *joined;
		if(element) {
			// There is a single element remaining that is not part
			// of the linked list.
			h(element).backlink = nullptr;
			joined = element;
		}else{
			auto predecessor = h(paired).backlink;

			// There is no element remaining.
			// Take the first element from the linked list instead.
			h(paired).backlink = nullptr;
			FRG_ASSERT(!h(paired).sibling);
			joined = paired;

			paired = predecessor;
		}

		while(paired) {
			auto predecessor = h(paired).backlink;

			// Remove an element of the linked list and merge it with all others.
			h(paired).backlink = nullptr;
			FRG_ASSERT(!h(paired).sibling);
			joined = _merge(joined, paired);

			paired = predecessor;
		}

		return joined;
	}

	// ------------------------------------------------------------------------
	// Interface functions.
	// ------------------------------------------------------------------------
public:
	pairing_heap()
	: _root{nullptr} { }

	pairing_heap(const pairing_heap &) = delete;
	
	~pairing_heap() {
		FRG_ASSERT(!_root);
	}

	pairing_heap &operator= (const pairing_heap &) = delete;

	bool empty() {
		return _root == nullptr;
	}

	void push(T *element) {
		FRG_ASSERT(!h(element).child);
		FRG_ASSERT(!h(element).backlink && !h(element).sibling);
		
		if(_root) {
			_root = _merge(_root, element);
		}else{
			_root = element;
		}
	}

	void pop() {
		FRG_ASSERT(_root);
		auto child = h(_root).child;
		
		// Remove the root from the heap.
		h(_root).child = nullptr;
		FRG_ASSERT(!h(_root).backlink && !h(_root).sibling);

		// Fix up the remaining heap.
		if(child) {
			FRG_ASSERT(h(child).backlink == _root);
			h(child).backlink = nullptr;
			_root = _collapse(child);
		}else{
			_root = nullptr;
		}
	}

	void remove(T *element) {
		if(_root == element) {
			pop();
		}else{
			auto predecessor = h(element).backlink;
			auto sibling = h(element).sibling;
			auto child = h(element).child;
			FRG_ASSERT(predecessor);
			
			if(h(predecessor).child == element) {
				h(predecessor).child = sibling;
			}else{
				FRG_ASSERT(h(predecessor).sibling == element);
				h(predecessor).sibling = sibling;
			}

			if(sibling)
				h(sibling).backlink = predecessor;

			if(child) {
				FRG_ASSERT(h(child).backlink == element);
				h(child).backlink = nullptr;
				_root = _merge(_root, _collapse(child));
			}

			h(element).backlink = nullptr;
			h(element).sibling = nullptr;
			h(element).child = nullptr;
		}
	}

	T *top() {
		return _root;
	}

private:
	T *_root;
};

} // namespace _pairing

using _pairing::pairing_heap_hook;
using _pairing::pairing_heap;

} // namespace frg

#endif // FRG_PAIRING_HEAP_HPP
