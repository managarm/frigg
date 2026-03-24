#pragma once

#include <atomic>
#include <iterator>
#include <type_traits>
#include <utility>
#include <frg/allocation.hpp>
#include <frg/intrusive.hpp>
#include <frg/macros.hpp>
#include <frg/utility.hpp>

namespace frg FRG_VISIBILITY {

namespace _list {

template<typename OwnerPointer, typename BorrowPointer>
struct intrusive_list_hook {
	using owner_pointer = OwnerPointer;
	using borrow_pointer = BorrowPointer;

	constexpr intrusive_list_hook()
	: next{nullptr}, previous{nullptr}, in_list{false} { }

	owner_pointer next;
	borrow_pointer previous;
	bool in_list;
};

struct locate_tag { };

template<typename T, typename Locate>
struct intrusive_list : private composition<locate_tag, Locate> {
private:
	using hook = std::remove_reference_t<std::invoke_result_t<Locate, T &>>;
	using owner_pointer = typename hook::owner_pointer;
	using borrow_pointer = typename hook::borrow_pointer;
	using traits = intrusive_traits<T, owner_pointer, borrow_pointer>;

	hook &h(borrow_pointer ptr) {
		return get<locate_tag>(this)(*ptr);
	}

public:
	struct iterator : private composition<locate_tag, Locate> {
	friend struct intrusive_list;
		using iterator_category = std::forward_iterator_tag;
		using iterator_concept = std::forward_iterator_tag;
		using difference_type = ptrdiff_t;
		using value_type = borrow_pointer;

	private:
		hook &h(borrow_pointer ptr) {
			return get<locate_tag>(this)(*ptr);
		}

	public:
		iterator(borrow_pointer current)
		: _current(current) { }

		iterator() : _current{nullptr} { }

		borrow_pointer operator* () const {
			return _current;
		}

		borrow_pointer operator-> () const {
			return _current;
		}

		bool operator== (const iterator &other) const {
			return _current == other._current;
		}
		bool operator!= (const iterator &other) const {
			return !(*this == other);
		}

		iterator &operator++ () {
			_current = h(_current).next;
			return *this;
		}
		iterator operator++ (int) {
			auto copy = *this;
			++(*this);
			return copy;
		}

	private:
		borrow_pointer _current;
	};

	iterator iterator_to(borrow_pointer ptr) {
		FRG_ASSERT(h(ptr).in_list);
		return iterator{ptr};
	}

	constexpr intrusive_list()
	: _front{nullptr}, _back{nullptr} { }

	iterator push_front(owner_pointer element) {
		FRG_ASSERT(element);
		borrow_pointer borrow = traits::decay(element);
		FRG_ASSERT(!h(borrow).in_list);
		FRG_ASSERT(!h(borrow).next);
		FRG_ASSERT(!h(borrow).previous);
		if(!_front) {
			_back = borrow;
		}else{
			h(borrow).next = std::move(_front);
			h(_front).previous = borrow;
		}
		_front = std::move(element);
		h(borrow).in_list = true;
		return iterator{borrow};
	}

	iterator push_back(owner_pointer element) {
		FRG_ASSERT(element);
		borrow_pointer borrow = traits::decay(element);
		FRG_ASSERT(!h(borrow).in_list);
		FRG_ASSERT(!h(borrow).next);
		FRG_ASSERT(!h(borrow).previous);
		if(!_back) {
			_front = std::move(element);
		}else{
			h(borrow).previous = _back;
			h(_back).next = std::move(element);
		}
		_back = borrow;
		h(borrow).in_list = true;
		return iterator{borrow};
	}

	iterator insert(iterator before, owner_pointer element) {
		if(!before._current) {
			return push_back(element);
		}else if(before._current == _front) {
			return push_front(element);
		}

		FRG_ASSERT(element);
		borrow_pointer borrow = traits::decay(element);
		FRG_ASSERT(!h(borrow).in_list);
		FRG_ASSERT(!h(borrow).next);
		FRG_ASSERT(!h(borrow).previous);
		borrow_pointer previous = h(before._current).previous;
		owner_pointer next = std::move(h(previous).next);

		h(previous).next = std::move(element);
		h(traits::decay(next)).previous = borrow;
		h(borrow).previous = previous;
		h(borrow).next = std::move(next);
		h(borrow).in_list = true;
		return iterator{borrow};
	}

