#ifndef FRG_TUPLE_HPP
#define FRG_TUPLE_HPP

#include <stddef.h>
#include <tuple>
#include <utility>
#include <type_traits>

#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

namespace _tuple {
	template<typename... Types>
	struct storage;

	template<typename T, typename... Types>
	struct storage<T, Types...> {
		storage() = default;

		storage(T item, Types... tail)
		: item(std::move(item)), tail(std::move(tail)...) { }

		template<typename... UTypes>
		storage(const storage<UTypes...> &other)
		: item(other.item), tail(other.tail) { }

		template<typename... UTypes>
		storage(storage<UTypes...> &&other)
		: item(std::move(other.item)), tail(std::move(other.tail)) { }

		T item;
		storage<Types...> tail;
	};

	template<>
	struct storage<> {

	};

	template<int n, typename... Types>
	struct nth_type;

	template<int n, typename T, typename... Types>
	struct nth_type<n, T, Types...> {
		typedef typename nth_type<n - 1, Types...>::type  type;
	};

	template<typename T, typename... Types>
	struct nth_type<0, T, Types...> {
		typedef T type;
	};

	template<int n, typename... Types>
	struct access_helper;

	template<int n, typename T, typename... Types>
	struct access_helper<n, T, Types...> {
		static typename nth_type<n - 1, Types...>::type &access(storage<T, Types...> &stor) {
			return access_helper<n - 1, Types...>::access(stor.tail);
		}
		static const typename nth_type<n - 1, Types...>::type &access(
				const storage<T, Types...> &stor) {
			return access_helper<n - 1, Types...>::access(stor.tail);
		}
	};

	template<typename T, typename... Types>
	struct access_helper<0, T, Types...> {
		static T &access(storage<T, Types...> &stor) {
			return stor.item;
		}
		static const T &access(const storage<T, Types...> &stor) {
			return stor.item;
		}
	};

} // namespace _tuple

template<typename... Types>
class tuple {
public:
	tuple() = default;

	tuple(Types... args)
	: _stor(std::move(args)...) { }

	template<typename... UTypes>
	friend class tuple;

	template<size_t n, typename... UTypes>
	struct _tuple_is_constructible {
		static constexpr bool value = std::is_constructible<typename _tuple::nth_type<n,
				 Types...>::type, typename _tuple::nth_type<n, UTypes...>::type>::
					 value && _tuple_is_constructible<n - 1,
				 UTypes...>::value;
	};

	template<typename... UTypes>
	struct _tuple_is_constructible<0, UTypes...> {
		static constexpr bool value = std::is_constructible<typename _tuple::nth_type<0,
				 Types...>::type, typename _tuple::nth_type<0, UTypes...>::type>::
					 value;
	};

	template<typename... UTypes,
		typename = std::enable_if_t<_tuple_is_constructible<
			sizeof...(UTypes) - 1, const UTypes &...>::value>>
	tuple(const tuple<UTypes...> &other) : _stor(other._stor) { }

	template<typename... UTypes,
		typename = std::enable_if_t<_tuple_is_constructible<
			sizeof...(UTypes) - 1, UTypes &&...>::value>>
	tuple(tuple<UTypes...> &&other) : _stor(std::move(other._stor)) { }

	template<int n>
	typename _tuple::nth_type<n, Types...>::type &get() {
		return _tuple::access_helper<n, Types...>::access(_stor);
	}
	template<int n>
	const typename _tuple::nth_type<n, Types...>::type &get() const {
		return _tuple::access_helper<n, Types...>::access(_stor);
	}

private:
	_tuple::storage<Types...> _stor;
};

// Specialization to allow empty tuples.
template<>
class tuple<> { };

template<typename... Types>
tuple<typename std::remove_reference_t<Types>...> make_tuple(Types &&... args) {
	return tuple<typename std::remove_reference_t<Types>...>(std::forward<Types>(args)...);
}

namespace _tuple {
	template<typename F, typename... Args, size_t... I>
	auto apply(F functor, const tuple<Args...> &args, std::index_sequence<I...>) {
		return functor(args.template get<I>()...);
	}

	template<typename F, typename... Args, size_t... I>
	auto apply(F functor, tuple<Args...> &&args, std::index_sequence<I...>) {
		return functor(std::move(args.template get<I>())...);
	}

