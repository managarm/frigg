#ifndef FRG_FORMATTING_HPP
#define FRG_FORMATTING_HPP

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <frg/macros.hpp>
#include <frg/optional.hpp>
#include <frg/string.hpp>
#include <frg/utility.hpp>

namespace frg FRG_VISIBILITY {

// Concept: Formatter.
// Supports a single operation: append().
// append() is overloaded for a variety of types.

// TODO: Does it make sense for this library to implement Formatters at all?
// TODO: Or should we only implement the formatting itself?

template<typename C, size_t Limit>
struct limited_formatter {
	limited_formatter()
	: _off{0} { }

	void append(C s) {
		_buffer[_off++] = s;
	}

	void append(C *str) {
		while(*str)
			_buffer[_off++] = *str++;
	}

private:
	C _buffer[Limit];
	size_t _off;
};

// ----------------------------------------------------------------------------
// General formatting machinery.
// ----------------------------------------------------------------------------

enum class format_conversion {
	null,
	decimal,
	hex
};

struct format_options {
	format_options()
	: conversion{format_conversion::null} { }

	format_options with_conversion(format_conversion c) {
		auto copy = *this;
		copy.conversion = c;
		return copy;
	}

	format_conversion conversion;
	int minimum_width = 0;
	optional<int> precision;
	bool left_justify = false;
	bool always_sign = false;
	bool plus_becomes_space = false;
	bool alt_conversion = false;
	bool fill_zeros = false;
};

// ----------------------------------------------------------------------------
// Formatting primitives for built-in types.
// ----------------------------------------------------------------------------

namespace _fmt_basics {
	// width: Minimum width of the output (padded with spaces by default).
	// precision: Minimum number of digits in the ouput (always padded with zeros).
	template<typename P, typename T>
	void print_digits(P &formatter, T number, bool negative, int radix,
			int width, int precision, char padding) {
		const char *digits = "0123456789abcdef";

		// print the number in reverse order and determine #digits.
		char buffer[32];
		int k = 0; // number of digits
		do {
			FRG_ASSERT(k < 32); // TODO: variable number of digits
			buffer[k++] = digits[number % radix];
			number /= radix;
		} while(number);

		if(negative) {
			FRG_ASSERT(k < 32); // TODO: variable number of digits
			buffer[k++] = '-';
		}

		if(max(k, precision) < width)
			for(int i = 0; i < width - max(k, precision); i++)
				formatter.append(padding);
		if(k < precision)
			for(int i = 0; i < precision - k; i++)
				formatter.append('0');
		for(int i = k - 1; i >= 0; i--)
			formatter.append(buffer[i]);
	}

	template<typename T>
	struct make_unsigned;

	template<> struct make_unsigned<int> { using type = unsigned int; };
	template<> struct make_unsigned<unsigned int> { using type = unsigned int; };
	template<> struct make_unsigned<long> { using type = unsigned long; };
	template<> struct make_unsigned<unsigned long> { using type = unsigned long; };
	template<> struct make_unsigned<long long> { using type = unsigned long long; };
	template<> struct make_unsigned<unsigned long long> { using type = unsigned long long; };

	// Signed integer formatting. We cannot print -x as that might not fit into the signed type.
	// Strategy: Cast to unsigned first (i.e. obtain 2s complement) and negate manually by
	// computing (~x + 1).
	template<typename P, typename T>
	void print_int(P &formatter, T number, int radix, int width = 0,
			int precision = 1, char padding = ' ') {
		if(number < 0) {
			auto absv = ~static_cast<typename make_unsigned<T>::type>(number) + 1;
			print_digits(formatter, absv, true, radix, width, precision, padding);
		}else{
			print_digits(formatter, number, false, radix, width, precision, padding);
		}
	}

