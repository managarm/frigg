#ifndef FRG_FORMATTING_HPP
#define FRG_FORMATTING_HPP

#include <stdarg.h>
#include <cstddef>
#include <stdint.h>
#include <frg/macros.hpp>
#include <frg/optional.hpp>
#include <frg/string.hpp>
#include <frg/utility.hpp>
#include <frg/tuple.hpp>
#include <frg/string_stub.hpp>

/* the ranges library is not even partially freestanding for some reason */
#if __STDC_HOSTED__
#  if __has_include(<ranges>) && __has_include(<algorithm>)
#    include <ranges>
#    include <algorithm>
#    define FRG_HAS_RANGES
#  endif
#endif

namespace frg FRG_VISIBILITY {

// Concept: Sink.
// Supports a single operation: append().
// append() is overloaded for a variety of types.

template<typename T>
concept Sink = requires (T t, const char *str, char c) {
	t.append(str);
	t.append(c);
};

// ----------------------------------------------------------------------------
// General formatting machinery.
// ----------------------------------------------------------------------------

enum class format_conversion {
	null,
	character,
	binary,
	octal,
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
	int arg_pos = -1;
	optional<int> precision;
	bool left_justify = false;
	bool always_sign = false;
	bool plus_becomes_space = false;
	bool alt_conversion = false;
	bool fill_zeros = false;
	bool group_thousands = false;
	bool use_capitals = false;
};

struct locale_options {
	locale_options()
	: decimal_point("."), thousands_sep(""), grouping("\255") { }

