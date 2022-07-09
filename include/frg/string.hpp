#ifndef FRG_STRING_HPP
#define FRG_STRING_HPP

#include <string.h>
#include <compare>
#include <memory>

#include <frg/hash.hpp>
#include <frg/macros.hpp>
#include <frg/optional.hpp>
#include <frg/utility.hpp>

namespace frg FRG_VISIBILITY {

template<typename Char>
class char_traits
{
	public:
	using char_type = Char;
	using int_type = int;

	static constexpr void assign(char_type &r, const char_type &a) { r = a; }
	static constexpr char_type *assign(char_type *p, size_t count, char_type a)
	{
		auto ret = p;
		while (count-- != 0) assign(*p++, a);
		return ret;
	}

	static constexpr bool eq(char_type a, char_type b) { return a == b; }
	static constexpr bool lt(char_type a, char_type b) { return a < b; }

	static constexpr char_type *move(char_type *dest, const char_type *src, size_t count)
	{
		auto ret = dest;
		if (dest < src) while (count-- != 0) assign(*dest++, *src++);
		else if (src < dest)
		{
			dest += count;
			src += count;
			while (count-- != 0) assign(*--dest, *--src);
		}
		return ret;
	}

	static constexpr char_type *copy(char_type *dest, const char_type *src, size_t count)
	{
		FRG_ASSERT(src < dest || src >= dest + count);
		auto ret = dest;
		while (count-- != 0) assign(*dest++, *src++);
		return ret;
	}

	static constexpr int compare(const char_type *s1, const char_type *s2, size_t count)
	{
		while (count-- != 0)
		{
			if (lt(*s1, *s2)) return -1;
			if (lt(*s2++, *s1++)) return 1;
		}
		return 0;
	}

	static constexpr size_t length(const char_type *s)
	{
		size_t len = 0;
		while (eq(*s++, char_type(0)) == false) len++;
		return len;
	}

	static constexpr const char_type *find(const char_type *p, size_t count, const char_type &ch)
	{
		while (count-- != 0)
		{
			if (eq(*p++, ch) == true) return p - 1;
		}
		return nullptr;
	}

	static constexpr char_type to_char_type(int_type c) { return char_type(c); }
	static constexpr int_type to_int_type(char_type c) { return int_type(c); }
	static constexpr bool eq_int_type(int_type c1, int_type c2) { return c1 == c2; }
	static constexpr int_type eof() { return int_type(-1); }
	static constexpr int_type not_eof(int_type e) { return eq_int_type(e, eof()) ? ~eof() : e; }
};

template<>
class char_traits<char>
{
	public:
	using char_type = char;
	using int_type = int;

	static constexpr void assign(char_type &r, const char_type &a) { r = a; }
	static constexpr char_type *assign(char_type *p, size_t count, char_type a)
	{
		auto ret = p;
		while (count-- != 0) assign(*p++, a);
		return ret;
	}

	static constexpr bool eq(char_type a, char_type b) { return a == b; }
	static constexpr bool lt(char_type a, char_type b) { return static_cast<unsigned char>(a) < static_cast<unsigned char>(b); }

	static constexpr char_type *move(char_type *dest, const char_type *src, size_t count)
	{
		if (count == 0) return nullptr;
		__builtin_memmove(dest, src, count * sizeof(char_type));
		return dest;
	}

	static constexpr char_type *copy(char_type *dest, const char_type *src, size_t count)
	{
		if (count == 0) return nullptr;
		FRG_ASSERT(src < dest || src >= dest + count);
		__builtin_memcpy(dest, src, count);
		return dest;
	}

	static constexpr int compare(const char_type *s1, const char_type *s2, size_t count)
	{
		if (count == 0) return 0;
		return __builtin_memcmp(s1, s2, count);
	}

	static constexpr size_t length(const char_type *s)
	{
		return __builtin_strlen(s);
	}

	static constexpr const char_type *find(const char_type *p, size_t count, const char_type &ch)
	{
		if (count == 0) return nullptr;
		return __builtin_char_memchr(p, to_int_type(ch), count);
	}

