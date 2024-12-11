#pragma once

#include <new>
#include <utility>
#include <source_location>
#include <type_traits>

#include <frg/macros.hpp>
#include <frg/utility.hpp>

#define FRG_TRY(expr) ({ \
    auto ex = (expr); \
    if(!ex) \
        return ex.error(); \
    value_or_void(ex); \
})

#define FRG_CO_TRY(expr) ({ \
    auto ex = (expr); \
    if(!ex) \
        co_return ex.error(); \
    value_or_void(ex); \
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

	T unwrap(std::source_location loc = std::source_location::current()) {
		if(indicates_error(e_)) {
			FRG_INTF(log)(loc.file_name());
			FRG_INTF(log)("Line: ");

			auto line = loc.line();

			char buffer[21] = {};
			buffer[20] = '\0';

			size_t digits = 0;
			while(line) {
				digits++;
				line /= 10;
			}

			line = loc.line();

			for(size_t digit = 0; digit < 20 && line; digit++) {
				buffer[19 - digit] = (line % 10) + '0';
				line /= 10;
			}
			FRG_INTF(log)(&buffer[frg::max(19 - digits + 1, size_t(0))]);

			FRG_INTF(log)("Function:");
			FRG_INTF(log)(loc.function_name());
			FRG_INTF(panic)("unwrap on error type!");
		}

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

	void unwrap(std::source_location loc = std::source_location::current()) {
		if(indicates_error(e_)) {
			FRG_INTF(log)(loc.file_name());
			FRG_INTF(log)("Line: ");

			auto line = loc.line();

			char buffer[21] = {};
			buffer[20] = '\0';

			size_t digits = 0;
			while(line) {
				digits++;
				line /= 10;
			}

			line = loc.line();

			for(size_t digit = 0; digit < 20 && line; digit++) {
				buffer[19 - digit] = (line % 10) + '0';
				line /= 10;
			}
			FRG_INTF(log)(&buffer[frg::max(19 - digits + 1, size_t(0))]);

			FRG_INTF(log)("Function:");
			FRG_INTF(log)(loc.function_name());
			FRG_INTF(panic)("unwrap on error type!");
		}
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

// Helper function for the FRG_TRY macros.
template<typename E, typename T>
T value_or_void(expected<E, T> &ex) {
	if constexpr (!std::is_same_v<T, void>)
		return std::move(ex.value());
}

} // namespace frg