	template<typename T, typename F>
	void format_integer(T object, format_options fo, F &formatter) {
		if(fo.conversion == format_conversion::hex) {
			print_int(formatter, object, 16);
		}else{
			FRG_ASSERT(fo.conversion == format_conversion::null
					|| fo.conversion == format_conversion::decimal);
			print_int(formatter, object, 10);
		}
	}
};

template<typename F>
void format_object(unsigned int object, format_options fo, F &formatter) {
	_fmt_basics::format_integer(object, fo, formatter);
}

template<typename F>
void format_object(unsigned long object, format_options fo, F &formatter) {
	_fmt_basics::format_integer(object, fo, formatter);
}

template<typename F>
void format_object(int object, format_options fo, F &formatter) {
	_fmt_basics::format_integer(object, fo, formatter);
}

template<typename F>
void format_object(long object, format_options fo, F &formatter) {
	_fmt_basics::format_integer(object, fo, formatter);
}

template<typename F>
void format_object(const char *object, format_options fo, F &formatter) {
	formatter.append(object);
}

template<typename F>
void format_object(const void *object, format_options fo, F &formatter) {
	formatter.append("0x");
	_fmt_basics::format_integer(reinterpret_cast<uintptr_t>(object),
			format_options{}.with_conversion(format_conversion::hex), formatter);
}

// ----------------------------------------------------------------------------

struct va_struct {
	va_list args;
};

enum class printf_size_mod {
	default_size,
	long_size,
	longlong_size,
	native_size
};

template<typename A>
void printf_format(A agent, const char *s, va_struct *vsp) {
	while(*s) {
		if(*s != '%') {
			size_t n = 1;
			while(s[n] && s[n] != '%')
				n++;
			agent(s, n);
			s += n;
			continue;
		}

		++s;
		FRG_ASSERT(*s);

		if(*s == '%') {
			agent('%');
			++s;
			continue;
		}

		format_options opts;
		while(true) {
			if(*s == '-') {
				opts.left_justify = true;
				++s;
				FRG_ASSERT(*s);
			}else if(*s == '+') {
				opts.always_sign = true;
				++s;
				FRG_ASSERT(*s);
			}else if(*s == ' ') {
				opts.plus_becomes_space = true;
				++s;
				FRG_ASSERT(*s);
			}else if(*s == '#') {
				opts.alt_conversion = true;
				++s;
				FRG_ASSERT(*s);
			}else if(*s == '0') {
				opts.fill_zeros = true;
				++s;
				FRG_ASSERT(*s);
			}else{
				break;
			}
		}

		FRG_ASSERT(!opts.always_sign);
		FRG_ASSERT(!opts.plus_becomes_space);

		if(*s == '*') {
			++s;
			FRG_ASSERT(*s);
			opts.minimum_width = va_arg(vsp->args, int);
		}else{
			int w = 0;
			while(*s >= '0' && *s <= '9') {
				w = w * 10 + (*s - '0');
				++s;
				FRG_ASSERT(*s);
			}
			opts.minimum_width = w;
		}

		if(*s == '.') {
			++s;
			FRG_ASSERT(*s);

			if(*s == '*') {
				++s;
				FRG_ASSERT(*s);
				opts.precision = va_arg(vsp->args, int);
			}else{
				int value = 0;
				FRG_ASSERT(*s >= '0' && *s <= '9');
				while(*s >= '0' && *s <= '9') {
					value = value * 10 + (*s - '0');
					++s;
					FRG_ASSERT(*s);
				}
				opts.precision = value;
			}
		}
		
		auto szmod = printf_size_mod::default_size;
		if(*s == 'l') {
			++s;
			FRG_ASSERT(*s);
			if(*s == 'l') {
				szmod = printf_size_mod::longlong_size;
				++s;
				FRG_ASSERT(*s);
			}else{
				szmod = printf_size_mod::long_size;
			}
		}else if(*s == 'z') {
			szmod = printf_size_mod::native_size;
			++s;
			FRG_ASSERT(*s);
		}

		agent(*s, opts, szmod);
		++s;
	}
}

template<typename F>
void do_printf_chars(F &formatter, char t, format_options opts,
		printf_size_mod szmod, va_struct *vsp) {
	switch(t) {
	case 'p':
		FRG_ASSERT(!opts.fill_zeros);
		FRG_ASSERT(!opts.left_justify);
		FRG_ASSERT(!opts.alt_conversion);
		FRG_ASSERT(opts.minimum_width == 0);
		formatter.append("0x");
		_fmt_basics::print_int(formatter, (uintptr_t)va_arg(vsp->args, void *), 16);
		break;
	case 'c':
		FRG_ASSERT(!opts.fill_zeros);
		FRG_ASSERT(!opts.left_justify);
		FRG_ASSERT(!opts.alt_conversion);
		FRG_ASSERT(szmod == printf_size_mod::default_size);
		FRG_ASSERT(!opts.precision);
		for (int i = 0; i < opts.minimum_width - 1; i++)
			formatter.append(' ');
		formatter.append((char)va_arg(vsp->args, int));
		break;
	case 's': {
		FRG_ASSERT(!opts.fill_zeros);
		FRG_ASSERT(!opts.alt_conversion);

		if(szmod == printf_size_mod::default_size) {
			auto s = va_arg(vsp->args, const char *);
			if(!s)
				s = "(null)";

			int length = string_view{s}.size();
			if(opts.precision && *opts.precision < length)
				length = *opts.precision;

			if(opts.left_justify) {
				for(int i = 0; i < length && s[i]; i++)
					formatter.append(s[i]);
				for(int i = length; i < opts.minimum_width; i++)
					formatter.append(' ');
			}else{
				for(int i = length; i < opts.minimum_width; i++)
					formatter.append(' ');
				for(int i = 0; i < length && s[i]; i++)
					formatter.append(s[i]);
			}
		}else{
			FRG_ASSERT(szmod == printf_size_mod::long_size);
			auto s = va_arg(vsp->args, const wchar_t *);
			if(!s)
				s = L"(null)";

			int length = basic_string_view<wchar_t>{s}.size();
			if(opts.precision && *opts.precision < length)
				length = *opts.precision;

			if(opts.left_justify) {
				for(int i = 0; i < length && s[i]; i++)
					formatter.append(s[i]);
				for(int i = length; i < opts.minimum_width; i++)
					formatter.append(' ');
			}else{
				for(int i = length; i < opts.minimum_width; i++)
					formatter.append(' ');
				for(int i = 0; i < length && s[i]; i++)
					formatter.append(s[i]);
			}
		}
	} break;
	default:
		FRG_ASSERT(!"Unexpected printf terminal");
	}
}

template<typename F>
void do_printf_ints(F &formatter, char t, format_options opts,
		printf_size_mod szmod, va_struct *vsp) {
	switch(t) {
	case 'd':
	case 'i': {
		FRG_ASSERT(!opts.left_justify);
		FRG_ASSERT(!opts.alt_conversion);
		long number;
		if(szmod == printf_size_mod::long_size) {
			number = va_arg(vsp->args, long);
		}else if(szmod == printf_size_mod::longlong_size) {
			number = va_arg(vsp->args, long long);
		}else if(szmod == printf_size_mod::native_size) {
			number = va_arg(vsp->args, intptr_t);
		}else{
			FRG_ASSERT(szmod == printf_size_mod::default_size);
			number = va_arg(vsp->args, int);
		}
		if(opts.precision && *opts.precision == 0 && !number) {
			// print nothing in this case
		}else{
			_fmt_basics::print_int(formatter, number, 10, opts.minimum_width,
					opts.precision ? *opts.precision : 1, opts.fill_zeros ? '0' : ' ');
		}
	} break;
	case 'o': {
		// TODO: Implement this correctly
		FRG_ASSERT(!opts.left_justify);

		auto print = [&] (auto number) {
			if(opts.precision && *opts.precision == 0 && !number) {
				// print nothing in this case
			}else{
				_fmt_basics::print_int(formatter, number, 8, opts.minimum_width,
						opts.precision ? *opts.precision : 1, opts.fill_zeros ? '0' : ' ');
			}
		};

		if(opts.alt_conversion)
			formatter.append('0');

		if(szmod == printf_size_mod::long_size) {
			print(va_arg(vsp->args, unsigned long));
		}else{
			FRG_ASSERT(szmod == printf_size_mod::default_size);
			print(va_arg(vsp->args, unsigned int));
		}
	} break;
	case 'x': {
		// TODO: Implement this correctly
		FRG_ASSERT(!opts.left_justify);
		FRG_ASSERT(!opts.alt_conversion);
		auto print = [&] (auto number) {
			if(opts.precision && *opts.precision == 0 && !number) {
				// print nothing in this case
			}else{
				_fmt_basics::print_int(formatter, number, 16, opts.minimum_width,
						opts.precision ? *opts.precision : 1, opts.fill_zeros ? '0' : ' ');
			}
		};
		if(szmod == printf_size_mod::long_size) {
			print(va_arg(vsp->args, unsigned long));
		}else{
			FRG_ASSERT(szmod == printf_size_mod::default_size);
			print(va_arg(vsp->args, unsigned int));
		}
	} break;
	case 'X': {
		FRG_ASSERT(!opts.left_justify);
		FRG_ASSERT(!opts.alt_conversion);
		auto print = [&] (auto number) {
			if(opts.precision && *opts.precision == 0 && !number) {
				// print nothing in this case
			}else{
				_fmt_basics::print_int(formatter, number, 16, opts.minimum_width,
						opts.precision ? *opts.precision : 1, opts.fill_zeros ? '0' : ' ');
			}
		};
		if(szmod == printf_size_mod::long_size) {
			print(va_arg(vsp->args, unsigned long));
		}else{
			print(va_arg(vsp->args, unsigned int));
		}
	} break;
	case 'u': {
		FRG_ASSERT(!opts.left_justify);
		FRG_ASSERT(!opts.alt_conversion);
		FRG_ASSERT(!opts.precision);
		if(szmod == printf_size_mod::longlong_size) {
			_fmt_basics::print_int(formatter, va_arg(vsp->args, unsigned long long),
					10, opts.minimum_width,
					1, opts.fill_zeros ? '0' : ' ');
		}else if(szmod == printf_size_mod::long_size) {
			_fmt_basics::print_int(formatter, va_arg(vsp->args, unsigned long),
					10, opts.minimum_width,
					1, opts.fill_zeros ? '0' : ' ');
		}else if(szmod == printf_size_mod::native_size) {
			_fmt_basics::print_int(formatter, va_arg(vsp->args, size_t),
					10, opts.minimum_width,
					1, opts.fill_zeros ? '0' : ' ');
		}else{
			FRG_ASSERT(szmod == printf_size_mod::default_size);
			_fmt_basics::print_int(formatter, va_arg(vsp->args, unsigned int),
					10, opts.minimum_width,
					1, opts.fill_zeros ? '0' : ' ');
		}
	} break;
	default:
		FRG_ASSERT(!"Unexpected printf terminal");
	}
}

template<typename F>
void do_printf_floats(F &formatter, char t, format_options opts,
		printf_size_mod szmod, va_struct *vsp) {
	switch(t) {
	case 'f':
	case 'F':
	case 'g':
	case 'G':
	case 'e':
	case 'E':
		formatter.append("%f");
		break;
	default:
		FRG_ASSERT(!"Unexpected printf terminal");
	}
}

// ----------------------------------------------------------------------------

template<typename T>
struct hex_fmt {
	template<typename F>
	friend void format_object(hex_fmt self, format_options fo, F &formatter) {
		format(*self._xp, fo.with_conversion(format_conversion::hex), formatter);
	}