	static constexpr char_type to_char_type(int_type c) { return char_type(c); }
	static constexpr int_type to_int_type(char_type c) { return int_type(c); }
	static constexpr bool eq_int_type(int_type c1, int_type c2) { return c1 == c2; }
	static constexpr int_type eof() { return int_type(-1); }
	static constexpr int_type not_eof(int_type e) { return eq_int_type(e, eof()) ? ~eof() : e; }
};

template<typename Char, typename Traits = char_traits<Char>>
class basic_string_view
{
	private:
	const Char *_pointer;
	size_t _length;

	public:
	using traits_type = Traits;
	using value_type = Char;
	using pointer = Char*;
	using const_pointer = const Char*;
	using reference = Char&;
	using const_reference = const Char&;
	using const_iterator = const_pointer;
	using iterator = const_iterator;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

	static constexpr size_type npos = size_type(-1);

	constexpr basic_string_view() : _pointer(nullptr), _length(0) { }
	constexpr basic_string_view(const basic_string_view &other) = default;

	constexpr basic_string_view(const Char *s, size_type length) : _pointer(s), _length(length) { }
	constexpr basic_string_view(const Char *s) : _pointer(s), _length(Traits::length(s)) { }

	constexpr basic_string_view(std::nullptr_t) = delete;

	constexpr basic_string_view &operator=(const basic_string_view &view) = default;

	constexpr const_iterator begin() const
	{
		return this->_pointer;
	}

	constexpr const_iterator cbegin() const
	{
		return this->_pointer;
	}

	constexpr const_iterator end() const
	{
		return this->_pointer + this->_length;
	}

	constexpr const_iterator cend() const
	{
		return this->_pointer + this->_length;
	}

	constexpr const_reference operator[](size_type index) const
	{
		return this->_pointer[index];
	}

	constexpr const_reference at(size_type index) const
	{
		if (index > this->_length) index = this->_length - 1;
		return this->operator[](index);
	}

	constexpr const_reference front() const
	{
		return this->_pointer[0];
	}

	constexpr const_reference back() const
	{
		return this->_pointer[this->_length - 1];
	}

	constexpr const_pointer data() const
	{
		return this->_pointer;
	}

	constexpr size_type size() const
	{
		return this->_length;
	}

	constexpr size_type length() const
	{
		return this->_length;
	}

	constexpr size_type max_size() const
	{
		return size_type(-1) / sizeof(value_type);
	}

	[[nodiscard]] constexpr bool empty() const
	{
		return this->_length == 0;
	}

	constexpr void remove_prefix(size_type n)
	{
		this->_pointer += n;
		this->_length -= n;
	}

	constexpr void remove_suffix(size_type n)
	{
		this->_length -= n;
	}

	constexpr void swap(basic_string_view &v)
	{
		using std::swap;
		swap(this->_pointer, v._pointer);
		swap(this->_length, v._length);
	}

	constexpr size_type copy(Char *dest, size_type count, size_type pos = 0) const
	{
		if (pos > this->length) return 0;
		return Traits::copy(dest, this->_pointer + pos, count);
	}

	constexpr basic_string_view substr(size_type pos = 0, size_type count = npos) const
	{
		if (pos > this->_length) pos = this->_length;
		return basic_string_view(_pointer + pos, std::min(count, this->_length - pos));
	}

	constexpr int compare(basic_string_view v) const
	{
		auto rlen = std::min(this->_length, v._length);
		auto result = Traits::compare(this->_pointer, v._pointer, rlen);

		if (result == 0) return this->_length < v._length ? -1 : (this->_length > v._length ? 1 : 0);
		else return result;
	}

	constexpr int compare(size_type pos1, size_type count1, basic_string_view v) const
	{
		return this->substr(pos1, count1).compare(v);
	}

	constexpr int compare(size_type pos1, size_type count1, basic_string_view v, size_type pos2, size_type count2) const
	{
		return this->substr(pos1, count1).compare(v.substr(pos2, count2));
	}

	constexpr int compare(const Char *s) const
	{
		return this->compare(basic_string_view(s));
	}

