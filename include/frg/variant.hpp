#pragma once

#include <frg/eternal.hpp>
#include <frg/macros.hpp>
#include <type_traits>
#include <stddef.h>

namespace frg {

namespace _variant {
	// check if S is one of the types T
	template<typename S, typename... T>
	struct exists : public std::false_type { };

	template<typename S, typename... T>
	struct exists<S, S, T...> : public std::true_type { };

	template<typename S, typename H, typename... T>
	struct exists<S, H, T...> : public exists<S, T...> { };

	// get the index of S in the argument pack T
	template<typename, typename S, typename... T>
	struct index_of_helper { };

	template<typename S, typename... T>
	struct index_of_helper<std::enable_if_t<exists<S, S, T...>::value>, S, S, T...>
	: public std::integral_constant<size_t, 0> { };

	template<typename S, typename H, typename... T>
	struct index_of_helper<std::enable_if_t<exists<S, H, T...>::value>, S, H, T...>
	: public std::integral_constant<size_t, index_of_helper<void, S, T...>::value + 1> { };

	template<typename S, typename... T>
	using index_of = index_of_helper<void, S, T...>;

	// get a type with a certain index from the argument pack T
	template<size_t Index, typename... T>
	struct get_helper { };

	template<typename H, typename... T>
	struct get_helper<0, H, T...> {
		using type = H;
	};

	template<size_t Index, typename H, typename... T>
	struct get_helper<Index, H, T...>
	: public get_helper<Index - 1, T...> { };

	template<size_t Index, typename... T>
	using get = typename get_helper<Index, T...>::type;
};

template<typename... T>
struct variant {
	static constexpr size_t invalid_tag = size_t(-1);

	template<typename X, size_t Index = _variant::index_of<X, T...>::value>
	static constexpr size_t tag_of() {
		return Index;
	}

	variant() : tag_{invalid_tag} { }

	template<typename X, size_t Index = _variant::index_of<X, T...>::value>
	variant(X object) : variant() {
		construct_<Index>(std::move(object));
	};

	variant(const variant &other) : variant() {
		if(other)
			copy_construct_<0>(other);
	}

	variant(variant &&other) : variant() {
		if(other)
			move_construct_<0>(std::move(other));
	}

	~variant() {
		if(*this)
			destruct_<0>();
	}

	explicit operator bool() const {
		return tag_ != invalid_tag;
	}

	variant &operator= (variant other) {
		// Because swap is quite hard to implement for this type we don't use copy-and-swap.
		// Instead we perform a destruct-then-move-construct operation on the internal object.
		// Note that we take the argument by value so there are no self-assignment problems.
		if(tag_ == other.tag_) {
			assign_<0>(std::move(other));
		} else {
			if(*this)
				destruct_<0>();
			if(other)
				move_construct_<0>(std::move(other));
		}
		return *this;
	}

	size_t tag() {
		return tag_;
	}

	template<typename X, size_t Index = _variant::index_of<X, T...>::value>
	bool is() const {
		return tag_ == Index;
	}

	template<typename X, size_t Index = _variant::index_of<X, T...>::value>
	X &get() {
		FRG_ASSERT(tag_ == Index);
		return *std::launder(reinterpret_cast<X *>(access_()));
	}
	template<typename X, size_t Index = _variant::index_of<X, T...>::value>
	const X &get() const {
		FRG_ASSERT(tag_ == Index);
		return *std::launder(reinterpret_cast<const X *>(access_()));
	}

	template<typename X, size_t Index = _variant::index_of<X, T...>::value,
			typename... Args>
	void emplace(Args &&... args) {
		if(tag_ != invalid_tag)
			destruct_<0>();
		new (access_()) X(std::forward<Args>(args)...);
		tag_ = Index;
	}

	template<typename F>
	std::common_type_t<std::invoke_result_t<F, T&>...> apply(F functor) {
		return apply_<F, 0>(std::move(functor));
	}

	template<typename F>
	std::common_type_t<std::invoke_result_t<F, const T&>...> const_apply(F functor) const {
		return apply_<F, 0>(std::move(functor));
	}

private:
	void *access_() {
		return storage_.buffer;
	}
	const void *access_() const {
		return storage_.buffer;
	}

	// construct the internal object from one of the summed types
	template<size_t Index, typename X = _variant::get<Index, T...>>
	void construct_(X object) {
		FRG_ASSERT(!*this);
		new (access_()) X(std::move(object));
		tag_ = Index;
	}

	// construct the internal object by copying from another variant
	template<size_t Index> requires (Index < sizeof...(T))
	void copy_construct_(const variant &other) {
		using value_type = _variant::get<Index, T...>;
		if(other.tag_ == Index) {
			FRG_ASSERT(!*this);
			new (access_()) value_type(other.get<value_type>());
			tag_ = Index;
		} else {
			copy_construct_<Index + 1>(other);
		}
	}

	template<size_t Index> requires (Index == sizeof...(T))
	void copy_construct_(const variant &) {
		FRG_ASSERT(!"Copy-construction from variant with illegal tag");
	}

	// construct the internal object by moving from another variant
	template<size_t Index> requires (Index < sizeof...(T))
	void move_construct_(variant &&other) {
		using value_type = _variant::get<Index, T...>;
		if(other.tag_ == Index) {
			FRG_ASSERT(!*this);
			new (access_()) value_type(std::move(other.get<value_type>()));
			tag_ = Index;
		} else {
			move_construct_<Index + 1>(std::move(other));
		}
	}

	template<size_t Index> requires (Index == sizeof...(T))
	void move_construct_(variant &&) {
		FRG_ASSERT(!"Move-construction from variant with illegal tag");
	}

	// destruct the internal object
	template<size_t Index> requires (Index < sizeof...(T))
	void destruct_() {
		using value_type = _variant::get<Index, T...>;
		if(tag_ == Index) {
			get<value_type>().~value_type();
			tag_ = invalid_tag;
		} else {
			destruct_<Index + 1>();
		}
	}

	template<size_t Index> requires (Index == sizeof...(T))
	void destruct_() {
		FRG_ASSERT(!"Destruction of variant with illegal tag");
	}

	// assign the internal object
	template<size_t Index> requires (Index < sizeof...(T))
	void assign_(variant other) {
		using value_type = _variant::get<Index, T...>;
		if(tag_ == Index) {
			get<value_type>() = std::move(other.get<value_type>());
		} else {
			assign_<Index + 1>(std::move(other));
		}
	}

	template<size_t Index> requires (Index == sizeof...(T))
	void assign_(variant) {
		FRG_ASSERT(!"Assignment from variant with illegal tag");
	}

	// apply a functor to the internal object
	template<typename F, size_t Index> requires (Index < sizeof...(T))
	std::common_type_t<std::invoke_result_t<F, T&>...>
	apply_(F functor) {
		using value_type = _variant::get<Index, T...>;
		if(tag_ == Index) {
			return functor(get<value_type>());
		} else {
			return apply_<F, Index + 1>(std::move(functor));
		}
	}

	template<typename F, size_t Index> requires (Index == sizeof...(T))
	std::common_type_t<std::invoke_result_t<F, T&>...>
	apply_(F) {
		FRG_ASSERT(!"_apply() on variant with illegal tag");
		__builtin_unreachable();
	}

	size_t tag_;
	frg::aligned_union<T...> storage_;
};

} // namespace frg
