#ifndef FRG_STACK_HPP
#define FRG_STACK_HPP

#include <frg/macros.hpp>
#include <frg/vector.hpp>

namespace frg FRG_VISIBILITY {

template<typename T, typename Allocator, typename Container =
	frg::vector<T, Allocator>>
class stack {
public:
	using value_type = typename Container::value_type;
	using reference = typename Container::reference;

	stack() { }
	stack(Allocator alloc) : _container(alloc) { }

	reference top() {
		return _container.back();
	}

	void pop() {
		_container.pop();
	}

	void push(const value_type &value) {
		_container.push_back(value);
	}

	size_t size() const {
		return _container.size();
	}

	bool empty() const {
		return _container.empty();
	}
private:
	Container _container;
};

} // namespace frg

#endif
