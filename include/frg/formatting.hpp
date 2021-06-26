#ifndef FRG_FORMATTING_HPP
#define FRG_FORMATTING_HPP

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
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
	bool group_thousands = false;
};

struct locale_options {
	locale_options()
	: decimal_point("."), thousands_sep(""), grouping("\255") { }

	locale_options(const char *d_p, const char *t_s, const char *grp)
	: decimal_point(d_p), thousands_sep(t_s), grouping(grp) {
		thousands_sep_size = strlen(thousands_sep);
	}

	const char *decimal_point;
	const char *thousands_sep;
	const char *grouping;
	size_t thousands_sep_size;
};

enum class format_error {
	success,
	agent_error,
};

// ----------------------------------------------------------------------------
// Formatting primitives for built-in types.
// ----------------------------------------------------------------------------

namespace _fmt_basics {
	// width: Minimum width of the output (padded with spaces by default).
	// precision: Minimum number of digits in the ouput (always padded with zeros).
	template<typename P, typename T>
	void print_digits(P &formatter, T number, bool negative, int radix,
			int width, int precision, char padding, bool left_justify,
			bool group_thousands, bool always_sign, bool plus_becomes_space,
			bool use_capitals, locale_options locale_opts) {
		const char *digits = use_capitals ? "0123456789ABCDEF" : "0123456789abcdef";
		char buffer[32];

		int k = 0; // number of digits
		int c = 0; // number of chars since last grouping
		int g = 0; // grouping index
		int r = 0; // amount of times we repeated the last grouping
		size_t extra = 0; // extra chars printed due to seperator

		auto step_grouping = [&] () {
			if (!group_thousands)
				return;

			if (++c == locale_opts.grouping[g]) {
				if (locale_opts.grouping[g + 1] > 0)
					g++;
				else
					r++;
				c = 0;
				extra += locale_opts.thousands_sep_size;
			}
		};

		auto emit_grouping = [&] () {
			if (!group_thousands)
				return;

			if (--c == 0) {
				formatter.append(locale_opts.thousands_sep);
				if (!r || !--r)
					g--;
				c = locale_opts.grouping[g];
			}
		};

		// print the number in reverse order and determine #digits.
		do {
			FRG_ASSERT(k < 32); // TODO: variable number of digits
			buffer[k++] = digits[number % radix];
			number /= radix;
			step_grouping();
		} while(number);

		if (k < precision)
			for (int i = 0; i < precision - k; i++)
				step_grouping();

		if (!c)
			c = locale_opts.grouping[g];

		int final_width = max(k, precision) + extra;

		if(!left_justify && final_width < width)
			for(int i = 0; i < width - final_width; i++)
				formatter.append(padding);

		if(negative)
			formatter.append('-');
		else if(always_sign)
			formatter.append('+');
		else if(plus_becomes_space)
			formatter.append(' ');

		if(k < precision) {
			for(int i = 0; i < precision - k; i++) {
				formatter.append('0');
				emit_grouping();
			}
		}

		for(int i = k - 1; i >= 0; i--) {
			formatter.append(buffer[i]);
			emit_grouping();
		}

		if(left_justify && final_width < width)
			for(int i = final_width; i < width; i++)
				formatter.append(padding);
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
			int precision = 1, char padding = ' ', bool left_justify = false,
			bool group_thousands = false, bool always_sign = false,
			bool plus_becomes_space = false, bool use_capitals = false,
			locale_options locale_opts = {}) {
		if(number < 0) {
			auto absv = ~static_cast<typename make_unsigned<T>::type>(number) + 1;
			print_digits(formatter, absv, true, radix, width, precision, padding,
					left_justify, group_thousands, always_sign, plus_becomes_space, use_capitals,
					locale_opts);
		}else{
			print_digits(formatter, number, false, radix, width, precision, padding,
					left_justify, group_thousands, always_sign, plus_becomes_space, use_capitals,
					locale_opts);
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

	template<typename P, typename T>
	void print_float(P &formatter, T number, int width = 0, int precision = 6,
			char padding = ' ', bool left_justify = false, bool use_capitals = false,
			bool group_thousands = false, locale_options locale_opts = {}) {
		(void)group_thousands;
		FRG_DEBUG_ASSERT(padding == ' ');

		bool has_sign = false;
		if (number < 0) {
			formatter.append('-');
			has_sign = true;
		}

		bool inf = __builtin_isinf(number), nan = __builtin_isnan(number);
		if (inf || nan) {
			auto total_length = 3 + has_sign;
			auto pad_length = width > total_length ? width - total_length : 0;
			if (!left_justify) {
				while (pad_length > 0) {
					formatter.append(padding);
					pad_length--;
				}
			}

			if (inf)
				formatter.append(use_capitals ? "INF" : "inf");
			else
				formatter.append(use_capitals ? "NAN" : "nan");

			if (left_justify) {
				while (pad_length > 0) {
					formatter.append(padding);
					pad_length--;
				}
			}

			return;
		}

		// At this point, we've already printed the sign, so pretend it's positive.
		if (number < 0)
			number = -number;

		// TODO: The cast below is UB if number is out of range.
		FRG_ASSERT(number < 0x1p40);
		uint64_t n = static_cast<uint64_t>(number);

		// Compute the number of decimal digits in the integer part of n
		// TODO: Don't assume base 10
		auto int_length = 0;
		auto x = n;
		do {
			x /= 10;
			int_length++;
		} while (x != 0);

		// Plus one for the decimal point
		auto total_length = has_sign + int_length + (precision > 0 ? 1 + precision : 0);
		auto pad_length = width > total_length ? width - total_length : 0;

		if (!left_justify) {
			while (pad_length > 0) {
				formatter.append(padding);
				pad_length--;
			}
		}

		print_int(formatter, n, 10);
		number -= n;

		if (precision > 0)
			formatter.append(locale_opts.decimal_point);

		// TODO: This doesn't account for rounding properly.
		// e.g 1.2 formatted with %.2f gives 1.19, but it should be 1.20
		number *= 10;
		n = static_cast<uint64_t>(number);
		number -= n;
		int i = 0;
		while (n > 0 && i < precision) {
			formatter.append('0' + n);
			number *= 10;
			n = static_cast<uint64_t>(number);
			number -= n;
			i++;
		}

		while (i < precision) {
			formatter.append('0');
			i++;
		}

		if (left_justify) {
			while (pad_length > 0) {
				formatter.append(padding);
				pad_length--;
			}
		}
	}

	template<typename T, typename F>
	void format_float(T object, format_options fo, F &formatter) {
		int precision_or_default = fo.precision.has_value() ? *fo.precision : 6;
		print_float(formatter, object, fo.minimum_width, precision_or_default,
				fo.fill_zeros ? '0' : ' ');
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
void format_object(unsigned long long object, format_options fo, F &formatter) {
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
void format_object(long long object, format_options fo, F &formatter) {
	_fmt_basics::format_integer(object, fo, formatter);
}

template<typename F>
void format_object(float object, format_options fo, F &formatter) {
	_fmt_basics::format_integer(object, fo, formatter);
}

template<typename F>
void format_object(double object, format_options fo, F &formatter) {
	_fmt_basics::format_integer(object, fo, formatter);
}

template<typename F>
void format_object(const char *object, format_options, F &formatter) {
	formatter.append(object);
}

template<typename F>
void format_object(const frg::string_view &object, format_options, F &formatter) {
	for(size_t i = 0; i < object.size(); ++i)
		formatter.append(object[i]);
}

template<typename F, typename Allocator>
void format_object(const frg::string<Allocator> &object, format_options, F &formatter) {
	formatter.append(object.data());
}

template<typename F>
void format_object(const void *object, format_options, F &formatter) {
	formatter.append("0x");
	_fmt_basics::format_integer(reinterpret_cast<uintptr_t>(object),
			format_options{}.with_conversion(format_conversion::hex), formatter);
}

// ----------------------------------------------------------------------------

struct char_fmt {
	template<typename F>
	friend void format_object(char_fmt self, format_options fo, F &formatter) {
		(void)fo;
		formatter.append(self.c_);
	}

	template<typename T>
	char_fmt(const T &x)
	: c_{static_cast<char>(x)} { }

private:
	const char c_;
};

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

static inline char *strchr(const char *s, int c) {
	while (*s) {
		if (*s == c)
			return const_cast<char *>(s);

		s++;
	}

	return nullptr;
}

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
