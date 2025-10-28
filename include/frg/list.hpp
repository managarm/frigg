#pragma once

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
	private:
		hook &h(borrow_pointer ptr) {
			return get<locate_tag>(this)(*ptr);
		}

	public:
		iterator(borrow_pointer current)
		: _current(current) { }

		borrow_pointer operator* () const {
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
		} else if(before._current == _front) {
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

	bool empty() {
		return !_front;
	}

	borrow_pointer front() {
		return _front;
	}
	borrow_pointer back() {
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
		} else {
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
		} else {
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

} // namespace _list

using _list::intrusive_list_hook;
using _list::intrusive_list;

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
	list(Allocator allocator = {})
	: allocator_{std::move(allocator)} { }

	template<typename... Args>
	void emplace_back(Args &&... args) {
		auto e = frg::construct<item>(allocator_, std::forward<Args>(args)...);
		items_.push_back(e);
	}

	bool empty() {
		return items_.empty();
	}

	T &front() {
		return items_.front()->object;
	}

	T &back() {
		return items_.back()->object;
	}

	auto push_back(T elem) {
		auto e = frg::construct<item>(allocator_, elem);
		return items_.push_back(e);
	}

	auto push_front(T elem) {
		auto e = frg::construct<item>(allocator_, elem);
		return items_.push_front(e);
	}

	void pop_front() {
		auto e = items_.pop_front();
		frg::destruct(allocator_, e);
	}

	void pop_back() {
		auto e = items_.pop_back();
		frg::destruct(allocator_, e);
	}

	void clear() {
		while(!empty())
			pop_front();
	}

	auto erase(auto it) {
		return items_.erase(it);
	}

	void insert(auto before, T elem)
	{
		auto e = frg::construct<item>(allocator_, elem);
		items_.insert(before, e);
	}

	void splice(auto it, list &other) {
		items_.splice(it, other.items_);
	}

	auto begin() {
		return items_.begin(); 
	}

	auto end() {
		return items_.end(); 
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
