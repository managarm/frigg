#ifndef FRG_LIST_HPP
#define FRG_LIST_HPP

#include <type_traits>
#include <utility>
#include <frg/intrusive.hpp>
#include <frg/macros.hpp>
#include <frg/utility.hpp>

namespace frg FRG_VISIBILITY {

namespace _list {

template<typename OwnerPointer, typename BorrowPointer>
struct intrusive_list_hook {
	using owner_pointer = OwnerPointer;
	using borrow_pointer = BorrowPointer;

	intrusive_list_hook()
	: next{nullptr}, previous{nullptr} { }

	owner_pointer next;
	borrow_pointer previous;
};

struct locate_tag { };

template<typename T, typename Locate>
struct intrusive_list : private composition<locate_tag, Locate> {
private:
	using hook = std::remove_reference_t<std::result_of_t<Locate(T &)>>;
	using owner_pointer = typename hook::owner_pointer;
	using borrow_pointer = typename hook::borrow_pointer;
	using traits = intrusive_traits<T, owner_pointer, borrow_pointer>;

	hook &h(borrow_pointer ptr) {
		return get<locate_tag>(this)(*ptr);
	}

public:
	class iterator : private composition<locate_tag, Locate> {
	friend class intrusive_list;
	private:
		hook &h(borrow_pointer ptr) {
			return get<locate_tag>(this)(*ptr);
		}

	public:
		iterator(borrow_pointer current)
		: _current(current) { }

		borrow_pointer operator* () {
			return _current;
		}

		bool operator== (const iterator &other) {
			return _current == other._current;
		}
		bool operator!= (const iterator &other) {
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
		return iterator{ptr};
	}

	intrusive_list()
	: _front{nullptr}, _back{nullptr} { }
	

	void push_front(owner_pointer element) {
		borrow_pointer borrow = traits::decay(element);
		if(!_front) {
			_back = borrow;
		}else{
			h(borrow).next = std::move(_front);
			h(_front).previous = borrow;
		}
		_front = std::move(element);
	}
	
	void push_back(owner_pointer element) {
		borrow_pointer borrow = traits::decay(element);
		if(!_back) {
			_front = std::move(element);
		}else{
			h(borrow).previous = _back;
			h(_back).next = std::move(element);
		}
		_back = borrow;
	}

	bool empty() {
		return !_front;
	}

	borrow_pointer front() {
		return _front;
	}

	owner_pointer pop_front() {
		return erase(iterator_to(_front));
	}
	owner_pointer pop_back() {
		return erase(iterator_to(_back));
	}

	owner_pointer erase(iterator it) {
		owner_pointer next = std::move(h(it._current).next);
		borrow_pointer previous = h(it._current).previous;

		if(!next) {
			_back = previous;
		}else{
			h(traits::decay(next)).previous = previous;
		}

		owner_pointer erased;
		if(!previous) {
			erased = std::move(_front);
			_front = std::move(next);
		}else{
			erased = std::move(h(previous).next);
			h(previous).next = std::move(next);
		}

		h(it._current).next = nullptr;
		h(it._current).previous = nullptr;

		FRG_ASSERT(traits::decay(erased) == it._current);
		return erased;
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

} // namespace frg

#endif // FRG_LIST_HPP