	constexpr int compare(size_type pos1, size_type count1, const Char *s) const
	{
		return this->substr(pos1, count1).compare(basic_string_view(s));
	}

	constexpr int compare(size_type pos1, size_type count1, const Char *s, size_type count2) const
	{
		return this->substr(pos1, count1).compare(basic_string_view(s, count2));
	}

	constexpr bool starts_with(basic_string_view sv) const
	{
		return this->substr(0, sv.size()) == sv;
	}

	constexpr bool starts_with(Char c) const
	{
		return !this->empty() && Traits::eq(this->front(), c);
	}

	constexpr bool starts_with(const Char *s) const
	{
		return this->starts_with(basic_string_view(s));
	}

	constexpr bool ends_with(basic_string_view sv) const
	{
		return this->size() >= sv.size() && this->compare(this->size() - sv.size(), npos, sv) == 0;
	}

	constexpr bool ends_with(Char c) const
	{
		return !this->empty() && Traits::eq(this->back(), c);
	}

	constexpr bool ends_with(const Char *s) const
	{
		return this->ends_with(basic_string_view(s));
	}

	constexpr bool contains(basic_string_view sv) const
	{
		return this->find(sv) != npos;
	}

	constexpr bool contains(Char c) const
	{
		return this->find(c) != npos;
	}

	constexpr bool contains(const Char *s) const
	{
		return this->find(s) != npos;
	}

	constexpr size_type find(basic_string_view v, size_type pos = 0) const
	{
		if (v._length == 0) return pos <= this->_length ? pos : npos;

		if (v._length <= this->_length)
		{
			for (size_type i = pos; i <= this->_length - v._length; i++)
			{
				if (Traits::eq(this->_pointer[i], v._pointer[0]))
				{
					if (Traits::compare(this->_pointer + i + 1, v._pointer + 1, v._length - 1) == 0) return i;
				}
			}
		}
		return npos;
	}

	constexpr size_type find(Char c, size_type pos = 0) const
	{
		return this->find(basic_string_view(std::addressof(c), 1), pos);
	}

	constexpr size_type find(const Char *s, size_type pos, size_type count) const
	{
		return this->find(basic_string_view(s, count), pos);
	}

	constexpr size_type find(const Char *s, size_type pos = 0) const
	{
		return this->find(basic_string_view(s), pos);
	}

	constexpr size_type rfind(basic_string_view v, size_type pos = npos) const
	{
		if (v._length <= this->_length)
		{
			size_type i = std::min(size_type(this->_length - v._length), pos);
			do
			{
				if (Traits::compare(this->_pointer + i, v._pointer, v._length) == 0) return i;
			}
			while (i-- > 0);
		}
		return npos;
	}

	constexpr size_type rfind(Char c, size_type pos = npos) const
	{
		return this->rfind(basic_string_view(std::addressof(c), 1), pos);
	}

	constexpr size_type rfind(const Char *s, size_type pos, size_type count) const
	{
		return this->rfind(basic_string_view(s, count), pos);
	}

	constexpr size_type rfind(const Char *s, size_type pos = npos) const
	{
		return this->rfind(basic_string_view(s), pos);
	}

	constexpr size_type find_first_of(basic_string_view v, size_type pos = 0) const
	{
		if (v._length == 0) return npos;

		for (size_type i = pos; i < this->_length; i++)
		{
			if (Traits::find(v._pointer, v._length, this->_pointer[i]) != nullptr) return i;
		}
		return npos;
	}

	constexpr size_type find_first_of(Char c, size_type pos = 0) const
	{
		return this->find_first_of(basic_string_view(std::addressof(c), 1), pos);
	}

	constexpr size_type find_first_of(const Char *s, size_type pos, size_type count) const
	{
		return this->find_first_of(basic_string_view(s, count), pos);
	}

	constexpr size_type find_first_of(const Char *s, size_type pos = 0) const
	{
		return this->find_first_of(basic_string_view(s), pos);
	}

