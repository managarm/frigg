#ifndef FRG_CMDLINE_HPP
#define FRG_CMDLINE_HPP

#include <frg/macros.hpp>
#include <frg/string.hpp>
#include <frg/tuple.hpp>

#include <ranges>

namespace frg FRG_VISIBILITY {

struct option {
	struct fn_type {
		void (*ptr)(frg::string_view, void *);
		void *ctx;
		bool has_arg;
	};

	frg::string_view opt;
	fn_type fn;

	constexpr void apply(frg::string_view value) {
		FRG_ASSERT(fn.ptr);
		fn.ptr(value, fn.ctx);
	}
};

template <typename T>
constexpr auto as_number(T &here) {
	return option::fn_type{
		[] (frg::string_view value, void *ctx) {
			auto n = value.to_number<T>();
			if (n)
				*static_cast<T *>(ctx) = n.value();
		},
		&here,
		true
	};
}

inline constexpr auto as_string_view(frg::string_view &here) {
	return option::fn_type{
		[] (frg::string_view value, void *ctx) {
			*static_cast<frg::string_view *>(ctx) = value;
		},
		&here,
		true
	};
}

template<typename T, T value>
constexpr auto store_const(T &here) {
	return option::fn_type{
		[] (frg::string_view, void *ctx) {
			*static_cast<T *>(ctx) = value;
		},
		&here,
		false
	};
}

inline constexpr auto store_true(bool &here) {
	return store_const<bool, true>(here);
}

inline constexpr auto store_false(bool &here) {
	return store_const<bool, false>(here);
}


inline constexpr void parse_arguments(frg::string_view cmdline, std::ranges::range auto args) {
	auto try_apply_arg = [&] (frg::string_view arg, option opt) -> bool {
		auto eq = arg.find_first('=');

		if (eq == size_t(-1)) {
			if (opt.fn.has_arg)
				return false;

			if (opt.opt != arg)
				return false;

			opt.apply({});
		} else {
			if (!opt.fn.has_arg)
				return false;

			auto name = arg.sub_string(0, eq);
			auto val = arg.sub_string(eq + 1, arg.size() - eq -1);

			if (opt.opt != name)
				return false;

			opt.apply(val);
		}

		return true;
	};

	while (true) {
		size_t spc = cmdline.find_first(' ');
		size_t opening_quote = cmdline.find_first('\"');
		size_t closing_quote = size_t(-1);
		bool quoted = false;

		if(opening_quote < spc) {
			quoted = true;

			closing_quote = opening_quote + 1 + cmdline.sub_string(opening_quote + 1, cmdline.size() - opening_quote - 1).find_first('\"');
			spc = closing_quote + 1 + cmdline.sub_string(closing_quote + 1, cmdline.size() - closing_quote - 1).find_first(' ');
		}

		size_t split_on = spc;
		if (spc == size_t(-1))
			split_on = cmdline.size();

		auto arg = cmdline.sub_string(quoted ? (opening_quote + 1) : 0, quoted ? (closing_quote - opening_quote - 1) : split_on);

		for (const option &opt : args) {
			if (try_apply_arg(arg, opt))
				break;
		}

		if (spc != size_t(-1)) {
			cmdline = cmdline.sub_string(spc + 1, cmdline.size() - spc - 1);
		} else {
			break;
		}
	}
}

} // namespace frg

#endif // FRG_CMDLINE_HPP