	locale_options(const char *d_p, const char *t_s, const char *grp)
	: decimal_point(d_p), thousands_sep(t_s), grouping(grp) {
		thousands_sep_size = generic_strlen(thousands_sep);
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
	template<Sink S, typename T>
	void print_digits(S &sink, T number, bool negative, int radix,
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
				sink.append(locale_opts.thousands_sep);
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
				sink.append(padding);

		if(negative)
			sink.append('-');
		else if(always_sign)
			sink.append('+');
		else if(plus_becomes_space)
			sink.append(' ');

		if(k < precision) {
			for(int i = 0; i < precision - k; i++) {
				sink.append('0');
				emit_grouping();
			}
		}

		for(int i = k - 1; i >= 0; i--) {
			sink.append(buffer[i]);
			emit_grouping();
		}

		if(left_justify && final_width < width)
			for(int i = final_width; i < width; i++)
				sink.append(padding);
	}

	// Signed integer formatting. We cannot print -x as that might not fit into the signed type.
	// Strategy: Cast to unsigned first (i.e. obtain 2s complement) and negate manually by
	// computing (~x + 1).
	template<Sink S, typename T>
	void print_int(S &sink, T number, int radix, int width = 0,
			int precision = 1, char padding = ' ', bool left_justify = false,
			bool group_thousands = false, bool always_sign = false,
			bool plus_becomes_space = false, bool use_capitals = false,
			locale_options locale_opts = {}) {
		if(number < 0) {
			auto absv = ~static_cast<typename std::make_unsigned_t<T>>(number) + 1;
			print_digits(sink, absv, true, radix, width, precision, padding,
					left_justify, group_thousands, always_sign, plus_becomes_space, use_capitals,
					locale_opts);
		}else{
			print_digits(sink, number, false, radix, width, precision, padding,
					left_justify, group_thousands, always_sign, plus_becomes_space, use_capitals,
					locale_opts);
		}
	}

	template<typename T, Sink S>
	void format_integer(T object, format_options fo, S &sink) {
		int radix = 10;
		if(fo.conversion == format_conversion::hex) {
			radix = 16;
		}else if(fo.conversion == format_conversion::octal) {
			radix = 8;
		}else if(fo.conversion == format_conversion::binary) {
			radix = 2;
		}else{
			FRG_ASSERT(fo.conversion == format_conversion::null
					|| fo.conversion == format_conversion::decimal);
		}

		print_int(sink, object, radix,
				fo.minimum_width, fo.precision ? *fo.precision : 1,
				fo.fill_zeros ? '0' : ' ', fo.left_justify, fo.group_thousands,
				fo.always_sign, fo.plus_becomes_space, fo.use_capitals);
	}

	template<Sink S, typename T>
	void print_float(S &sink, T number, int width = 0, int precision = 6,
			char padding = ' ', bool left_justify = false, bool use_capitals = false,
			bool group_thousands = false, locale_options locale_opts = {}) {
		(void)group_thousands;

		bool has_sign = false;
		if (number < 0) {
			sink.append('-');
			has_sign = true;
		}

		bool inf = __builtin_isinf(number), nan = __builtin_isnan(number);
		if (inf || nan) {
			auto total_length = 3 + has_sign;
			auto pad_length = width > total_length ? width - total_length : 0;
			if (!left_justify) {
				while (pad_length > 0) {
					sink.append(' '); // for infs and nan's, always pad with spaces
					pad_length--;
				}
			}

			if (inf)
				sink.append(use_capitals ? "INF" : "inf");
			else
				sink.append(use_capitals ? "NAN" : "nan");

			if (left_justify) {
				while (pad_length > 0) {
					sink.append(' '); // for infs and nan's, always pad with spaces
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
				sink.append(padding);
				pad_length--;
			}
		}

		print_int(sink, n, 10);
		number -= n;

		if (precision > 0)
			sink.append(locale_opts.decimal_point);

		// TODO: This doesn't account for rounding properly.
		// e.g 1.2 formatted with %.2f gives 1.19, but it should be 1.20
		number *= 10;
		n = static_cast<uint64_t>(number);
		number -= n;
		int i = 0;
		while (n > 0 && i < precision) {
			sink.append('0' + n);
			number *= 10;
			n = static_cast<uint64_t>(number);
			number -= n;
			i++;
		}

		while (i < precision) {
			sink.append('0');
			i++;
		}

		if (left_justify) {
			while (pad_length > 0) {
				sink.append(padding);
				pad_length--;
			}
		}
	}

	template<typename T, Sink S>
	void format_float(T object, format_options fo, S &sink) {
		int precision_or_default = fo.precision.has_value() ? *fo.precision : 6;
		print_float(sink, object, fo.minimum_width, precision_or_default,
				fo.fill_zeros ? '0' : ' ');
	}
};

template<Sink S>
void format_object(unsigned int object, format_options fo, S &sink) {
	_fmt_basics::format_integer(object, fo, sink);
}

template<Sink S>
void format_object(unsigned long object, format_options fo, S &sink) {
	_fmt_basics::format_integer(object, fo, sink);
}

template<Sink S>
void format_object(unsigned long long object, format_options fo, S &sink) {
	_fmt_basics::format_integer(object, fo, sink);
}

template<Sink S>
void format_object(int object, format_options fo, S &sink) {
	_fmt_basics::format_integer(object, fo, sink);
}

template<Sink S>
void format_object(long object, format_options fo, S &sink) {
	_fmt_basics::format_integer(object, fo, sink);
}

template<Sink S>
void format_object(long long object, format_options fo, S &sink) {
	_fmt_basics::format_integer(object, fo, sink);
}

template<Sink S>
void format_object(float object, format_options fo, S &sink) {
	_fmt_basics::format_integer(object, fo, sink);
}

template<Sink S>
void format_object(double object, format_options fo, S &sink) {
	_fmt_basics::format_integer(object, fo, sink);
}


template<Sink F>
void format_object(char object, format_options fo, F &formatter) {
	if(fo.conversion == format_conversion::character) {
		formatter.append(object);
		return;
	}
	_fmt_basics::format_integer(object, fo, formatter);
}

template<Sink S>
void format_object(const char *object, format_options, S &sink) {
	sink.append(object);
}

template<Sink S>
void format_object(const frg::string_view &object, format_options, S &sink) {
	for(size_t i = 0; i < object.size(); ++i)
		sink.append(object[i]);
}

template<Sink S, typename Allocator>
void format_object(const frg::string<Allocator> &object, format_options, S &sink) {
	sink.append(object.data());
}

template<Sink S>
void format_object(const void *object, format_options fo, S &sink) {
	sink.append("0x");
	_fmt_basics::format_integer(reinterpret_cast<uintptr_t>(object),
			fo.with_conversion(format_conversion::hex), sink);
}

template<Sink S>
void format_object(std::nullptr_t, format_options fo, S &sink) {
	format_object(static_cast<const void *>(nullptr), fo, sink);
}

// ----------------------------------------------------------------------------

struct char_fmt {
	template<Sink S>
	friend void format_object(char_fmt self, format_options fo, S &sink) {
		(void)fo;
		sink.append(self.c_);
	}

	template<typename T>
	char_fmt(const T &x)
	: c_{static_cast<char>(x)} { }

private:
	const char c_;
};

#ifdef FRG_HAS_RANGES
template<std::ranges::input_range R, Sink F>
requires requires {
    typename std::char_traits<std::ranges::range_value_t<R>>;
}
void format_object(const R &range, format_options fo, F &formatter) {
	/* TODO(arsen): figure out what to do about wchar (and if we care...)
	 */
	std::ranges::for_each(range, [&] (const auto &x) {
		format_object(char_fmt { x }, fo, formatter);
	});
}
#endif

template<typename T>
struct hex_fmt {
	template<Sink F>
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
	template<Sink S>
	friend void format_object(escape_fmt self, format_options fo, S &sink) {
		auto p = reinterpret_cast<const unsigned char *>(self._buffer);
		for(size_t i = 0; i < self._size; i++) {
			auto c = p[i];
			if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
				sink.append(c);
			}else if(c >= '0' && c <= '9') {
				sink.append(c);
			}else if(c == ' ') {
				sink.append(' ');
			}else if(strchr("!#$%&()*+,-./:;<=>?@[]^_`{|}~", c)) {
				sink.append(c);
			}else if(c == '\\') {
				sink.append("\\\\");
			}else if(c == '\"') {
				sink.append("\\\"");
			}else if(c == '\'') {
				sink.append("\\\'");
			}else if(c == '\n') {
				sink.append("\\n");
			}else if(c == '\t') {
				sink.append("\\t");
			}else{
				sink.append("\\x{");
				format((unsigned int)c, fo.with_conversion(format_conversion::hex), sink);
				sink.append('}');
			}
		}
	}

	escape_fmt(const void *buffer, size_t size)
	: _buffer{buffer}, _size{size} { }

private:
	const void *_buffer;
	size_t _size;
};

static inline bool isdigit(char c) {
	return c >= '0' && c <= '9';
}

namespace detail_ {
	template <typename ...Ts>
	struct fmt_impl {
		frg::string_view fmt;
		frg::tuple<Ts...> args;

