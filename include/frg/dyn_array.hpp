#pragma once

#include <utility>
#include <stddef.h>

#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

template<typename T, typename Allocator>
class dyn_array {
public:
	using value_type = T;
	using reference = value_type &;

	friend void swap(dyn_array &a, dyn_array &b) {
		using std::swap;
		swap(a.allocator_, b.allocator_);
		swap(a.elements_, b.elements_);
		swap(a.size_, b.size_);
	}

	dyn_array() = default;

	dyn_array(Allocator allocator)
	: allocator_{std::move(allocator)} { }

	dyn_array(size_t size, Allocator allocator = Allocator{})
	: allocator_{std::move(allocator)}, size_{size} {
		elements_ = reinterpret_cast<T *>(allocator_.allocate(sizeof(T) * size_));
		for (size_t i = 0; i < size_; i++)
			new (&elements_[i]) T{};
	}

	dyn_array(const dyn_array &other)
	: allocator_{other.allocator_}, size_{other.size_} {
		elements_ = reinterpret_cast<T *>(allocator_.allocate(sizeof(T) * size_));
		for (size_t i = 0; i < size_; i++)
			new (&elements_[i]) T{other[i]};
	}

	dyn_array(dyn_array &&other)
	: dyn_array(other.allocator_) {
		swap(*this, other);
	}

	~dyn_array() {
		for(size_t i = 0; i < size_; i++)
			elements_[i].~T();
		allocator_.deallocate(elements_, sizeof(T) * size_);
	}


	dyn_array &operator= (dyn_array other) {
		swap(*this, other);
		return *this;
	}

	T *data() {
		return elements_;
	}

	const T *data() const {
		return elements_;
	}

	size_t size() const {
		return size_;
	}

	bool empty() const {
		return size_;
	}

	T *begin() {
		return elements_;
	}

	const T *begin() const {
		return elements_;
	}

	T *end() {
		return elements_ + size_;
	}

	const T *end() const {
		return elements_ + size_;
	}

	const T &operator[] (size_t index) const {
		return elements_[index];
	}

	T &operator[] (size_t index) {
		return elements_[index];
	}

private:
	Allocator allocator_{};
	T *elements_ = nullptr;
	size_t size_ = 0;
};

} // namespace frg
