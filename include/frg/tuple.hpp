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
			sizeof...(UTypes) - 1, Types..., const UTypes&...>::value>>
	tuple(const tuple<UTypes...> &other) : _stor(other._stor) { }

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
	auto apply(F functor, tuple<Args...> args, std::index_sequence<I...>) {
		return functor(std::move(args.template get<I>())...);
	}
} // namespace tuple

template<typename F, typename... Args>
auto apply(F functor, tuple<Args...> args) {
	return _tuple::apply(std::move(functor), std::move(args), std::index_sequence_for<Args...>());
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