		template <typename F> requires (sizeof...(Ts) > 0)
		bool format_nth(size_t n, format_options fo, F &formatter) const {
			if (n >= sizeof...(Ts))
				return false;

			return ![&]<size_t ...I>(std::index_sequence<I...>) -> bool {
				return ((I == n
					? (format(args.template get<I>(), fo, formatter), false)
					: true) && ...);
			}(std::make_index_sequence<sizeof...(Ts)>{});
		}

		template <typename F>
		bool format_nth(size_t, format_options, F &) const {
			return false;
		}

		// Format specifier syntax:
		// ([0-9]+)?(:0?[0-9]*[bcdioXx]?)?
		bool parse_fmt_spec(frg::string_view spec, size_t &pos, format_options &fo) const {
			enum class modes {
				pos, fill, width, conv
			} mode = modes::pos;
			bool pos_set = false;
			size_t tmp_pos = 0;

			fo.minimum_width = 0;

			for (size_t i = 0; i < spec.size(); i++) {
				char c = spec[i];

				switch (mode) {
					case modes::pos:
						if (isdigit(c)) {
							pos_set = true;
							tmp_pos *= 10;
							tmp_pos += c - '0';
						} else if (c == ':') {
							mode = modes::fill;
						} else {
							return false;
						}

						break;

					case modes::fill:
						if (c == '0')
							fo.fill_zeros = true;

						mode = modes::width;

						[[fallthrough]];

					case modes::width:
						if (isdigit(c)) {
							fo.minimum_width *= 10;
							fo.minimum_width += spec[i] - '0';
						} else {
							switch (spec[i]) {
								case 'b': fo.conversion = format_conversion::binary; break;
								case 'c': fo.conversion = format_conversion::character; break;
								case 'o': fo.conversion = format_conversion::octal; break;
								case 'i':
								case 'd': fo.conversion = format_conversion::decimal; break;
								case 'X': fo.use_capitals = true; [[fallthrough]];
								case 'x': fo.conversion = format_conversion::hex; break;
								default: return false;
							}

							mode = modes::conv;
						}

						break;

					// Anything after the conversion specifier is illegal.
					case modes::conv:
						return false;
				}
			}

			if (pos_set)
				pos = tmp_pos;

			return true;
		}

