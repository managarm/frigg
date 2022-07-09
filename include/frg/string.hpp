#ifndef FRG_STRING_HPP
#define FRG_STRING_HPP

#include <initializer_list>
#include <type_traits>
#include <algorithm>
#include <concepts>
#include <string.h>
#include <compare>
#include <utility>
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

template<typename Char, typename Traits = char_traits<Char>, typename Allocator = std::allocator<Char>>
class basic_string
{
	private:
	Allocator _allocator;
	Char *_buffer = nullptr;
	size_t _length = 0;
	size_t _cap = 0;

	public:
	using traits_type = Traits;
	using value_type = Char;
	using allocator_type = Allocator;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using pointer = Char*;
	using const_pointer = const Char*;
	using reference = Char&;
	using const_reference = const Char&;

	// TODO: use real iterators?
	using iterator = Char*;
	using const_iterator = const Char*;

	static constexpr size_type npos = size_type(-1);

	private:
	constexpr void null_terminate()
	{
		if (this->_buffer == nullptr || this->_length == 0) return;
		this->_buffer[this->_length] = 0;
	}

	constexpr Char *reallocate(size_t new_cap)
	{
		auto new_length = std::min(new_cap, this->_length);
		auto new_buffer = static_cast<Char*>(this->_allocator.allocate(new_cap + 1));

		Traits::copy(new_buffer, this->_buffer, new_length);
		this->_allocator.deallocate(this->_buffer, this->_cap + 1);

		this->_buffer = new_buffer;
		this->_cap = new_cap;
		this->_length = new_length;

		return new_buffer;
	}

	constexpr size_t it2pos(const_iterator pos)
	{
		if (this->_buffer == nullptr || this->_length == 0) return npos;
		pos = std::max(pos, const_iterator(this->_buffer));
		pos = std::min(pos, const_iterator(this->_buffer + this->_length));

		return static_cast<size_type>(pos - this->_buffer) / sizeof(Char);
	}

	constexpr size_t range2count(const_iterator first, const_iterator last)
	{
		if (this->_buffer == nullptr || this->_length == 0) return npos;
		first = std::max(first, const_iterator(this->_buffer));
		first = std::min(first, const_iterator(this->_buffer + this->_length));

		last = std::max(last, const_iterator(this->_buffer));
		last = std::min(last, const_iterator(this->_buffer + this->_length));

		last = std::max(first, last);

		return static_cast<size_type>(last - first) / sizeof(Char);
	}

	public:
	constexpr basic_string() : basic_string(Allocator()) { }
	explicit constexpr basic_string(const Allocator &alloc) : _allocator(alloc), _buffer(nullptr), _length(0), _cap(0) { }

	constexpr basic_string(size_type count, Char c, const Allocator &alloc = Allocator()) : _allocator(alloc), _length(count), _cap(count)
	{
		this->_buffer = static_cast<Char*>(this->_allocator.allocate(this->_length + 1));
		Traits::assign(this->_buffer, count, c);
		this->null_terminate();
	}

	constexpr basic_string(const basic_string &other, size_type pos, size_type count, const Allocator &alloc = Allocator()) : _allocator(alloc)
	{
		if (pos + count > other._length) count = other._length - pos;
		this->_length = this->_cap = count;

		this->_buffer = static_cast<Char*>(this->_allocator.allocate(this->_length + 1));
		Traits::copy(this->_buffer, other._buffer + pos, this->_length);
		this->null_terminate();
	}
	constexpr basic_string(const basic_string &other, size_type pos, const Allocator &alloc = Allocator()) : basic_string(other, pos, other._length, alloc) { }

	constexpr basic_string(const Char *s, size_type count, const Allocator &alloc = Allocator()) : _allocator(alloc), _length(count), _cap(count)
	{
		this->_buffer = static_cast<Char*>(this->_allocator.allocate(this->_length + 1));
		Traits::copy(this->_buffer, s, this->_length);
		this->null_terminate();
	}
	constexpr basic_string(const Char *s, const Allocator &alloc = Allocator()) : basic_string(s, Traits::length(s), alloc) { }

