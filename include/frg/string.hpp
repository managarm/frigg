#ifndef FRG_STRING_HPP
#define FRG_STRING_HPP

#include <cstddef>
#include <frg/hash.hpp>
#include <frg/macros.hpp>
#include <frg/optional.hpp>
#include <frg/utility.hpp>
#include <frg/string_stub.hpp>

namespace frg FRG_VISIBILITY {
template<typename CharT>
auto generic_strlen(const CharT *c) {
	std::size_t len = 0;
	while (*(c++)) {
		len++;
	}
	return len;
}

template<typename Char>
class basic_string_view {
public:
	typedef Char CharType;
	using value_type = Char;

	basic_string_view()
	: _pointer{nullptr}, _length{0} { }

	basic_string_view(const Char *cs)
	: _pointer{cs}, _length{generic_strlen(cs)} { }

	basic_string_view(const Char *s, size_t length)
	: _pointer{s}, _length{length} { }

	const Char *data() const {
		return _pointer;
	}

	const Char &operator[] (size_t index) const {
		return _pointer[index];
	}

	size_t size() const {
		return _length;
	}

	bool operator== (basic_string_view other) const {
		if(_length != other._length)
			return false;
		for(size_t i = 0; i < _length; i++)
			if(_pointer[i] != other._pointer[i])
				return false;
		return true;
	}

	size_t find_first(Char c, size_t start_from = 0) const {
		for(size_t i = start_from; i < _length; i++)
			if(_pointer[i] == c)
				return i;

		return size_t(-1);
	}

	size_t find_first_of(basic_string_view chars, size_t start_from = 0) const {
		for(size_t i = start_from; i < _length; ++i) {
			for(size_t j = 0; j < chars.size(); ++j)
				if(_pointer[i] == chars[j])
					return i;
		}
		return size_t(-1);
	}

	size_t find_last(Char c) const {
		for(size_t i = _length; i > 0; i--)
			if(_pointer[i - 1] == c)
				return i - 1;

		return size_t(-1);
	}

	basic_string_view sub_string(size_t from, size_t size) const {
		FRG_ASSERT(from + size <= _length);
		return basic_string_view(_pointer + from, size);
	}

	bool starts_with(basic_string_view other) {
		if (other.size() > size()) {
			return false;
		}

		return sub_string(0, other.size()) == other;
	}

	bool ends_with(basic_string_view other) {
		if (other.size() > size()) {
			return false;
		}

		return sub_string(size() - other.size(), other.size()) == other;
	}

	template<typename T>
	optional<T> to_number() {
		T value = 0;
		for(size_t i = 0; i < _length; i++) {
			if(!(_pointer[i] >= '0' && _pointer[i] <= '9'))
				return null_opt;
			value = value * 10 + (_pointer[i] - '0');
		}
		return value;
	}

private:
	const Char *_pointer;
	size_t _length;
};

typedef basic_string_view<char> string_view;

template<typename Char, typename Allocator>
class basic_string {
public:
	typedef Char CharType;
	using value_type = Char;

	friend void swap(basic_string &a, basic_string &b) {
		using std::swap;
		swap(a._allocator, b._allocator);
		swap(a._buffer, b._buffer);
		swap(a._length, b._length);
	}

	basic_string(Allocator allocator = Allocator())
	: _allocator{std::move(allocator)}, _buffer{nullptr}, _length{0} { }

	basic_string(const Char *c_string, Allocator allocator = Allocator())
	: _allocator{std::move(allocator)} {
		_length = generic_strlen(c_string);
		_buffer = (Char *)_allocator.allocate(sizeof(Char) * _length + 1);
		memcpy(_buffer, c_string, sizeof(Char) * _length);
		_buffer[_length] = 0;
	}

	// Compatibility/transition constructor.
	basic_string(Allocator allocator, const Char *c_string)
	: basic_string{c_string, std::move(allocator)} { }

	basic_string(const Char *buffer, size_t size, Allocator allocator = Allocator())
	: _allocator{std::move(allocator)}, _length{size} {
		_buffer = (Char *)_allocator.allocate(sizeof(Char) * _length + 1);
		memcpy(_buffer, buffer, sizeof(Char) * _length);
		_buffer[_length] = 0;
	}