		template <Sink S>
		friend void format_object(const fmt_impl &self, format_options fo, S &sink) {
			size_t current_arg = 0;
			size_t arg_fmt_start = 0, arg_fmt_end = 0;

			enum class modes {
				str, arg
			} mode = modes::str;

			for (size_t i = 0; i < self.fmt.size(); i++) {
				auto c = self.fmt[i];
				auto next = (i + 1) < self.fmt.size() ? self.fmt[i + 1] : 0;

				switch (mode) {
					case modes::str: {
						if (c == '{' && next != '{') {
							mode = modes::arg;
							arg_fmt_start = i;
						} else {
							if (c == '{')
								i++;
							sink.append(c);
						}

						break;
					}

					case modes::arg:
						if (c == '}') {
							mode = modes::str;
							arg_fmt_end = i;

							format_options fo{};
							size_t pos = current_arg++;

							if (!self.parse_fmt_spec(self.fmt.sub_string(arg_fmt_start + 1,
											arg_fmt_end - arg_fmt_start - 1), pos, fo)) {
								// Failed to parse format specifier, print it as is
								format_object(self.fmt.sub_string(arg_fmt_start,
										arg_fmt_end - arg_fmt_start + 1),
									fo, sink);

								break;
							}

							if (!self.format_nth(pos, fo, sink)) {
								// Failed to print argument (arg index out of bounds), print format specifier instead
								format_object(self.fmt.sub_string(arg_fmt_start,
										arg_fmt_end - arg_fmt_start + 1),
									fo, sink);
							}
						}
						break;
				}
			}

			// Unclosed format specifier, print it as is
			if (mode != modes::str) {
				format_object(self.fmt.sub_string(arg_fmt_start,
						self.fmt.size() - arg_fmt_start),
					fo, sink);
			}
		}
	};
} // namespace detail_

template <typename ...Ts>
auto fmt(frg::string_view fmt, Ts &&...ts) {
	return detail_::fmt_impl<Ts...>{fmt, frg::tuple<Ts...>{std::forward<Ts>(ts)...}};
}

// ----------------------------------------------------------------------------
// Formatting entry points.
// ----------------------------------------------------------------------------

// Internally calls format_object() with default options to format an object.
// format_object() is an ADL customization point of this formatting library.
template<typename T, Sink S>
void format(const T &object, S &sink) {
	format_object(object, format_options{}, sink);
}

template<typename T, Sink S>
void format(const T &object, format_options fo, S &sink) {
	format_object(object, fo, sink);
}

} // namespace frg

#endif // FRG_FORMATTING_HPP