	constexpr bool empty() const {
		return !_front;
	}

	constexpr borrow_pointer front() {
		return _front;
	}
	constexpr borrow_pointer back() {
		return _back;
	}

	owner_pointer pop_front() {
		FRG_ASSERT(h(_front).in_list);
		return erase(iterator{_front});
	}
	owner_pointer pop_back() {
		FRG_ASSERT(h(_back).in_list);
		return erase(iterator{_back});
	}

	owner_pointer erase(iterator it) {
		FRG_ASSERT(it._current);
		FRG_ASSERT(h(it._current).in_list);
		owner_pointer next = std::move(h(it._current).next);
		borrow_pointer previous = h(it._current).previous;

		if(!next) {
			FRG_ASSERT(_back == it._current);
			_back = previous;
		}else{
			FRG_ASSERT(h(traits::decay(next)).previous == it._current);
			h(traits::decay(next)).previous = previous;
		}

		owner_pointer erased;
		if(!previous) {
			FRG_ASSERT(traits::decay(_front) == it._current);
			erased = std::move(_front);
			_front = std::move(next);
		}else{
			FRG_ASSERT(traits::decay(h(previous).next) == it._current);
			erased = std::move(h(previous).next);
			h(previous).next = std::move(next);
		}

		FRG_ASSERT(traits::decay(erased) == it._current);
		h(it._current).next = nullptr;
		h(it._current).previous = nullptr;
		h(it._current).in_list = false;

		return erased;
	}

	void clear() {
		while(!empty())
			pop_front();
	}

	void splice(iterator it, intrusive_list &other) {
		FRG_ASSERT(!it._current);
		
		if(!other._front)
			return;

		borrow_pointer borrow = traits::decay(other._front);
		FRG_ASSERT(h(borrow).in_list);
		FRG_ASSERT(!h(borrow).previous);
		if(!_back) {
			_front = std::move(other._front);
		}else{
			h(borrow).previous = _back;
			h(_back).next = std::move(other._front);
		}
		_back = other._back;

		other._front = nullptr;
		other._back = nullptr;
	}

	iterator begin() {
		return iterator{traits::decay(_front)};
	}
	iterator end() {
		return iterator{nullptr};
	}

private:
	owner_pointer _front;
	borrow_pointer _back;
};

// TODO: As an alternative design, we could also use the existing list hook
//       together with std::atomic_ref (or atomic builtins).
//       Evaluate whether that makes sense or not.
template<typename T>
struct intrusive_rcu_list_hook {
	constexpr intrusive_rcu_list_hook()
	: next{nullptr}, previous{nullptr}, in_list{false} { }

	std::atomic<T *> next;
	std::atomic<T *> previous;
	bool in_list;
};

// RCU-compatible linked list.
// Supports lock-free traversal if the nodes are RCU-protected.
// Note that this list does not support multiple concurrent writers.
template<typename T, typename Locate>
struct intrusive_rcu_list {
private:
	using hook = intrusive_rcu_list_hook<T>;

public:
	// TODO: We could add a reverse_iterator but bi-directional iterators
	//       would be messy since the end of the list is represented by nullptr.
	struct iterator {
		iterator(T *current, Locate locate)
		: _current(current), locate_(locate) { }

		T *operator*() const { return _current; }

		bool operator==(const iterator &other) const {
			return _current == other._current;
		}
		bool operator!=(const iterator &other) const {
			return !(*this == other);
		}

		iterator &operator++() {
			_current = h(_current).next.load(std::memory_order_acquire);
			return *this;
		}
		iterator operator++(int) {
			auto copy = *this;
			++(*this);
			return copy;
		}

	private:
		hook &h(T *ptr) { return locate_(*ptr); }

		T *_current;
		FRG_NO_UNIQUE_ADDRESS Locate locate_;
	};