	// Compatibility/transition constructor.
	basic_string(Allocator allocator, const Char *buffer, size_t size)
	: basic_string{buffer, size, std::move(allocator)} { }

	explicit basic_string(const basic_string_view<Char> &view, Allocator allocator = Allocator())
	: _allocator{std::move(allocator)}, _length{view.size()} {
		_buffer = (Char *)_allocator.allocate(sizeof(Char) * _length + 1);
		memcpy(_buffer, view.data(), sizeof(Char) * _length + 1);
		_buffer[_length] = 0;
	}

	// Compatibility/transition constructor.
	explicit basic_string(Allocator allocator, const basic_string_view<Char> &view)
	: basic_string{view, std::move(allocator)} { }

	basic_string(size_t size, Char c = 0, Allocator allocator = Allocator())
	: _allocator{std::move(allocator)}, _length{size} {
		_buffer = (Char *)_allocator.allocate(sizeof(Char) * _length + 1);
		for(size_t i = 0; i < size; i++)
			_buffer[i] = c;
		_buffer[_length] = 0;
	}

	basic_string(const basic_string &other)
	: _allocator{other._allocator}, _length{other._length} {
		_buffer = (Char *)_allocator.allocate(sizeof(Char) * _length + 1);
		memcpy(_buffer, other._buffer, sizeof(Char) * _length);
		_buffer[_length] = 0;
	}

	~basic_string() {
		if(_buffer)
			_allocator.free(_buffer);
	}

	basic_string &operator= (basic_string other) {
		swap(*this, other);
		return *this;
	}

	void resize(size_t new_length) {
		size_t copy_length = _length;
		if(copy_length > new_length)
			copy_length = new_length;

		Char *new_buffer = (Char *)_allocator.allocate(sizeof(Char) * new_length + 1);
		memcpy(new_buffer, _buffer, sizeof(Char) * copy_length);
		new_buffer[new_length] = 0;

		if(_buffer)
			_allocator.free(_buffer);
		_length = new_length;
		_buffer = new_buffer;
	}

	// TODO: Inefficient. Does two copies (one here, one in constructor).
	// TODO: Better: Return expression template?
	basic_string operator+ (const basic_string_view<Char> &other) {
		size_t new_length = _length + other.size();
		Char *new_buffer = (Char *)_allocator.allocate(sizeof(Char) * new_length + 1);
		memcpy(new_buffer, _buffer, sizeof(Char) * _length);
		memcpy(new_buffer + _length, other.data(), sizeof(Char) * other.size());
		new_buffer[new_length] = 0;

		return basic_string(_allocator, new_buffer, new_length);
	}

	// TODO: Inefficient. Does two copies (one here, one in constructor).
	// TODO: Better: Return expression template?
	basic_string operator+ (Char c) {
		size_t new_length = _length + 1;
		Char *new_buffer = (Char *)_allocator.allocate(sizeof(Char) * new_length + 1);
		memcpy(new_buffer, _buffer, sizeof(Char) * _length);
		new_buffer[_length] = c;
		new_buffer[new_length] = 0;

		return basic_string(_allocator, new_buffer, new_length);
	}

	void push_back(Char c) {
		operator+=(c);
	}

	basic_string &operator+= (const basic_string_view<Char> &other) {
		size_t new_length = _length + other.size();
		Char *new_buffer = (Char *)_allocator.allocate(sizeof(Char) * new_length + 1);
		memcpy(new_buffer, _buffer, sizeof(Char) * _length);
		memcpy(new_buffer + _length, other.data(), sizeof(Char) * other.size());
		new_buffer[new_length] = 0;

		if(_buffer)
			_allocator.free(_buffer);
		_length = new_length;
		_buffer = new_buffer;

		return *this;
	}

	basic_string &operator+= (Char c) {
		/* TODO: SUPER INEFFICIENT should be done with a _capacity variable */
		Char *new_buffer = (Char *)_allocator.allocate(sizeof(Char) * _length + 2);
		memcpy(new_buffer, _buffer, sizeof(Char) * _length);
		new_buffer[_length] = c;
		new_buffer[_length + 1] = 0;

		if (_buffer)
			_allocator.free(_buffer);
		_length++;
		_buffer = new_buffer;

		return *this;
	}

