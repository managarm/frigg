#pragma once

/* If the toolchain provides std::expected, we provide some extra tools for it.
   If one uses a mixed toolchain of clang++ combined with libstdc++, however,
   std::expected is not available even if <expected> is with some versions of
   clang, because of clang defining a lower __cpp_concepts FTM value.

   The test here verifies the preconditions on std::expected.  See
   <https://gcc.gnu.org/cgit/gcc/tree/libstdc++-v3/include/bits/version.def?id=23657d3972605542b02a33e67edcc74a7eede02c#n1504>.  */
#if __has_include(<expected>) \
  && __cpp_concepts >= 202002L \
  && __cplusplus >= 202100L
#define FRG_HAS_STD_EXPECTED
#include <expected>
#endif
#include <new>
#include <utility>
#include <type_traits>

#include <frg/macros.hpp>

#define FRG_TRY(expr) ({ \
    auto ex = (expr); \
    if(!ex) \
        return ::frg::propagate_error(ex); \
    ::frg::value_or_void(ex); \
})

#define FRG_CO_TRY(expr) ({ \
    auto ex = (expr); \
    if(!ex) \
        co_return ::frg::propagate_error(ex); \
    ::frg::value_or_void(ex); \
})

namespace frg {

struct success_tag { };

inline constexpr success_tag success;

// Contextually convert to bool but also handle enum classes.
template<typename E>
bool indicates_error(E v) {
	return v != E{};
}

// Conditionally add a non-trivial destructor.
// This should probably work with requires clauses on ~expected()
// but on Clang 10, it does not.
template<typename E, typename T, bool Destructible = std::is_trivially_destructible_v<T>>
struct destructor_crtp;

template<typename E, typename T = void>
struct [[nodiscard]] expected : destructor_crtp<E, T> {
	static_assert(std::is_default_constructible_v<E>
			&& std::is_trivially_copy_constructible_v<E>
			&& std::is_trivially_move_constructible_v<E>
			&& std::is_trivially_destructible_v<E>);
	static_assert(!std::is_convertible_v<E, T> && !std::is_convertible_v<T, E>);

	friend struct destructor_crtp<E, T>;

	template<typename = std::enable_if<std::is_default_constructible_v<T>>>
	expected()
	: e_{} {
		FRG_ASSERT(!indicates_error(e_));
		new (stor_) T{};
	}

	expected(const expected &other)
	: e_{other.e_} {
		if(!indicates_error(e_))
			new (stor_) T{*std::launder(reinterpret_cast<const T *>(other.stor_))};
	}

	expected(expected &&other)
	: e_{other.e_} {
		if(!indicates_error(e_))
			new (stor_) T{std::move(*std::launder(reinterpret_cast<T *>(other.stor_)))};
	}

	template<typename = std::enable_if<std::is_default_constructible_v<T>>>
	expected(success_tag)
	: e_{} {
		FRG_ASSERT(!indicates_error(e_));
		new (stor_) T{};
	}

	expected(E e)
	: e_{e} {
		FRG_ASSERT(indicates_error(e));
	}

	expected(T val)
	: e_{} {
		FRG_ASSERT(!indicates_error(e_));
		new (stor_) T{std::move(val)};
	}

	expected &operator= (const expected &other) {
		if(!indicates_error(other.e_)) {
			T temp{*std::launder(reinterpret_cast<T *>(other.stor_))};
			if(!indicates_error(e_))
				std::launder(reinterpret_cast<T *>(stor_))->~T();
			e_ = other.e_;
			new (stor_) T{std::move(temp)};
		}else{
			if(!indicates_error(e_))
				std::launder(reinterpret_cast<T *>(stor_))->~T();
			e_ = other.e_;
		}
		return *this;
	}

	expected &operator= (expected &&other) {
		if(!indicates_error(other.e_)) {
			T temp{std::move(*std::launder(reinterpret_cast<T *>(other.stor_)))};
			if(!indicates_error(e_))
				std::launder(reinterpret_cast<T *>(stor_))->~T();
			e_ = other.e_;
			new (stor_) T{std::move(temp)};
		}else{
			if(!indicates_error(e_))
				std::launder(reinterpret_cast<T *>(stor_))->~T();
			e_ = other.e_;
		}
		return *this;
	}


