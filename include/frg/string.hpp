#ifndef FRG_STRING_HPP
#define FRG_STRING_HPP

#include <string.h>

#include <frg/hash.hpp>
#include <frg/macros.hpp>
#include <frg/optional.hpp>

namespace frg FRG_VISIBILITY {

template<typename Char>
class basic_string_view {
public:
	typedef Char CharType;

	basic_string_view()
	: _pointer{nullptr}, _length{0} { }

	basic_string_view(const Char *cs)
	: _pointer{cs}, _length{0} {
		// We cannot call strlen() as Char might not be the usual char.
		while(cs[_length])
			_length++;
	}

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
	bool operator!= (basic_string_view other) const {
		return !(*this == other);
	}

	size_t find_first(Char c, size_t start_from = 0) {
		for(size_t i = start_from; i < _length; i++)
			if(_pointer[i] == c)
				return i;

		return size_t(-1);
	}

	size_t find_last(Char c) {
		for(size_t i = _length; i > 0; i--)
			if(_pointer[i - 1] == c)
				return i - 1;
		
		return size_t(-1);
	}

	basic_string_view sub_string(size_t from, size_t size) {
		FRG_ASSERT(from + size <= _length);
		return basic_string_view(_pointer + from, size);
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

	friend void swap(basic_string &a, basic_string &b) {
		using std::swap;
		swap(a._allocator, b._allocator);
		swap(a._buffer, b._buffer);
		swap(a._length, b._length);
	}

	basic_string(Allocator &allocator)
	: _allocator{&allocator}, _buffer{nullptr}, _length{0} { }

	basic_string(Allocator &allocator, const Char *c_string)
	: _allocator{&allocator} {
		_length = strlen(c_string);
		_buffer = (Char *)_allocator->allocate(sizeof(Char) * _length);
		memcpy(_buffer, c_string, sizeof(Char) * _length);
	}

	basic_string(Allocator &allocator, const Char *buffer, size_t size)
	: _allocator{&allocator}, _length{size} {
		_buffer = (Char *)_allocator->allocate(sizeof(Char) * _length);
		memcpy(_buffer, buffer, sizeof(Char) * _length);
	}

	basic_string(Allocator &allocator, const basic_string_view<Char> &view)
	: _allocator{&allocator}, _length{view.size()} {
		_buffer = (Char *)_allocator->allocate(sizeof(Char) * _length);
		memcpy(_buffer, view.data(), sizeof(Char) * _length);
	}
	
	basic_string(Allocator &allocator, size_t size, Char c = 0)
	: _allocator{&allocator}, _length{size} {
		_buffer = (Char *)_allocator->allocate(sizeof(Char) * _length);
		for(size_t i = 0; i < size; i++)
			_buffer[i] = c;
	}

	basic_string(const basic_string &other)
	: _allocator{other._allocator}, _length{other._length} {
		_buffer = (Char *)_allocator->allocate(sizeof(Char) * _length);
		memcpy(_buffer, other._buffer, sizeof(Char) * _length);
	}

	void resize(size_t new_length) {
		size_t copy_length = _length;
		if(copy_length > new_length)
			copy_length = new_length;

		Char *new_buffer = (Char *)_allocator->allocate(sizeof(Char) * new_length);
		memcpy(new_buffer, _buffer, sizeof(Char) * copy_length);
		
		if(_buffer != nullptr)
			_allocator->free(_buffer);
		_length = new_length;
		_buffer = new_buffer;
	}

	// TODO: Inefficient. Does two copies (one here, one in constructor).
	// TODO: Better: Return expression template?
	basic_string operator+ (const basic_string_view<Char> &other) {
		size_t new_length = _length + other.size();
		Char *new_buffer = (Char *)_allocator->allocate(sizeof(Char) * new_length);
		memcpy(new_buffer, _buffer, sizeof(Char) * _length);
		memcpy(new_buffer + _length, other.data(), sizeof(Char) * other.size());
		
		return basic_string(*_allocator, new_buffer, new_length);
	}

	// TODO: Inefficient. Does two copies (one here, one in constructor).
	// TODO: Better: Return expression template?
	basic_string operator+ (Char c) {
		size_t new_length = _length + 1;
		Char *new_buffer = (Char *)_allocator->allocate(sizeof(Char) * new_length);
		memcpy(new_buffer, _buffer, sizeof(Char) * _length);
		new_buffer[_length] = c;
		
		return basic_string(*_allocator, new_buffer, new_length);
	}

	basic_string &operator+= (const basic_string_view<Char> &other) {
		size_t new_length = _length + other.size();
		Char *new_buffer = (Char *)_allocator->allocate(sizeof(Char) * new_length);
		memcpy(new_buffer, _buffer, sizeof(Char) * _length);
		memcpy(new_buffer + _length, other.data(), sizeof(Char) * other.size());
		
		if(_buffer != nullptr)
			_allocator->free(_buffer);
		_length = new_length;
		_buffer = new_buffer;

		return *this;
	}

	basic_string &operator= (basic_string other) {
		swap(*this, other);
		return *this;
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

	bool operator== (const basic_string_view<Char> &other) const {
		if(_length != other.size())
			return false;
		for(size_t i = 0; i < _length; i++)
			if(_buffer[i] != other[i])
				return false;
		return true;
	}

	bool operator!= (const basic_string_view<Char> &other) const {
		return !(*this == other);
	}

	operator basic_string_view<Char> () const {
		return basic_string_view<Char>(_buffer, _length);
	}

private:
	Allocator *_allocator;
	Char *_buffer;
	size_t _length;
};

template<typename Allocator>
using string = basic_string<char, Allocator>;

template<typename Char>
struct hash<basic_string_view<Char>> {
	unsigned int operator() (const basic_string_view<Char> &string) {
		unsigned int hash = 0;
		for(size_t i = 0; i < string.size(); i++)
			hash += 31 * hash + string[i];
		return hash;
	}
};

template<typename Char, typename Allocator>
struct hash<basic_string<Char, Allocator>> {
	unsigned int operator() (const basic_string<Char, Allocator> &string) {
		unsigned int hash = 0;
		for(size_t i = 0; i < string.size(); i++)
			hash += 31 * hash + string[i];
		return hash;
	}
};

} // namespace frg

#endif // FRG_STRING_HPP