	// Turns a set of tuple-like types into a tuple
	template<size_t, typename, typename, size_t>
	struct make_tuple_impl;

	template<size_t idx, typename Tuple, typename... Types, size_t size>
	struct make_tuple_impl<idx, tuple<Types...>, Tuple, size> :
	make_tuple_impl<idx + 1, tuple<Types..., typename std::tuple_element<idx, Tuple>::type>,
	Tuple, size>
	{};

	template<size_t size, typename Tuple, typename... Types>
	struct make_tuple_impl<size, tuple<Types...>, Tuple, size> {
		using type = tuple<Types...>;
	};

	// Helper struct to turn a tuple-like T into a tuple<T>
	template<typename T>
	struct do_make_tuple
	: public make_tuple_impl<0, tuple<>, std::remove_reference_t<T>, std::tuple_size<
	std::remove_reference_t<T>>::value>
	{};

	// Computes the return type of a tuple_cat call
	template<typename...>
	struct tuple_combiner;

	template<>
	struct tuple_combiner<> {
		using type = tuple<>;
	};

	template<typename... Ts>
	struct tuple_combiner<tuple<Ts...>> {
		using type = tuple<Ts...>;
	};

	template<typename... T1, typename... T2, typename... Remainder>
	struct tuple_combiner<tuple<T1...>, tuple<T2...>, Remainder...> {
		using type = typename tuple_combiner<tuple<T1..., T2...>, Remainder...>::type;
	};

	// Computes the return type of a tuple_cat call taking
	// a pack of tuple-like types
	template<typename... Tuples>
	struct tuple_cat_result {
		typedef typename tuple_combiner<
			typename do_make_tuple<Tuples>::type...>::type type;
	};

	// Builds an std::integer_sequence of the tuple_size of the
	// first tuple passed to the template pack
	template<typename...>
	struct make_indices_from_1st;

	template<>
	struct make_indices_from_1st<> {
		typedef typename std::make_index_sequence<0> type;
	};

	template<typename Tuple, typename... Tuples>
	struct make_indices_from_1st<Tuple, Tuples...> {
		typedef typename std::make_index_sequence<std::tuple_size<
			typename std::remove_reference<Tuple>::type>::value> type;
	};

	// Performs the actual concatenation for tuple_cat
	template<typename Ret, typename Indices, typename... Tuples>
	struct tuple_concater;

	template<typename Ret, size_t... Indices, typename Tuple, typename... Tuples>
	struct tuple_concater<Ret, std::index_sequence<Indices...>, Tuple, Tuples...> {
		template<typename... Res>
		static constexpr Ret do_concat(Tuple&& tp, Tuples&&... tps, Res&&... res) {
			typedef typename make_indices_from_1st<Tuples...>::type index;
			typedef tuple_concater<Ret, index, Tuples...> next;
			return next::do_concat(std::forward<Tuples>(tps)...,
					std::forward<Res>(res)...,
					std::move(tp.template get<Indices>())...);
		}
	};

	template<typename Ret>
	struct tuple_concater<Ret, std::index_sequence<>> {
		template <typename... Res>
		static constexpr Ret do_concat(Res&&... res) {
			return Ret(std::forward<Res>(res)...);
		}
	};
} // namespace tuple

template<typename F, typename... Args>
auto apply(F functor, const tuple<Args...> &args) {
	return _tuple::apply(std::move(functor), args, std::index_sequence_for<Args...>());
}

template<typename F, typename... Args>
auto apply(F functor, tuple<Args...> &&args) {
	return _tuple::apply(std::move(functor), std::move(args), std::index_sequence_for<Args...>());
}

template <typename... Tuples,
	 typename Ret = typename _tuple::tuple_cat_result<Tuples...>::type>
Ret tuple_cat(Tuples&&... args) {
	typedef typename _tuple::make_indices_from_1st<Tuples...>::type index;
	typedef _tuple::tuple_concater<Ret, index, Tuples...> concater;
	return concater::do_concat(std::forward<Tuples>(args)...);
}

} // namespace frg

namespace std {
	template<typename... Types>
	struct tuple_size<frg::tuple<Types...>> {
		static constexpr size_t value = sizeof...(Types);
	};

	template<size_t I, typename... Types>
	struct tuple_element<I, frg::tuple<Types...>> {
		using type = typename frg::_tuple::nth_type<I, Types...>::type;
	};
}

#endif // FRG_TUPLE_HPP