	explicit operator bool () const {
		return !indicates_error(e_);
	}

	friend bool operator==(const frg::expected<E, T> &a, const T &b) {
		return bool(a) && a.value() == b;
	}

	friend bool operator==(const frg::expected<E, T> &a, const E &b) {
		return !bool(a) && a.error() == b;
	}

	E maybe_error() const {
		return e_;
	}

	E error() const {
		FRG_ASSERT(indicates_error(e_));
		return e_;
	}

	T &value() {
		FRG_ASSERT(!indicates_error(e_));
		return *std::launder(reinterpret_cast<T *>(stor_));
	}

	const T &value() const {
		FRG_ASSERT(!indicates_error(e_));
		return *std::launder(reinterpret_cast<const T *>(stor_));
	}

	T unwrap() {
		// TODO: Take std::source_location here; print an error message.
		FRG_ASSERT(!indicates_error(e_));
		return std::move(*std::launder(reinterpret_cast<T *>(stor_)));
	}

	template<typename F>
	expected<E, std::invoke_result_t<F, T>> map(F fun) {
		if((*this))
		return fun(std::move(value()));
		return error();
	}

	template<typename F>
	expected<std::invoke_result_t<F, E>, T> map_error(F fun) {
		if(!(*this))
		return fun(error());
		return std::move(value());
	}

private:
    alignas(alignof(T)) char stor_[sizeof(T)];
    E e_;
};

template<typename E, typename T>
struct destructor_crtp<E, T, true> { };

template<typename E, typename T>
struct destructor_crtp<E, T, false> {
	~destructor_crtp() {
		auto self = static_cast<expected<E, T> *>(this);
		if(!indicates_error(self->e_)) {
			auto p = std::launder(reinterpret_cast<T *>(self->stor_));
			p->~T();
		}
	}
};

template<typename E>
struct [[nodiscard]] expected<E, void> {
	static_assert(std::is_default_constructible_v<E>
			&& std::is_trivially_copy_constructible_v<E>
			&& std::is_trivially_move_constructible_v<E>
			&& std::is_trivially_destructible_v<E>);

	expected()
	: e_{} {
		FRG_ASSERT(!indicates_error(e_));
	}

	expected(success_tag)
	: e_{} {
		FRG_ASSERT(!indicates_error(e_));
	}

	expected(E e)
	: e_{e} {
		FRG_ASSERT(indicates_error(e));
	}

	explicit operator bool () const {
		return !indicates_error(e_);
	}

	E maybe_error() const {
		return e_;
	}

	E error() const {
		FRG_ASSERT(indicates_error(e_));
		return e_;
	}

	void unwrap() {
		// TODO: Take std::source_location here; print an error message.
		FRG_ASSERT(!indicates_error(e_));
	}

	template<typename F>
	expected<std::invoke_result_t<F, E>> map_error(F fun) {
		if(!(*this))
		return fun(error());
		return {};
	}

private:
    E e_;
};

// Helper functions for the FRG_TRY macros.
template<typename E, typename T>
E propagate_error(expected<E, T> &ex) {
	return ex.error();
}

#ifdef FRG_HAS_STD_EXPECTED
template<typename E, typename T>
std::unexpected<E> propagate_error(std::expected<T, E> &ex) {
	return std::unexpected{ex.error()};
}
#endif // FRG_HAS_STD_EXPECTED

template<typename E, typename T>
T value_or_void(expected<E, T> &ex) {
	if constexpr (!std::is_same_v<T, void>)
		return std::move(ex.value());
}

#ifdef FRG_HAS_STD_EXPECTED
template<typename E, typename T>
T value_or_void(std::expected<T, E> &ex) {
	if constexpr (!std::is_same_v<T, void>)
		return std::move(ex.value());
}
#endif

} // namespace frg