	constexpr size_type find_last_of(basic_string_view v, size_type pos = npos) const
	{
		if (this->_length == 0 || v._length == 0) return npos;

		size_type i = std::min(this->_length - 1, pos);
		do
		{
			if (Traits::find(v._pointer, v._length, this->_pointer[i]) != nullptr) return i;
		}
		while (i-- > 0);

		return npos;
	}

	constexpr size_type find_last_of(Char c, size_type pos = npos) const
	{
		return this->find_last_of(basic_string_view(std::addressof(c), 1), pos);
	}

	constexpr size_type find_last_of(const Char *s, size_type pos, size_type count) const
	{
		return this->find_last_of(basic_string_view(s, count), pos);
	}

	constexpr size_type find_last_of(const Char *s, size_type pos = npos) const
	{
		return this->find_last_of(basic_string_view(s), pos);
	}

	constexpr size_type find_first_not_of(basic_string_view v, size_type pos = 0) const
	{
		for (size_type i = pos; i < this->_length; i++)
		{
			if (Traits::find(v._pointer, v._length, this->_pointer[i]) == nullptr) return i;
		}
		return npos;
	}

	constexpr size_type find_first_not_of(Char c, size_type pos = 0) const
	{
		return this->find_first_not_of(basic_string_view(std::addressof(c), 1), pos);
	}

	constexpr size_type find_first_not_of(const Char *s, size_type pos, size_type count) const
	{
		return this->find_first_not_of(basic_string_view(s, count), pos);
	}

	constexpr size_type find_first_not_of(const Char *s, size_type pos = 0) const
	{
		return this->find_first_not_of(basic_string_view(s), pos);
	}

	constexpr size_type find_last_not_of(basic_string_view v, size_type pos = npos) const
	{
		if (this->_length == 0) return npos;

		size_type i = std::min(this->_length - 1, pos);
		do
		{
			if (Traits::find(v._pointer, v._length, this->_pointer[i]) == nullptr) return i;
		}
		while (i--);

		return npos;
	}

	constexpr size_type find_last_not_of(Char c, size_type pos = npos) const
	{
		return this->find_last_not_of(basic_string_view(std::addressof(c), 1), pos);
	}

	constexpr size_type find_last_not_of(const Char *s, size_type pos, size_type count) const
	{
		return this->find_last_not_of(basic_string_view(s, count), pos);
	}

	constexpr size_type find_last_not_of(const Char *s, size_type pos = npos) const
	{
		return this->find_last_not_of(basic_string_view(s), pos);
	}

	friend constexpr bool operator==(basic_string_view lhs, basic_string_view rhs)
	{
		return lhs.compare(rhs) == 0;
	}

	friend constexpr auto operator<=>(basic_string_view lhs, basic_string_view rhs)
	{
		return lhs.compare(rhs) <=> 0;
	}
};

using string_view = basic_string_view<char, char_traits<char>>;

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

	basic_string(Allocator allocator = Allocator())
	: _allocator{std::move(allocator)}, _buffer{nullptr}, _length{0} { }

	basic_string(const Char *c_string, Allocator allocator = Allocator())
	: _allocator{std::move(allocator)} {
		_length = strlen(c_string);
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
		if(_length != strlen(other))
			return _length < strlen(other) ? -1 : 1;
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

	bool operator!= (const basic_string &other) const {
		return compare(other) != 0;
	}

	bool operator!= (const char *rhs) const {
		return compare(rhs) != 0;
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

inline namespace literals
{
	inline namespace string_view_literals
	{
		#pragma GCC diagnostic push
		#if defined(__clang__)
		#pragma GCC diagnostic ignored "-Wuser-defined-literals"
		#elif defined(__GNUC__)
		#pragma GCC diagnostic ignored "-Wno-literal-suffix"
		#endif

		inline string operator""s(const char *str, size_t len)
		{
			return string(str, len);
		}

		inline std::string_view operator""sv(const char *str, size_t len)
		{
			return std::string_view(str, len);
		}

		#pragma GCC diagnostic pop
	} // inline namespace string_view_literals
} // inline namespace literals

} // namespace frg

#endif // FRG_STRING_HPP