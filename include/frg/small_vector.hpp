#pragma once

#include <utility>
#include <stdint.h>

#include <frg/array.hpp>
#include <frg/eternal.hpp>
#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

template<typename T, size_t N>
class static_vector {
public:
	using value_type = T;
	using reference = value_type&;
	using iterator = value_type*;
	using const_iterator = const value_type*;

	static_vector()
	: _size(0) { }

	static_vector(const static_vector &other)
	: static_vector() {
		auto other_size = other.size();
		auto container = _get_container();
		for (size_t i = 0; i < other_size; i++)
			new (&container[i]) T(other[i]);
		_size = other_size;
	}

	static_vector(static_vector &&other)
	: static_vector() {
		auto other_size = other.size();
		auto container = _get_container();
		for (size_t i = 0; i < other_size; i++)
			new (&container[i]) T(std::move(other[i]));
		_size = other_size;
	}

	~static_vector() {
		auto container = _get_container();
		for (size_t i = 0; i < _size; i++)
			container[i].~T();
	}

	static_vector operator= (static_vector other) {
		auto other_size = other.size();
		auto container = _get_container();
		for (size_t i = 0; i < _size; i++)
			container[i].~T();
		for (size_t i = 0; i < other_size; i++)
			new (&container[i]) T(std::move(other[i]));
		_size = other_size;
		return *this;
	}

	size_t size() const {
		return _size;
	}

	bool empty() const {
		return _size == 0;
	}

	value_type &push_back(const T &element) {
		FRG_ASSERT(_size < N);
		auto container = _get_container();
		T *pointer = new (&container[_size]) T(element);
		_size++;
		return *pointer;
	}
	value_type &push_back(T &&element) {
		FRG_ASSERT(_size < N);
		auto container = _get_container();
		T *pointer = new (&container[_size]) T(std::move(element));
		_size++;
		return *pointer;
	}

	template<typename... Args>
	value_type &emplace_back(Args&&... args) {
		FRG_ASSERT(_size < N);
		auto container = _get_container();
		T *pointer = new (&container[_size]) T(std::forward<Args>(args)...);
		_size++;
		return *pointer;
	}

	void pop_back() {
		FRG_ASSERT(_size);
		auto container = _get_container();
		container[_size - 1].~T();
		--_size;
	}

	template<typename... Args>
	void resize(size_t new_size, Args&&... args) {
		FRG_ASSERT(new_size <= N);
		auto container = _get_container();
		if (new_size < _size) {
			for (size_t i = new_size; i < _size; i++)
				container[i].~T();
		} else {
			for (size_t i = _size; i < new_size; i++)
				new (&container[i]) T(std::forward<Args>(args)...);
		}
		_size = new_size;
	}

	T *data() {
		return _get_container();
	}
	const T *data() const {
		return _get_container();
	}

	iterator begin() {
		return _get_container();
	}
	const_iterator begin() const {
		return _get_container();
	}

	iterator end() {
		return _get_container() + _size;
	}
	const_iterator end() const {
		return _get_container() + _size;
	}

	value_type &operator[] (size_t index) {
		FRG_ASSERT(index < _size);
		auto container = _get_container();
		return container[index];
	}
	const value_type &operator[] (size_t index) const {
		FRG_ASSERT(index < _size);
		auto container = _get_container();
		return container[index];
	}

	value_type &front() {
		FRG_ASSERT(_size);
		return _get_container()[0];
	}
	const value_type &front() const {
		FRG_ASSERT(_size);
		return _get_container()[0];
	}

	value_type &back() {
		FRG_ASSERT(_size);
		return _get_container()[_size - 1];
	}
	const value_type &back() const {
		FRG_ASSERT(_size);
		return _get_container()[_size - 1];
	}

private:
	value_type *_get_container() {
		return reinterpret_cast<value_type*>(&_array[0].buffer);
	}
	const value_type *_get_container() const {
		return reinterpret_cast<const value_type*>(&_array[0].buffer);
	}

	array<aligned_storage<sizeof(T), alignof(T)>, N> _array;
	size_t _size;
};

template<typename T, size_t N, typename Allocator>
class small_vector {
public:
	using value_type = T;
	using reference = value_type&;
	using iterator = value_type*;
	using const_iterator = const value_type*;

	friend void swap(small_vector &a, small_vector &b) {
		using std::swap;
		swap(a._allocator, b._allocator);
		swap(a._array, b._array);
		swap(a._elements, b._elements);
		swap(a._size, b._size);
		swap(a._capacity, b._capacity);
	}