	constexpr basic_string(const basic_string &other, const Allocator &alloc) : _allocator(alloc), _length(other._length), _cap(other._length)
	{
		this->_buffer = static_cast<Char*>(this->_allocator.allocate(this->_length + 1));
		Traits::copy(this->_buffer, other._buffer, this->_length);
		this->null_terminate();
	}

	constexpr basic_string(const basic_string &other) : _allocator(other._allocator), _length(other._length), _cap(other._length)
	{
		this->_buffer = static_cast<Char*>(this->_allocator.allocate(this->_length + 1));
		Traits::copy(this->_buffer, other._buffer, this->_length);
		this->null_terminate();
	}

	constexpr basic_string(basic_string &&other) = default;
	constexpr basic_string(basic_string &&other, const Allocator &alloc) : _allocator(alloc), _buffer(std::move(other._buffer)), _length(std::move(other._length)), _cap(std::move(other._length)) { }

	constexpr basic_string(std::initializer_list<Char> ilist, const Allocator &alloc = Allocator()) : basic_string(ilist.begin(), ilist.size(), alloc) { }

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	explicit constexpr basic_string(const type &t, const Allocator &alloc = Allocator()) : basic_string(basic_string_view<Char>(t).data(), basic_string_view<Char>(t).size(), alloc) { }

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>>)
	constexpr basic_string(const type &t, size_type pos, size_type n, const Allocator &alloc = Allocator()) : basic_string(basic_string_view<Char>(t).substr(pos, n), alloc) { }

	constexpr basic_string(std::nullptr_t) = delete;

	~basic_string()
	{
		if (this->_buffer) this->_allocator.deallocate(this->_buffer, this->_cap + 1);
		this->_length = this->_cap = 0;
	}

	constexpr basic_string &operator=(const basic_string &str)
	{
		return this->assign(str);
	}

	constexpr basic_string &operator=(basic_string &&str) = default;

	constexpr basic_string &operator=(const Char *s)
	{
		return this->assign(s, Traits::length(s));
	}

	constexpr basic_string &operator=(Char c)
	{
		return this->assign(std::addressof(c), 1);
	}

	constexpr basic_string &operator=(std::initializer_list<Char> ilist)
	{
		return this->assign(ilist.begin(), ilist.size());
	}

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	constexpr basic_string &operator=(const type &t)
	{
		return this->assign(basic_string_view<Char>(t));
	}

	constexpr basic_string &operator=(std::nullptr_t) = delete;

	constexpr basic_string &assign(size_type count, Char ch)
	{
		this->clear();
		this->resize(count, ch);
		this->null_terminate();

		return *this;
	}

	constexpr basic_string &assign(const basic_string &str)
	{
		this->clear();
		this->append(str);

		return *this;
	}

	constexpr basic_string &assign(const basic_string &str, size_type pos, size_type count = npos)
	{
		if (pos > str._length) return *this;
		if (pos + count > str._length || count == npos) count = str._length - pos;

		this->clear();
		this->append(str.data() + pos, count);

		return *this;
	}

	constexpr basic_string &assign(basic_string &&str)
	{
		return this->swap(str);
	}

	constexpr basic_string &assign(const Char *s, size_type count)
	{
		this->clear();
		this->append(s, count);
		this->null_terminate();

		return *this;
	}

	constexpr basic_string &assign(const Char *s)
	{
		this->clear();
		this->append(s, Traits::length(s));
		this->null_terminate();

		return *this;
	}

	constexpr basic_string &assign(std::initializer_list<Char> ilist)
	{
		return this->assign(ilist.begin(), ilist.size());
	}

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	constexpr basic_string &assign(const type &t)
	{
		basic_string_view<Char> sv(t);
		return this->assign(sv.data(), sv.size());
	}

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	constexpr basic_string &assign(const type &t, size_type pos, size_type count = npos)
	{
		basic_string_view<Char> sv(t);
		if (pos > sv.size()) return *this;
		if (pos + count > sv.size() || count == npos) count = sv.size() - pos;

		return this->assign(sv.data() + pos, count);
	}

	constexpr allocator_type get_allocator() const
	{
		return this->_allocator;
	}

	constexpr reference at(size_type index)
	{
		index = std::min(index, this->_length - 1);
		return this->_buffer[index];
	}

	constexpr const_reference at(size_type index) const
	{
		index = std::min(index, this->_length - 1);
		return this->_buffer[index];
	}

	constexpr reference operator[](size_type index)
	{
		index = std::min(index, this->_length - 1);
		return this->_buffer[index];
	}

	constexpr const_reference operator[](size_type index) const
	{
		index = std::min(index, this->_length - 1);
		return this->_buffer[index];
	}

	constexpr reference front()
	{
		return this->operator[](0);
	}

	constexpr const_reference front() const
	{
		return this->operator[](0);
	}

	constexpr reference back()
	{
		return this->operator[](this->_length - 1);
	}

	constexpr const_reference back() const
	{
		return this->operator[](this->_length - 1);
	}

	constexpr pointer data()
	{
		return this->_buffer;
	}

	constexpr const_pointer data() const
	{
		return this->_buffer;
	}

	constexpr const_pointer c_str() const
	{
		return this->_buffer;
	}

	constexpr operator basic_string_view<Char>() const
	{
		return basic_string_view<Char>(this->_buffer, this->_length);
	}

	constexpr iterator begin()
	{
		return this->_buffer;
	}

	constexpr const_iterator cbegin() const
	{
		return this->_buffer;
	}

	constexpr iterator end()
	{
		return this->_buffer + this->_length;
	}

	constexpr const_iterator cend() const
	{
		return this->_buffer + this->_length;
	}

	[[nodiscard]] constexpr bool empty() const
	{
		return this->_length == 0;
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
		return size_type(-1) / sizeof(Char);
	}

	constexpr size_type capacity() const
	{
		return this->_cap;
	}

	constexpr void shrink_to_fit()
	{
		if (this->_length == this->_cap) return;
		this->_buffer = this->reallocate(this->_length);
	}

	constexpr void clear()
	{
		this->erase();
	}

	constexpr basic_string &insert(size_type index, size_type count, Char ch)
	{
		if (count == 0 || index > this->_length) return *this;

		size_type new_size = this->_length + count;
		this->resize(new_size);

		Traits::move(this->_buffer + index + count, this->_buffer + index, this->_length  - index);
		Traits::assign(this->_buffer + index, count, ch);

		this->_length += count;
		this->null_terminate();
		return *this;
	}

	constexpr basic_string &insert(size_type index, const Char *s)
	{
		return this->insert(index, s, Traits::length(s));
	}

	constexpr basic_string &insert(size_type index, const Char *s, size_type count)
	{
		if (count == 0 || index > this->_length) return *this;

		size_type new_size = this->_length + count;
		this->resize(new_size);

		Traits::move(this->_buffer + index + count, this->_buffer + index, this->_length - index);
		Traits::copy(this->_buffer + index, s, count);

		this->_length += count;
		this->null_terminate();
		return *this;
	}

	constexpr basic_string &insert(size_type index, const basic_string &str)
	{
		return this->insert(index, str.c_str(), str.size());
	}

	constexpr basic_string &insert(size_type index, const basic_string &str, size_type index_str, size_type count = npos)
	{
		basic_string newstr = str.substr(index_str, count);
		return this->insert(index, newstr.c_str(), newstr.size());
	}

	constexpr iterator insert(const_iterator pos, Char c)
	{
		this->insert(this->it2pos(pos), 1, c);
		return const_cast<iterator>(pos);
	}

	constexpr iterator insert(const_iterator pos, size_type count, Char c)
	{
		this->insert(this->it2pos(pos), count, c);
		return const_cast<iterator>(pos);
	}

	constexpr iterator insert(const_iterator pos, std::initializer_list<Char> ilist)
	{
		this->insert(this->it2pos(pos), ilist.begin(), ilist.size());
		return const_cast<iterator>(pos);
	}

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	constexpr basic_string &insert(size_type pos, const type &t)
	{
		basic_string_view<Char> sv(t);
		return this->insert(pos, sv.data(), sv.size());
	}

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	constexpr basic_string &insert(size_type index, const type &t, size_type index_str, size_type count = npos)
	{
		basic_string_view<Char> sv(t);
		if (index_str > sv.size()) return *this;
		if (index_str + count > sv.size() || count == npos) count = sv.size() - index_str;

		return this->insert(index_str, sv.data(), count);
	}

	constexpr basic_string &erase(size_type index = 0, size_type count = npos)
	{
		if (index > this->_length) return *this;
		count = std::min(count, this->_length - index);
		if (count == 0) return *this;

		Traits::move(this->_buffer + index, this->_buffer + index + count, this->_length - (index + count));

		this->_length -= count;
		this->null_terminate();
		return *this;
	}

	constexpr iterator erase(const_iterator pos)
	{
		this->erase(this->it2pos(pos), 1);
		return this->end();
	}

	constexpr iterator erase(const_iterator first, const_iterator last)
	{
		this->erase(this->it2pos(first), this->range2count(first, last));
		return const_cast<iterator>(first);
	}

	constexpr void push_back(Char ch)
	{
		this->append(1, ch);
	}

	constexpr void pop_back()
	{
		this->erase(this->end() - 1);
	}

	constexpr basic_string &append(size_type count, Char ch)
	{
		return this->insert(this->_length, count, ch);
	}

	constexpr basic_string &append(const basic_string &str)
	{
		return this->insert(this->_length, str);
	}

	constexpr basic_string &append(const basic_string &str, size_type pos, size_type count = npos)
	{
		if (pos > str._length) return *this;
		if (pos + count > str._length || count == npos) count = str._length - pos;

		return this->insert(this->_length, str.substr);
	}

	constexpr basic_string &append(const Char *s, size_type count)
	{
		return this->insert(this->_length, s, count);
	}

	constexpr basic_string &append(const Char *s)
	{
		return this->insert(this->_length, s, Traits::length(s));
	}

	constexpr basic_string &append(std::initializer_list<Char> ilist)
	{
		return this->append(ilist.begin(), ilist.size());
	}

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	constexpr basic_string &append(const type &t)
	{
		basic_string_view<Char> sv(t);
		return this->append(sv.data(), sv.size());
	}

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	constexpr basic_string &append(const type &t, size_type pos, size_type count = npos)
	{
		basic_string_view<Char> sv(t);
		if (pos >= sv.size()) return *this;
		if (pos + count > sv.size() || count == npos) count = sv.size() - pos;

		return this->append(sv.data(), sv.size());
	}

	constexpr basic_string &operator+=(std::initializer_list<Char> ilist)
	{
		return this->append(ilist.begin(), ilist.size());
	}

	constexpr basic_string &operator+=(const basic_string &str)
	{
		return this->append(str);
	}

	constexpr basic_string &operator+=(Char c)
	{
		return this->append(1, c);
	}

	constexpr basic_string &operator+=(const Char *s)
	{
		return this->append(s);
	}

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	constexpr basic_string &operator+=(const type &t)
	{
		basic_string_view<Char> sv(t);
		return this->append(sv);
	}

	constexpr int compare(const basic_string &str) const
	{
		if (this->_length != str._length) return this->_length < str._length ? -1 : 1;
		return Traits::compare(this->_buffer, str._buffer, this->_length);
	}

	constexpr int compare(size_type pos1, size_type count1, const basic_string &str) const
	{
		if (pos1 + count1 > this->_length) count1 = this->_length - pos1;
		if (count1 != str._length) return count1 < str._length ? -1 : 1;

		return Traits::compare(this->_buffer + pos1, str._buffer, count1);
	}

	constexpr int compare(size_type pos1, size_type count1, const basic_string &str, size_type pos2, size_type count2 = npos) const
	{
		if (pos1 + count1 > this->_length) count1 = this->_length - pos1;
		if (pos2 + count2 > str._length) count2 = str._length - pos2;

		if (count1 != count2) return count1 < count2 ? -1 : 1;

		return Traits::compare(this->_buffer + pos1, str._buffer + pos2, count1);
	}

	constexpr int compare(const Char *s) const
	{
		size_type sl = Traits::length(s);
		if (this->_length != sl) return this->_length < sl ? -1 : 1;

		return Traits::compare(this->_buffer, s, this->_length);
	}

	constexpr int compare(size_type pos1, size_type count1, const Char *s) const
	{
		size_type sl = Traits::length(s);
		if (pos1 + count1 > this->_length) count1 = this->_length - pos1;
		if (count1 != sl) return count1 < sl ? -1 : 1;

		return Traits::compare(this->_buffer + pos1, s, count1);
	}

	constexpr int compare(size_type pos1, size_type count1, const Char *s, size_type count2) const
	{
		if (pos1 + count1 > this->_length) count1 = this->_length - pos1;
		if (count1 != count2) return count1 < count2 ? -1 : 1;

		return Traits::compare(this->_buffer + pos1, s, count1);
	}

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	constexpr int compare(const type &t) const
	{
		basic_string_view<Char> sv(t);
		if (this->_length != sv.size()) return this->_length < sv.size() ? -1 : 1;

		return Traits::compare(this->_buffer, sv.data(), this->_length);
	}

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	constexpr int compare(size_type pos1, size_type count1, const type &t) const
	{
		basic_string_view<Char> sv(t);
		if (count1 != sv.size()) return count1 < sv.size() ? -1 : 1;

		return basic_string_view<Char>(*this).substr(pos1, count1).compare(sv);
	}

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	constexpr int compare(size_type pos1, size_type count1, const type &t, size_type pos2, size_type count2 = npos) const
	{
		basic_string_view<Char> sv(t);
		sv = sv.substr(pos2, count2);
		if (count1 != count2) return count1 < count2 ? -1 : 1;

		return basic_string_view<Char>(*this).substr(pos1, count1).compare(sv);
	}

	constexpr bool starts_with(basic_string_view<Char> sv) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).starts_with(sv);
	}

	constexpr bool starts_with(Char c) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).starts_with(c);
	}

	constexpr bool starts_with(const Char *s) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).starts_with(s);
	}

	constexpr bool ends_with(basic_string_view<Char> sv) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).ends_with(sv);
	}

	constexpr bool ends_with(Char c) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).ends_with(c);
	}

	constexpr bool ends_with(const Char *s) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).ends_with(s);
	}

	constexpr bool contains(basic_string_view<Char> sv) const
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

	constexpr basic_string &replace(size_type pos, size_type count, const basic_string &str)
	{
		if (pos > this->_length) return *this;
		if (pos + count > this->_length) count = this->_length - pos;

		this->erase(pos, count);
		this->insert(pos, str);

		return *this;
	}

	constexpr basic_string &replace(const_iterator first, const_iterator last, const basic_string &str)
	{
		return this->replace(this->it2pos(first), this->range2count(first, last), str);
	}

	constexpr basic_string &replace(size_type pos, size_type count, const basic_string &str, size_type pos2, size_type count2 = npos)
	{
		if (pos > this->_length) return *this;
		if (pos + count > this->_length) count = this->_length - pos;

		if (pos2 > str._length) return *this;
		if (pos2 + count2 > str._length || count2 == npos) count2 = str._length - pos2;

		this->erase(pos, count);
		this->insert(pos, str, pos2, count2);

		return *this;
	}

	constexpr basic_string &replace(size_type pos, size_type count, const Char *str, size_type count2)
	{
		if (pos > this->_length) return *this;
		if (pos + count > this->_length) count = this->_length - pos;

		this->erase(pos, count);
		this->insert(pos, str, count2);

		return *this;
	}

	constexpr basic_string &replace(const_iterator first, const_iterator last, const Char *str, size_type count2)
	{
		return this->replace(this->it2pos(first), this->range2count(first, last), str, count2);
	}

	constexpr basic_string &replace(size_type pos, size_type count, const Char *str)
	{
		if (pos > this->_length) return *this;
		if (pos + count > this->_length) count = this->_length - pos;

		this->erase(pos, count);
		this->insert(pos, str, Traits::length(str));

		return *this;
	}

	constexpr basic_string &replace(const_iterator first, const_iterator last, const Char *str)
	{
		return this->replace(this->it2pos(first), this->range2count(first, last), str);
	}

	constexpr basic_string &replace(size_type pos, size_type count, size_type count2, Char c)
	{
		if (pos > this->_length) return *this;
		if (pos + count > this->_length) count = this->_length - pos;

		this->erase(pos, count);
		this->insert(pos, count2, c);

		return *this;
	}

	constexpr basic_string &replace(const_iterator first, const_iterator last, size_type count2, Char c)
	{
		return this->replace(this->it2pos(first), this->range2count(first, last), count2, c);
	}

	constexpr basic_string &replace(const_iterator first, const_iterator last, std::initializer_list<Char> ilist)
	{
		return this->replace(this->it2pos(first), this->range2count(first, last), ilist.begin(), ilist.size());
	}

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	constexpr basic_string &replace(size_type pos, size_type count, const type &t)
	{
		basic_string_view<Char> sv(t);

		if (pos > this->_length) return *this;
		if (pos + count > this->_length) count = this->_length - pos;

		this->erase(pos, count);
		this->insert(pos, sv.data(), sv.size());

		return *this;
	}

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	constexpr basic_string &replace(const_iterator first, const_iterator last, const type &t)
	{
		return this->replace(this->it2pos(first), this->range2count(first, last), t);
	}

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	constexpr basic_string &replace(size_type pos, size_type count, const type &t, size_type pos2, size_type count2 = npos)
	{
		basic_string_view<Char> sv(t);

		if (pos > this->_length) return *this;
		if (pos + count > this->_length) count = this->_length - pos;

		if (pos2 > sv.size()) return *this;
		if (pos2 + count2 > sv.size() || count2 == npos) count2 = sv.size() - pos2;

		this->erase(pos, count);
		this->insert(pos, sv.data(), pos2, count2);

		return *this;
	}

	constexpr basic_string substr(size_type pos = 0, size_type count = npos) const
	{
		if (pos > this->_length) return *this;
		if (pos + count > this->_length || count == npos) count = this->_length - pos;

		return basic_string(this->_buffer + pos, count);
	}

	constexpr size_type copy(Char *dest, size_type count, size_type pos = 0) const
	{
		if (pos > this->_length) return *this;
		if (pos + count > this->_length || count == npos) count = this->_length - pos;

		memcpy(dest, this->_buffer + pos, count);
		return count;
	}

	constexpr void resize(size_type count)
	{
		size_type oldsize = this->_length;

		this->_buffer = this->reallocate(count);
		if (count > oldsize)
		{
			Traits::assign(this->_buffer + oldsize, count - oldsize, 0);
		}
		this->_buffer[count] = 0;

		if (count < oldsize) this->_length = count;
		this->_cap = count;
	}

	constexpr void resize(size_type count, Char ch)
	{
		size_type oldsize = this->_length;

		this->_buffer = this->reallocate(count);
		if (count > oldsize)
		{
			Traits::assign(this->_buffer + oldsize, count - oldsize, ch);
		}
		this->_buffer[count] = 0;

		this->_length = this->_cap = count;
	}

	constexpr void swap(basic_string &other)
	{
		using std::swap;
		swap(this->_buffer, other._buffer);
		swap(this->_length, other._length);
		swap(this->_cap, other._cap);
	}

	constexpr size_type find(const basic_string &str, size_type pos = 0) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find(basic_string_view<Char>(str), pos);
	}

	constexpr size_type find(const Char *s, size_type pos, size_type count) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find(s, pos, count);
	}

	constexpr size_type find(const Char *s, size_type pos = 0) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find(s, pos);
	}

	constexpr size_type find(Char c, size_type pos = 0) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find(c, pos);
	}

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	constexpr size_type find(const type &t, size_type pos = 0) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find(basic_string_view<Char>(t), pos);
	}

	constexpr size_type rfind(const basic_string &str, size_type pos = npos) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).rfind(basic_string_view<Char>(str), pos);
	}

	constexpr size_type rfind(const Char *s, size_type pos, size_type count) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).rfind(s, pos, count);
	}

	constexpr size_type rfind(const Char *s, size_type pos = npos) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).rfind(s, pos);
	}

	constexpr size_type rfind(Char c, size_type pos = npos) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).rfind(c, pos);
	}

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	constexpr size_type rfind(const type &t, size_type pos = npos) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).rfind(basic_string_view<Char>(t), pos);
	}

	constexpr size_type find_first_of(const basic_string &str, size_type pos = 0) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find_first_of(basic_string_view<Char>(str), pos);
	}

	constexpr size_type find_first_of(const Char *s, size_type pos, size_type count) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find_first_of(s, pos, count);
	}

	constexpr size_type find_first_of(const Char *s, size_type pos = 0) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find_first_of(s, pos);
	}

	constexpr size_type find_first_of(Char c, size_type pos = 0) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find_first_of(c, pos);
	}

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	constexpr size_type find_first_of(const type &t, size_type pos = 0) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find_first_of(basic_string_view<Char>(t), pos);
	}

	constexpr size_type find_first_not_of(const basic_string &str, size_type pos = 0) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find_first_not_of(basic_string_view<Char>(str), pos);
	}

	constexpr size_type find_first_not_of(const Char *s, size_type pos, size_type count) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find_first_not_of(s, pos, count);
	}

	constexpr size_type find_first_not_of(const Char *s, size_type pos = 0) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find_first_not_of(s, pos);
	}

	constexpr size_type find_first_not_of(Char c, size_type pos = 0) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find_first_not_of(c, pos);
	}

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	constexpr size_type find_first_not_of(const type &t, size_type pos = 0) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find_first_not_of(basic_string_view<Char>(t), pos);
	}

	constexpr size_type find_last_of(const basic_string &str, size_type pos = npos) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find_last_of(basic_string_view<Char>(str), pos);
	}

	constexpr size_type find_last_of(const Char *s, size_type pos, size_type count) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find_last_of(s, pos, count);
	}

	constexpr size_type find_last_of(const Char *s, size_type pos = npos) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find_last_of(s, pos);
	}

	constexpr size_type find_last_of(Char c, size_type pos = npos) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find_last_of(c, pos);
	}

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	constexpr size_type find_last_of(const type &t, size_type pos = npos) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find_last_of(basic_string_view<Char>(t), pos);
	}

	constexpr size_type find_last_not_of(const basic_string &str, size_type pos = npos) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find_last_not_of(basic_string_view<Char>(str), pos);
	}

	constexpr size_type find_last_not_of(const Char *s, size_type pos, size_type count) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find_last_not_of(s, pos, count);
	}

	constexpr size_type find_last_not_of(const Char *s, size_type pos = npos) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find_last_not_of(s, pos);
	}

	constexpr size_type find_last_not_of(Char c, size_type pos = npos) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find_last_not_of(c, pos);
	}

	template<typename type> requires (std::convertible_to<const type&, basic_string_view<Char>> && !std::convertible_to<const type&, const Char*>)
	constexpr size_type find_last_not_of(const type &t, size_type pos = npos) const
	{
		return basic_string_view<Char>(this->_buffer, this->_length).find_last_not_of(basic_string_view<Char>(t), pos);
	}

	friend constexpr basic_string operator+(const basic_string &lhs, const basic_string &rhs)
	{
		return basic_string<Char>(lhs).append(rhs);
	}

	friend constexpr basic_string operator+(const basic_string &lhs, const Char *rhs)
	{
		return basic_string<Char>(lhs).append(rhs);
	}

	friend constexpr basic_string operator+(const basic_string &lhs, Char rhs)
	{
		return basic_string<Char>(lhs).append(rhs);
	}

	friend constexpr basic_string operator+(const Char *lhs, const basic_string &rhs)
	{
		return basic_string<Char>(lhs).append(rhs);
	}

	friend constexpr basic_string operator+(Char lhs, const basic_string &rhs)
	{
		return basic_string<Char>(std::addressof(lhs), 1).append(rhs);
	}

	friend constexpr basic_string operator+(basic_string &&lhs, basic_string &&rhs)
	{
		const auto _size = lhs.size() + rhs.size();
		return (_size > lhs.capacity() && _size <= rhs.capacity()) ? std::move(rhs.insert(0, lhs)) : std::move(lhs.append(rhs));
	}

	friend constexpr basic_string operator+(basic_string &&lhs, const basic_string &rhs)
	{
		return std::move(lhs.append(rhs));
	}

	friend constexpr basic_string operator+(basic_string &&lhs, const Char *rhs)
	{
		return std::move(lhs.append(rhs));
	}

	friend constexpr basic_string operator+(basic_string &&lhs, Char rhs)
	{
		return std::move(lhs.append(1, rhs));
	}

	friend constexpr basic_string operator+(const basic_string &lhs, basic_string &&rhs)
	{
		return std::move(rhs.insert(0, lhs));
	}

	friend constexpr basic_string operator+(const Char *lhs, basic_string &&rhs)
	{
		return std::move(rhs.insert(0, lhs));
	}

	friend constexpr basic_string operator+(Char lhs, basic_string &&rhs)
	{
		return std::move(rhs.insert(0, 1, lhs));
	}

	friend constexpr bool operator==(const basic_string &lhs, const basic_string &rhs)
	{
		return lhs.compare(rhs) == 0;
	}

	friend constexpr bool operator==(const basic_string &lhs, const Char *rhs)
	{
		return lhs.compare(rhs) == 0;
	}

	friend constexpr auto operator<=>(const basic_string &lhs, const basic_string &rhs)
	{
		return lhs.compare(rhs) <=> 0;
	}

	friend constexpr auto operator<=>(const basic_string &lhs, const Char *rhs)
	{
		return lhs.compare(rhs) <=> 0;
	}

	friend constexpr void swap(basic_string &lhs, basic_string &rhs)
	{
		lhs.swap(rhs);
	}

	basic_string_view<Char> sub_view() const
	{
		return basic_string_view<Char>(_buffer, this->_length);
	}
	basic_string_view<Char> sub_view(size_type start) const
	{
		return basic_string_view<Char>(_buffer + start, this->_length - start);
	}
	basic_string_view<Char> sub_view(size_type start, size_type length) const
	{
		return basic_string_view<Char>(_buffer + start, (start + length) > this->_length ? this->_length - start : length);
	}
};

template<typename Allocator>
using string = basic_string<char, char_traits<char>, Allocator>;
using string_view = basic_string_view<char>;

template<typename Char, typename Traits, typename Allocator>
struct hash<basic_string<Char, Traits, Allocator>>
{
	size_t operator()(const basic_string<Char, Traits, Allocator> &str) const
	{
		return MurmurHash2_64A(str.data(), str.length());
	}
};

template<typename Char, typename Traits>
struct hash<basic_string_view<Char, Traits>>
{
	size_t operator()(const basic_string_view<Char, Traits> &str) const
	{
		return MurmurHash2_64A(str.data(), str.length());
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

		inline basic_string<char> operator""s(const char *str, size_t len)
		{
			return basic_string<char>(str, len);
		}

		inline basic_string_view<char> operator""sv(const char *str, size_t len)
		{
			return basic_string_view<char>(str, len);
		}

		#pragma GCC diagnostic pop
	} // inline namespace string_view_literals
} // inline namespace literals

} // namespace frg

#endif // FRG_STRING_HPP