	// used to disable deallocation upon object destruction
	void detach() {
		_buffer = nullptr;
		_length = 0;
	}

	Char *data() {
		return _buffer;
	}
	const Char *data() const {
		return _buffer;
	}

	Char &operator[] (size_t index) {
		return _buffer[index];
	}
	const Char &operator[] (size_t index) const {
		return _buffer[index];
	}

	size_t size() const {
		return _length;
	}

	bool empty() const {
		return _length == 0;
	}

	Char *begin() {
		return _buffer;
	}
	const Char *begin() const {
		return _buffer;
	}

	Char *end() {
		return _buffer + _length;
	}
	const Char *end() const {
		return _buffer + _length;
	}

	int compare(const basic_string &other) const {
		if(_length != other.size())
			return _length < other.size() ? -1 : 1;
		for(size_t i = 0; i < _length; i++)
			if(_buffer[i] != other[i])
				return _buffer[i] < other[i] ? -1 : 1;
		return 0;
	}

	int compare(const char *other) const {
		auto other_len = generic_strlen(other);
		if(_length != other_len)
			return _length < other_len ? -1 : 1;
		for(size_t i = 0; i < _length; i++)
			if(_buffer[i] != other[i])
				return _buffer[i] < other[i] ? -1 : 1;
		return 0;
	}

	bool operator== (const basic_string &other) const {
		return compare(other) == 0;
	}

	bool operator== (const char *rhs) const {
		return compare(rhs) == 0;
	}

	operator basic_string_view<Char> () const {
		return basic_string_view<Char>(_buffer, _length);
	}

	bool starts_with(basic_string_view<Char> other) {
		auto self = basic_string_view<Char> { *this };
		return self.starts_with(other);
	}

	bool ends_with(basic_string_view<Char> other) {
		auto self = basic_string_view<Char> { *this };
		return self.ends_with(other);
	}

private:
	Allocator _allocator;
	Char *_buffer;
	size_t _length;
};

template<typename Allocator>
using string = basic_string<char, Allocator>;

template<typename Char>
class hash<basic_string_view<Char>> {
public:
	unsigned int operator() (const basic_string_view<Char> &string) const {
		unsigned int hash = 0;
		for(size_t i = 0; i < string.size(); i++)
			hash += 31 * hash + string[i];
		return hash;
	}
};

template<typename Char, typename Allocator>
class hash<basic_string<Char, Allocator>> {
public:
	unsigned int operator() (const basic_string<Char, Allocator> &string) const {
		unsigned int hash = 0;
		for(size_t i = 0; i < string.size(); i++)
			hash += 31 * hash + string[i];
		return hash;
	}
};

namespace _to_string_impl {
	template<typename T>
	constexpr size_t num_digits(T v, int radix) {
		size_t n = 0;
		while(v) {
			v /= radix;
			n++;
		}
		return n;
	}

	template<typename T>
	constexpr size_t num_digits(int) {
		return 33;
		//TODO: This is actually something like: max(num_digits(std::numeric_limits<T>::max(), radix),
		//		num_digits(std::numeric_limits<T>::min(), radix) + 1);
	}

	template<typename T>
	constexpr size_t num_digits() {
		return num_digits<T>(2);
	}

	constexpr auto small_digits = "0123456789abcdef";

	template<typename T, typename Pool>
	string<Pool> to_allocated_string(Pool &pool, T v, int radix = 10, size_t precision = 1,
			const char *digits = small_digits) {
		constexpr auto m = num_digits<T>();
		FRG_ASSERT(v >= 0);

		char buffer[m];
		size_t n = 0;
		while(v) {
			FRG_ASSERT(n < m);
			buffer[n++] = digits[v % radix];
			v /= radix;
		}

		string<Pool> result(pool);
		auto len = max(precision, n);
		result.resize(max(precision, n));

		for(size_t i = 0; i < len - n; i++)
			result[i] = '0';
		for(size_t i = 0; i < n; i++)
			result[len - n + i] = buffer[n - (i + 1)];
		return result;
	}
}

using _to_string_impl::to_allocated_string;

} // namespace frg

#endif // FRG_STRING_HPP