	hex_fmt(const T &x)
	: _xp{&x} { }

private:
	const T *_xp;
};

struct escape_fmt {
	template<typename F>
	friend void format_object(escape_fmt self, format_options fo, F &formatter) {
		auto p = reinterpret_cast<const unsigned char *>(self._buffer);
		for(size_t i = 0; i < self._size; i++) {
			auto c = p[i];
			if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
				formatter.append(c);
			}else if(c >= '0' && c <= '9') {
				formatter.append(c);
			}else if(c == ' ') {
				formatter.append(' ');
			}else if(strchr("!#$%&()*+,-./:;<=>?@[]^_`{|}~", c)) {
				formatter.append(c);
			}else if(c == '\\') {
				formatter.append("\\\\");
			}else if(c == '\"') {
				formatter.append("\\\"");
			}else if(c == '\'') {
				formatter.append("\\\'");
			}else if(c == '\n') {
				formatter.append("\\n");
			}else if(c == '\t') {
				formatter.append("\\t");
			}else{
				formatter.append("\\x{");
				format((unsigned int)c, fo.with_conversion(format_conversion::hex), formatter);
				formatter.append('}');
			}
		}
	}

	escape_fmt(const void *buffer, size_t size)
	: _buffer{buffer}, _size{size} { }

private:
	const void *_buffer;
	size_t _size;
};

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Formatting entry points.
// ----------------------------------------------------------------------------

// Internally calls format_object() with default options to format an object.
// format_object() is an ADL customization point of this formatting library.
template<typename T, typename F>
void format(const T &object, F &formatter) {
	format_object(object, format_options{}, formatter);
}

template<typename T, typename F>
void format(const T &object, format_options fo, F &formatter) {
	format_object(object, fo, formatter);
}

} // namespace frg

#endif // FRG_FORMATTING_HPP