	small_vector(Allocator allocator = Allocator())
	: _allocator(allocator), _elements(nullptr),
		_size(0), _capacity(N)
	{ }

	small_vector(const small_vector &other)
	: small_vector(other._allocator) {
		auto other_size = other.size();
		_ensure_capacity(other_size);
		auto container = _get_container();
		for (size_t i = 0; i < other_size; i++)
			new (&container[i]) T(other[i]);
		_size = other_size;
	}

	small_vector(small_vector &&other)
	: small_vector(other._allocator) {
		swap(*this, other);
	}

	~small_vector() {
		auto container = _get_container();
		for (size_t i = 0; i < _size; i++)
			container[i].~T();
		if(!_is_small())
			_allocator.deallocate(container, sizeof(T) * _capacity);
	}

	size_t size() const {
		return _size;
	}

	bool empty() const {
		return _size == 0;
	}

	value_type &push_back(const T &element) {
		_ensure_capacity(_size + 1);
		auto container = _get_container();
		T *pointer = new (&container[_size]) T(element);
		_size++;
		return *pointer;
	}
	value_type &push_back(T &&element) {
		_ensure_capacity(_size + 1);
		auto container = _get_container();
		T *pointer = new (&container[_size]) T(std::move(element));
		_size++;
		return *pointer;
	}

	template<typename... Args>
	value_type &emplace_back(Args&&... args) {
		_ensure_capacity(_size + 1);
		auto container = _get_container();
		T *pointer = new (&container[_size]) T(std::forward<Args>(args)...);
		_size++;
		return *pointer;
	}

	void pop_back() {
		FRG_ASSERT(_size);
		auto container = _get_container();
		container[_size - 1].~T();
		--_size;
	}

	template<typename... Args>
	void resize(size_t new_size, Args&&... args) {
		_ensure_capacity(new_size);
		auto container = _get_container();
		if (new_size < _size) {
			for (size_t i = new_size; i < _size; i++)
				container[i].~T();
		} else {
			for (size_t i = _size; i < new_size; i++)
				new (&container[i]) T(std::forward<Args>(args)...);
		}
		_size = new_size;
	}

	T *data() {
		return _get_container();
	}
	const T *data() const {
		return _get_container();
	}

	iterator begin() {
		return _get_container();
	}
	const_iterator begin() const {
		return _get_container();
	}

	iterator end() {
		return _get_container() + _size;
	}
	const_iterator end() const {
		return _get_container() + _size;
	}

	value_type &operator[] (size_t index) {
		auto container = _get_container();
		return container[index];
	}
	const value_type &operator[] (size_t index) const {
		auto container = _get_container();
		return container[index];
	}

	value_type &front() {
		FRG_ASSERT(_size);
		return _get_container()[0];
	}
	const value_type &front() const {
		FRG_ASSERT(_size);
		return _get_container()[0];
	}

	value_type &back() {
		FRG_ASSERT(_size);
		return _get_container()[_size - 1];
	}
	const value_type &back() const {
		FRG_ASSERT(_size);
		return _get_container()[_size - 1];
	}

private:
	bool _is_small() const {
		return _capacity <= N;
	}

	void _ensure_capacity(size_t capacity) {
		if (capacity <= _capacity)
			return;

		auto container = _get_container();		
		size_t new_capacity = capacity * 2;
		T *new_array = (T *)_allocator.allocate(sizeof(T) * new_capacity);
		for(size_t i = 0; i < _capacity; i++)
			new (&new_array[i]) T(std::move(container[i]));

		for(size_t i = 0; i < _size; i++)
			container[i].~T();
		// if the container is the array then _elements is a nullptr
		// and this is an no-op
		_allocator.free(_elements);
		
		_elements = new_array;
		_capacity = new_capacity;
	}

	value_type *_get_container() {
		if (_is_small())
			return reinterpret_cast<value_type*>(&_array[0].buffer);
		else
			return _elements;
	}
	const value_type *_get_container() const {
		if (_is_small())
			return reinterpret_cast<const value_type*>(&_array[0].buffer);
		else
			return _elements;
	}

	Allocator _allocator;
	array<aligned_storage<sizeof(T), alignof(T)>, N> _array;
	T *_elements;
	size_t _size;
	size_t _capacity;
};

} // namespace frg