	constexpr intrusive_rcu_list(Locate locate = {})
	: _front{nullptr}, _back{nullptr}, locate_(locate)  { }

	iterator iterator_to(T *ptr) {
		FRG_ASSERT(h(ptr).in_list);
		return iterator{ptr, locate_};
	}

	void push_front(T *element) {
		FRG_ASSERT(element);
		FRG_ASSERT(!h(element).in_list);
		T *old_front = _front.load(std::memory_order_relaxed);
		// Prepare the new element.
		h(element).next.store(old_front, std::memory_order_relaxed);
		h(element).previous.store(nullptr, std::memory_order_relaxed);
		h(element).in_list = true;
		// Publish the new element with release ordering.
		if(!old_front) {
			_back.store(element, std::memory_order_release);
		} else {
			h(old_front).previous.store(element, std::memory_order_release);
		}
		_front.store(element, std::memory_order_release);
	}

	void push_back(T *element) {
		FRG_ASSERT(element);
		FRG_ASSERT(!h(element).in_list);
		T *old_back = _back.load(std::memory_order_relaxed);
		// Prepare the new element.
		h(element).next.store(nullptr, std::memory_order_relaxed);
		h(element).previous.store(old_back, std::memory_order_relaxed);
		h(element).in_list = true;
		// Publish the new element with release ordering.
		if(!old_back) {
			_front.store(element, std::memory_order_release);
		} else {
			h(old_back).next.store(element, std::memory_order_release);
		}
		_back.store(element, std::memory_order_release);
	}

	// Note that erase() does not clear element->previous and element->next,
	// such that iterators that are currently at element are not invalidated.
	void erase(T *element) {
		FRG_ASSERT(element);
		FRG_ASSERT(h(element).in_list);
		T *next = h(element).next.load(std::memory_order_relaxed);
		T *previous = h(element).previous.load(std::memory_order_relaxed);
		// Publish the updated pointers with release ordering.
		if(!next) {
			_back.store(previous, std::memory_order_release);
		} else {
			h(next).previous.store(previous, std::memory_order_release);
		}
		if(!previous) {
			_front.store(next, std::memory_order_release);
		} else {
			h(previous).next.store(next, std::memory_order_release);
		}
	}

	bool empty() const {
		return !_front.load(std::memory_order_acquire);
	}

	T *front() {
		return _front.load(std::memory_order_acquire);
	}

	T *back() {
		return _back.load(std::memory_order_acquire);
	}

	iterator begin() {
		return iterator{_front.load(std::memory_order_acquire), locate_};
	}
	iterator end() {
		return iterator{nullptr, locate_};
	}

private:
	hook &h(T *ptr) { return locate_(*ptr); }

	std::atomic<T *> _front;
	std::atomic<T *> _back;
	FRG_NO_UNIQUE_ADDRESS Locate locate_;

};

} // namespace _list

using _list::intrusive_list_hook;
using _list::intrusive_list;
using _list::intrusive_rcu_list_hook;
using _list::intrusive_rcu_list;

template<typename T>
using default_list_hook = intrusive_list_hook<
	std::add_pointer_t<T>,
	std::add_pointer_t<T>
>;

template<typename T, typename Allocator>
struct list {
private:
	struct item {
		template<typename... Args>
		item(Args &&... args)
		: object(std::forward<Args>(args)...) { }

		T object;
		frg::default_list_hook<item> hook;
	};

public:
	constexpr list(Allocator allocator = {})
	: allocator_{std::move(allocator)} { }

	template<typename... Args>
	void emplace_back(Args &&... args) {
		auto e = frg::construct<item>(allocator_, std::forward<Args>(args)...);
		items_.push_back(e);
	}

	constexpr bool empty() const {
		return items_.empty();
	}

	T &front() {
		return items_.front()->object;
	}

	void pop_front() {
		auto e = items_.pop_front();
		frg::destruct(allocator_, e);
	}

private:
	Allocator allocator_;

	frg::intrusive_list<
		item,
		frg::locate_member<
			item,
			frg::default_list_hook<item>,
			&item::hook
		>
	> items_;
};

} // namespace frg
