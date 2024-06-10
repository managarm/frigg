#pragma once

#include <frg/macros.hpp>
#include <frg/expected.hpp>
#include <frg/formatting.hpp>

namespace frg FRG_VISIBILITY {

union arg {
	uintmax_t i;
#ifdef FRG_DONT_USE_LONG_DOUBLE
	double f;
#else
	long double f;
#endif
	void *p;
};

struct va_struct {
	va_list args;
	arg *arg_list;
	int num_args = 0;
};

enum class printf_size_mod {
	default_size,
	char_size,
	short_size,
	long_size,
	longlong_size,
#ifndef FRG_DONT_USE_LONG_DOUBLE
	longdouble_size,
#endif
	native_size,
	intmax_size
};

template<typename T>
T pop_arg(va_struct *vsp, format_options *opts) {
	auto pop_va_arg = [&] {
		if constexpr (std::is_same_v<T, unsigned char> ||
				std::is_same_v<T, char> ||
				std::is_same_v<T, signed char> ||
				std::is_same_v<T, unsigned short> ||
				std::is_same_v<T, short> ||
				std::is_same_v<T, signed short>) {
			return static_cast<T>(va_arg(vsp->args, int));
		} else {
			return va_arg(vsp->args, T);
		}
	};

	auto get_union_member = [&] (int pos) {
		if constexpr (std::is_same_v<T, void*>)
			return &vsp->arg_list[pos].p;
		else if constexpr (std::is_floating_point_v<T>)
			return reinterpret_cast<T*>(&vsp->arg_list[pos].f);
		else
			return reinterpret_cast<T*>(&vsp->arg_list[pos].i);
	};

	if (opts->arg_pos == -1)
		return pop_va_arg();

	FRG_ASSERT(opts->arg_pos <= vsp->num_args);
	if (opts->arg_pos < vsp->num_args)
		return *get_union_member(opts->arg_pos);

	auto arg = pop_va_arg();
	*get_union_member(vsp->num_args++) = arg;
	return arg;
}

template<typename A>
frg::expected<format_error> printf_format(A agent, const char *s, va_struct *vsp) {
	FRG_ASSERT(s != nullptr);

	while(*s) {
		if(*s != '%') {
			size_t n = 1;
			while(s[n] && s[n] != '%')
				n++;
			auto res = agent(s, n);
			if (!res)
				return res;
			s += n;
			continue;
		}

		++s;
		FRG_ASSERT(*s);

		if(*s == '%') {
			auto res = agent('%');
			if (!res)
				return res;
			++s;
			continue;
		}


		format_options opts;
		while(true) {
			if (*s >= '0' && *s <= '9' && s[1] && s[1] == '$') {
				opts.arg_pos = *s - '0' - 1; // args are 1-indexed
				s += 2;
				FRG_ASSERT(*s);
			} else if(*s == '-') {
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
			}else if(*s == '\'') {
				opts.group_thousands = true;
				++s;
				FRG_ASSERT(*s);
			}else{
				break;
			}
		}

		if(*s == '*') {
			++s;
			FRG_ASSERT(*s);
			opts.minimum_width = pop_arg<int>(vsp, &opts);
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
				opts.precision = pop_arg<int>(vsp, &opts);
			}else{
				int value = 0;
				// If no integer follows the '.', then precision is taken to be zero
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
#ifndef FRG_DONT_USE_LONG_DOUBLE
		} else if(*s == 'L') {
			szmod = printf_size_mod::longdouble_size;
			++s;
			FRG_ASSERT(*s);
#endif
		} else if(*s == 'h') {
			++s;
			FRG_ASSERT(*s);
			if(*s == 'h') {
				szmod = printf_size_mod::char_size;
				++s;
				FRG_ASSERT(*s);
			}else{
				szmod = printf_size_mod::short_size;
			}
		} else if(*s == 't') {
			szmod = printf_size_mod::native_size;
			++s;
			FRG_ASSERT(*s);
		} else if(*s == 'j') {
			szmod = printf_size_mod::intmax_size;
			++s;
			FRG_ASSERT(*s);
		}

		auto res = agent(*s, opts, szmod);
		if(!res)
			return res;

		++s;
	}

	return {};
}

template<Sink S>
void do_printf_chars(S &sink, char t, format_options opts,
		printf_size_mod szmod, va_struct *vsp) {
	switch(t) {
	case 'p':
		FRG_ASSERT(!opts.fill_zeros);
		FRG_ASSERT(!opts.left_justify);
		FRG_ASSERT(!opts.alt_conversion);
		FRG_ASSERT(opts.minimum_width == 0);
		sink.append("0x");
		_fmt_basics::print_int(sink, (uintptr_t)pop_arg<void*>(vsp, &opts), 16);
		break;
	case 'c':
		FRG_ASSERT(!opts.fill_zeros);
		FRG_ASSERT(!opts.alt_conversion);
		FRG_ASSERT(szmod == printf_size_mod::default_size);
		FRG_ASSERT(!opts.precision);
		if (opts.left_justify) {
			sink.append(pop_arg<char>(vsp, &opts));
			for (int i = 0; i < opts.minimum_width - 1; i++)
				sink.append(' ');
		} else {
			for (int i = 0; i < opts.minimum_width - 1; i++)
				sink.append(' ');
			sink.append(pop_arg<char>(vsp, &opts));
		}
		break;
	case 's': {
		FRG_ASSERT(!opts.fill_zeros);
		FRG_ASSERT(!opts.alt_conversion);

		if(szmod == printf_size_mod::default_size) {
			auto s = (const char *)pop_arg<void*>(vsp, &opts);
			if(!s)
				s = "(null)";

			int length;
			if(opts.precision)
				length = generic_strnlen(s, *opts.precision);
			else
				length = generic_strlen(s);

			if(opts.left_justify) {
				for(int i = 0; i < length && s[i]; i++)
					sink.append(s[i]);
				for(int i = length; i < opts.minimum_width; i++)
					sink.append(' ');
			}else{
				for(int i = length; i < opts.minimum_width; i++)
					sink.append(' ');
				for(int i = 0; i < length && s[i]; i++)
					sink.append(s[i]);
			}
		}else{
			FRG_ASSERT(szmod == printf_size_mod::long_size);
			auto s = (const wchar_t *)pop_arg<void*>(vsp, &opts);
			if(!s)
				s = L"(null)";

			int length;
			if(opts.precision)
				length = generic_strnlen(s, *opts.precision);
			else
				length = generic_strlen(s);

			if(opts.left_justify) {
				for(int i = 0; i < length && s[i]; i++)
					sink.append(s[i]);
				for(int i = length; i < opts.minimum_width; i++)
					sink.append(' ');
			}else{
				for(int i = length; i < opts.minimum_width; i++)
					sink.append(' ');
				for(int i = 0; i < length && s[i]; i++)
					sink.append(s[i]);
			}
		}
	} break;
	default:
		FRG_ASSERT(!"Unexpected printf terminal");
	}
}

template<Sink S>
void do_printf_ints(S &sink, char t, format_options opts,
		printf_size_mod szmod, va_struct *vsp, locale_options locale_opts = {}) {
	switch(t) {
	case 'd':
	case 'i': {
		FRG_ASSERT(!opts.alt_conversion);
		long number;
		if(szmod == printf_size_mod::char_size) {
			number = pop_arg<signed char>(vsp, &opts);
		}else if(szmod == printf_size_mod::short_size) {
			number = pop_arg<short>(vsp, &opts);
		}else if(szmod == printf_size_mod::long_size) {
			number = pop_arg<long>(vsp, &opts);
		}else if(szmod == printf_size_mod::longlong_size) {
			number = pop_arg<long long>(vsp, &opts);
		}else if(szmod == printf_size_mod::native_size) {
			number = pop_arg<intptr_t>(vsp, &opts);
		}else if(szmod == printf_size_mod::intmax_size) {
			number = pop_arg<intmax_t>(vsp, &opts);
		}else{
			FRG_ASSERT(szmod == printf_size_mod::default_size);
			number = pop_arg<int>(vsp, &opts);
		}
		if(opts.precision && *opts.precision == 0 && !number) {
			// print nothing in this case
		}else{
			_fmt_basics::print_int(sink, number, 10, opts.minimum_width,
					opts.precision ? *opts.precision : 1, opts.fill_zeros ? '0' : ' ',
					opts.left_justify, opts.group_thousands, opts.always_sign,
					opts.plus_becomes_space, false, locale_opts);
		}
	} break;
	case 'o': {
		auto print = [&] (auto number) {
			if (number && opts.alt_conversion)
				sink.append('0');

			if(opts.precision && *opts.precision == 0 && !number) {
				// print nothing in this case
			}else{
				_fmt_basics::print_int(sink, number, 8, opts.minimum_width,
						opts.precision ? *opts.precision : 1, opts.fill_zeros ? '0' : ' ',
						opts.left_justify, false, opts.always_sign, opts.plus_becomes_space,
						false, locale_opts);
			}
		};

		if(szmod == printf_size_mod::char_size) {
			print(pop_arg<unsigned char>(vsp, &opts));
		}else if(szmod == printf_size_mod::short_size) {
			print(pop_arg<unsigned short>(vsp, &opts));
		}else if(szmod == printf_size_mod::long_size) {
			print(pop_arg<unsigned long>(vsp, &opts));
		}else if(szmod == printf_size_mod::longlong_size) {
			print(pop_arg<unsigned long long>(vsp, &opts));
		}else if(szmod == printf_size_mod::native_size) {
			print(pop_arg<size_t>(vsp, &opts));
		}else if(szmod == printf_size_mod::intmax_size) {
			print(pop_arg<uintmax_t>(vsp, &opts));
		}else{
			FRG_ASSERT(szmod == printf_size_mod::default_size);
			print(pop_arg<unsigned int>(vsp, &opts));
		}
	} break;
	case 'x': {
		auto print = [&] (auto number) {
			if (number && opts.alt_conversion)
				sink.append("0x");

			if(opts.precision && *opts.precision == 0 && !number) {
				// print nothing in this case
			}else{
				_fmt_basics::print_int(sink, number, 16, opts.minimum_width,
						opts.precision ? *opts.precision : 1, opts.fill_zeros ? '0' : ' ',
						opts.left_justify, false, opts.always_sign, opts.plus_becomes_space,
						false, locale_opts);
			}
		};

		if(szmod == printf_size_mod::char_size) {
			print(pop_arg<unsigned char>(vsp, &opts));
		}else if(szmod == printf_size_mod::short_size) {
			print(pop_arg<unsigned short>(vsp, &opts));
		}else if(szmod == printf_size_mod::long_size) {
			print(pop_arg<unsigned long>(vsp, &opts));
		}else if(szmod == printf_size_mod::longlong_size) {
			print(pop_arg<unsigned long long>(vsp, &opts));
		}else if(szmod == printf_size_mod::native_size) {
			print(pop_arg<size_t>(vsp, &opts));
		}else if(szmod == printf_size_mod::intmax_size) {
			print(pop_arg<uintmax_t>(vsp, &opts));
		}else{
			FRG_ASSERT(szmod == printf_size_mod::default_size);
			print(pop_arg<unsigned int>(vsp, &opts));
		}
	} break;
	case 'X': {
		auto print = [&] (auto number) {
			if (number && opts.alt_conversion)
				sink.append("0X");

			if(opts.precision && *opts.precision == 0 && !number) {
				// print nothing in this case
			}else{
				_fmt_basics::print_int(sink, number, 16, opts.minimum_width,
						opts.precision ? *opts.precision : 1, opts.fill_zeros ? '0' : ' ',
						opts.left_justify, false, opts.always_sign, opts.plus_becomes_space,
						true, locale_opts);
			}
		};

		if(szmod == printf_size_mod::char_size) {
			print(pop_arg<unsigned char>(vsp, &opts));
		}else if(szmod == printf_size_mod::short_size) {
			print(pop_arg<unsigned short>(vsp, &opts));
		}else if(szmod == printf_size_mod::long_size) {
			print(pop_arg<unsigned long>(vsp, &opts));
		}else if(szmod == printf_size_mod::longlong_size) {
			print(pop_arg<unsigned long long>(vsp, &opts));
		}else if(szmod == printf_size_mod::native_size) {
			print(pop_arg<size_t>(vsp, &opts));
		}else if(szmod == printf_size_mod::intmax_size) {
			print(pop_arg<uintmax_t>(vsp, &opts));
		}else{
			FRG_ASSERT(szmod == printf_size_mod::default_size);
			print(pop_arg<unsigned int>(vsp, &opts));
		}
	} break;
	case 'u': {
		auto print = [&] (auto number) {
			FRG_ASSERT(!opts.alt_conversion);
			if(opts.precision && *opts.precision == 0 && !number) {
				// print nothing in this case
			}else{
				_fmt_basics::print_int(sink, number, 10, opts.minimum_width,
						opts.precision ? *opts.precision : 1, opts.fill_zeros ? '0' : ' ',
						opts.left_justify, opts.group_thousands, opts.always_sign,
						opts.plus_becomes_space, false, locale_opts);
			}
		};

		if(szmod == printf_size_mod::char_size) {
			print(pop_arg<unsigned char>(vsp, &opts));
		}else if(szmod == printf_size_mod::short_size) {
			print(pop_arg<unsigned short>(vsp, &opts));
		}else if(szmod == printf_size_mod::long_size) {
			print(pop_arg<unsigned long>(vsp, &opts));
		}else if(szmod == printf_size_mod::longlong_size) {
			print(pop_arg<unsigned long long>(vsp, &opts));
		}else if(szmod == printf_size_mod::native_size) {
			print(pop_arg<size_t>(vsp, &opts));
		}else if(szmod == printf_size_mod::intmax_size) {
			print(pop_arg<uintmax_t>(vsp, &opts));
		}else{
			FRG_ASSERT(szmod == printf_size_mod::default_size);
			print(pop_arg<unsigned int>(vsp, &opts));
		}
	} break;
	default:
		FRG_ASSERT(!"Unexpected printf terminal");
	}
}

template<Sink S>
void do_printf_floats(S &sink, char t, format_options opts,
		printf_size_mod szmod, va_struct *vsp, locale_options locale_opts = {}) {
	int precision_or_default = opts.precision.has_value() ? *opts.precision : 6;
	bool use_capitals = true;
	bool drop_trailing_decimal_zeroes = false;
	switch(t) {
	case 'f':
		use_capitals = false;
		[[fallthrough]];
	case 'F':
#ifndef FRG_DONT_USE_LONG_DOUBLE
		if (szmod == printf_size_mod::longdouble_size) {
			_fmt_basics::print_float(sink, pop_arg<long double>(vsp, &opts),
					opts.minimum_width, precision_or_default,
					opts.fill_zeros ? '0' : ' ', opts.left_justify, use_capitals,
					opts.group_thousands, drop_trailing_decimal_zeroes, locale_opts);
			break;
		}
#endif
		FRG_ASSERT(szmod == printf_size_mod::default_size || szmod == printf_size_mod::long_size);
		_fmt_basics::print_float(sink, pop_arg<double>(vsp, &opts),
				opts.minimum_width, precision_or_default,
				opts.fill_zeros ? '0' : ' ', opts.left_justify, use_capitals,
				opts.group_thousands, drop_trailing_decimal_zeroes, locale_opts);
		break;
	case 'g':
		use_capitals = false;
		drop_trailing_decimal_zeroes = true;
		[[fallthrough]];
	case 'G':
#ifndef FRG_DONT_USE_LONG_DOUBLE
		if (szmod == printf_size_mod::longdouble_size) {
			_fmt_basics::print_float(sink, pop_arg<long double>(vsp, &opts),
					opts.minimum_width, precision_or_default,
					opts.fill_zeros ? '0' : ' ', opts.left_justify, use_capitals,
					opts.group_thousands, drop_trailing_decimal_zeroes, locale_opts);
			break;
		}
#endif
		FRG_ASSERT(szmod == printf_size_mod::default_size || szmod == printf_size_mod::long_size);
		_fmt_basics::print_float(sink, pop_arg<double>(vsp, &opts),
				opts.minimum_width, precision_or_default,
				opts.fill_zeros ? '0' : ' ', opts.left_justify, use_capitals,
				opts.group_thousands, drop_trailing_decimal_zeroes, locale_opts);
		break;
	case 'e':
	case 'E':
		sink.append("%");
		sink.append(t);
		break;
	default:
		FRG_ASSERT(!"Unexpected printf terminal");
	}
}

} // namespace frg
