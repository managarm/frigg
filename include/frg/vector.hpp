#ifndef FRG_VECTOR_HPP
#define FRG_VECTOR_HPP

#include <concepts>
#include <iterator>
#include <utility>
#include <stddef.h>

#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

template<typename T, typename Allocator>
class vector {
public:
	using value_type = T;
	using reference = value_type&;

	template <typename Element>
	struct Iterator {
		using iterator_category = std::random_access_iterator_tag;
		using iterator_concept = std::contiguous_iterator_tag;
		using value_type = std::remove_cv_t<Element>;
		using element_type = Element;
		using difference_type = ptrdiff_t;
		using pointer = Element *;
		using reference = Element &;

		constexpr Iterator() noexcept = default;
		constexpr explicit Iterator(pointer ptr) noexcept : ptr_{ptr} {}

		template <typename OtherElement>
			requires std::convertible_to<OtherElement *, pointer>
		constexpr Iterator(const Iterator<OtherElement> &other) noexcept : ptr_(other.ptr_) {}

		constexpr reference operator*() const noexcept { return *ptr_; }
		constexpr pointer operator->() const noexcept { return ptr_; }

		constexpr Iterator &operator++() noexcept {
			++ptr_;
			return *this;
		}

		constexpr Iterator operator++(int) noexcept {
			Iterator tmp = *this;
			++ptr_;
			return tmp;
		}

		constexpr Iterator &operator--() noexcept {
			--ptr_;
			return *this;
		}

		constexpr Iterator operator--(int) noexcept {
			Iterator tmp = *this;
			--ptr_;
			return tmp;
		}

		constexpr Iterator &operator+=(difference_type offset) noexcept {
			ptr_ += offset;
			return *this;
		}

		constexpr Iterator operator+(difference_type offset) const noexcept {
			return Iterator(ptr_ + offset);
		}

		constexpr friend Iterator operator+(difference_type offset, const Iterator &it) noexcept {
			return Iterator(it.ptr_ + offset);
		}

		constexpr Iterator &operator-=(difference_type offset) noexcept {
			ptr_ -= offset;
			return *this;
		}

		constexpr Iterator operator-(difference_type offset) const noexcept {
			return Iterator(ptr_ - offset);
		}

		template <typename OtherElement>
			requires requires(pointer p1, OtherElement *p2) { p1 - p2; }
		constexpr difference_type operator-(const Iterator<OtherElement> &other) const noexcept {
			return ptr_ - other.ptr_;
		}

		constexpr reference operator[](difference_type offset) const noexcept {
			return ptr_[offset];
		}

		template <typename OtherElement>
			requires requires(pointer p1, OtherElement *p2) { p1 <=> p2; }
		constexpr auto operator<=>(const Iterator<OtherElement> &other) const noexcept {
			return ptr_ <=> other.ptr_;
		}

		template <typename OtherElement>
			requires requires(pointer p1, OtherElement *p2) { p1 == p2; }
		constexpr bool operator==(const Iterator<OtherElement> &other) const noexcept {
			return ptr_ == other.ptr_;
		}

		template <typename> friend struct Iterator;

	private:
		pointer ptr_ = nullptr;
	};

	using iterator = Iterator<T>;
	using const_iterator = Iterator<const T>;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	friend constexpr void swap(vector &a, vector &b) {
		using std::swap;
		swap(a._allocator, b._allocator);
		swap(a._elements, b._elements);
		swap(a._size, b._size);
		swap(a._capacity, b._capacity);
	}

	constexpr vector(Allocator allocator = Allocator());

	vector(const vector &other)
	: vector(other._allocator) {
		auto other_size = other.size();
		_ensure_capacity(other_size);
		for (size_t i = 0; i < other_size; i++)
			new (&_elements[i]) T(other[i]);
		_size = other_size;
	}

	constexpr vector(vector &&other)
	: vector(other._allocator) {
		swap(*this, other);
	}

	~vector();

	constexpr vector &operator= (vector other) {
		swap(*this, other);
		return *this;
	}

	T &push(const T &element);

	T &push(T &&element);

	T &push_back(const T &element) {
		return push(element);
	}

	T &push_back(T &&element) {
		return push(std::move(element));
	}

	template<typename... Args>
	T &emplace_back(Args &&... args);

	T pop();

	template<typename... Args>
	void resize(size_t new_size, Args &&... args);

	void clear() {
		for(size_t i = 0; i < _size; i++)
			_elements[i].~T();
		_size = 0;
	}

	constexpr void detach() {
		_size = 0;
		_capacity = 0;
		_elements = nullptr;
	}

