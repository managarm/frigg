#ifndef FRG_CMDLINE_HPP
#define FRG_CMDLINE_HPP

#include <frg/macros.hpp>
#include <frg/string.hpp>
#include <frg/tuple.hpp>

#include <concepts>

namespace frg FRG_VISIBILITY {

struct option {
	struct fn_type {
		void (*ptr)(frg::string_view, void *);
		void *ctx;
		bool has_arg;
	};

	frg::string_view opt;
	fn_type fn;

	void apply(frg::string_view value) {
		FRG_ASSERT(fn.ptr);
		fn.ptr(value, fn.ctx);
	}
};

template <typename T>
auto as_number(T &here) {
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

inline auto as_string_view(frg::string_view &here) {
	return option::fn_type{
		[] (frg::string_view value, void *ctx) {
			*static_cast<frg::string_view *>(ctx) = value;
		},
		&here,
		true
	};
}

inline auto store_true(bool &here) {
	return option::fn_type{
		[] (frg::string_view value, void *ctx) {
			*static_cast<bool *>(ctx) = true;
		},
		&here,
		false
	};
}

template <typename T>
concept option_span = requires (T t) {
	{ t.data() } -> std::convertible_to<option *>;
	{ t.size() } -> std::convertible_to<size_t>;
};


template <option_span ...Ts>
inline void parse_arguments(frg::string_view cmdline, Ts &&...args) {
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
	
	auto try_arg_span = [&] (frg::string_view arg, auto span) -> bool {
		for (size_t i = 0; i < span.size(); i++) {
			if (try_apply_arg(arg, span.data()[i]))
				return true;
		}
		
		return false;
	};
	
	while (true) {
		size_t spc = cmdline.find_first(' ');
		size_t split_on = spc;
		if (spc == size_t(-1))
			split_on = cmdline.size();

		auto arg = cmdline.sub_string(0, split_on);

		(try_arg_span(arg, args) || ...);

		if (spc != size_t(-1)) {
			cmdline = cmdline.sub_string(spc + 1, cmdline.size() - spc - 1);
		} else {
			break;
		}
	}
}

} // namespace frg

#endif // FRG_CMDLINE_HPP