	constexpr T *data() {
		return _elements;
	}

	constexpr const T *data() const {
		return _elements;
	}

	constexpr size_t size() const {
		return _size;
	}

	constexpr bool empty() const {
		return size() == 0;
	}

	constexpr iterator begin() {
		return iterator(_elements);
	}

	constexpr const_iterator begin() const {
		return const_iterator(_elements);
	}

	constexpr const_iterator cbegin() const {
		return const_iterator(_elements);
	}

	constexpr iterator end() {
		return iterator(_elements + _size);
	}

	constexpr const_iterator end() const {
		return const_iterator(_elements + _size);
	}

	constexpr const_iterator cend() const {
		return const_iterator(_elements + _size);
	}

	constexpr reverse_iterator rbegin() {
		return reverse_iterator(end());
	}

	constexpr const_reverse_iterator rbegin() const {
		return const_reverse_iterator(end());
	}

	constexpr const_reverse_iterator crbegin() const {
		return const_reverse_iterator(cend());
	}

	constexpr reverse_iterator rend() {
		return reverse_iterator(begin());
	}

	constexpr const_reverse_iterator rend() const {
		return const_reverse_iterator(begin());
	}

	constexpr const_reverse_iterator crend() const {
		return const_reverse_iterator(cbegin());
	}

	constexpr T &front() {
		return _elements[0];
	}
	constexpr const T &front() const {
		return _elements[0];
	}

	constexpr T &back() {
		return _elements[_size - 1];
	}
	constexpr const T &back() const {
		return _elements[_size - 1];
	}

	constexpr const T &operator[] (size_t index) const {
		return _elements[index];
	}
	constexpr T &operator[] (size_t index) {
		return _elements[index];
	}

	constexpr bool operator==(const vector &other) const {
		if (other.size() != size())
			return false;

		for (size_t i = 0; i < size(); i++)
			if (other[i] != _elements[i])
				return false;

		return true;
	}

	constexpr bool operator!=(const vector &other) const {
		return !(other == *this);
	}

private:
	void _ensure_capacity(size_t capacity);

	Allocator _allocator;
	T *_elements;
	size_t _size;
	size_t _capacity;
};

template<typename T, typename Allocator>
constexpr vector<T, Allocator>::vector(Allocator allocator)
: _allocator{std::move(allocator)}, _elements{nullptr}, _size{0}, _capacity{0} { }

template<typename T, typename Allocator>
vector<T, Allocator>::~vector() {
	for(size_t i = 0; i < _size; i++)
		_elements[i].~T();
	_allocator.free(_elements);
}

template<typename T, typename Allocator>
T &vector<T, Allocator>::push(const T &element) {
	_ensure_capacity(_size + 1);
	T *pointer = new (&_elements[_size]) T(element);
	_size++;
	return *pointer;
}

template<typename T, typename Allocator>
T &vector<T, Allocator>::push(T &&element) {
	_ensure_capacity(_size + 1);
	T *pointer = new (&_elements[_size]) T(std::move(element));
	_size++;
	return *pointer;
}

template<typename T, typename Allocator>
template<typename... Args>
T &vector<T, Allocator>::emplace_back(Args &&... args) {
	_ensure_capacity(_size + 1);
	T *pointer = new(&_elements[_size]) T(std::forward<Args>(args)...);
	_size++;
	return *pointer;
}

template<typename T, typename Allocator>
template<typename... Args>
void vector<T, Allocator>::resize(size_t new_size, Args &&... args) {
	_ensure_capacity(new_size);
	if(new_size < _size) {
		for(size_t i = new_size; i < _size; i++)
			_elements[i].~T();
	}else{
		for(size_t i = _size; i < new_size; i++)
			new (&_elements[i]) T(std::forward<Args>(args)...);
	}
	_size = new_size;
}

template<typename T, typename Allocator>
void vector<T, Allocator>::_ensure_capacity(size_t capacity) {
	if(capacity <= _capacity)
		return;

	size_t new_capacity = capacity * 2;
	T *new_array = (T *)_allocator.allocate(sizeof(T) * new_capacity);
	for(size_t i = 0; i < _size; i++)
		new (&new_array[i]) T(std::move(_elements[i]));

	for(size_t i = 0; i < _size; i++)
		_elements[i].~T();
	_allocator.free(_elements);

	_elements = new_array;
	_capacity = new_capacity;
}

template<typename T, typename Allocator>
T vector<T, Allocator>::pop() {
	_size--;
	T element = std::move(_elements[_size]);
	_elements[_size].~T();
	return element;
}

} // namespace frg

#endif // FRG_VECTOR_HPP
