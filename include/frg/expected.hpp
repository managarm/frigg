#pragma once

#include <new>
#include <utility>

#include <frg/macros.hpp>

#define FRG_TRY(expr) ({ \
    auto ex = (expr); \
    if(!ex) \
        return ex.error(); \
    ex.value(); \
})

#define FRG_CO_TRY(expr) ({ \
    auto ex = (expr); \
    if(!ex) \
        co_return ex.error(); \
    ex.value(); \
})

namespace frg {

// Contextually convert to bool but also handle enum classes.
template<typename E>
bool indicates_error(E v) {
	return v != E{};
}

template<typename E, typename T = void>
struct expected {
	static_assert(std::is_default_constructible_v<E>
			&& std::is_trivially_copy_constructible_v<E>
			&& std::is_trivially_move_constructible_v<E>
			&& std::is_trivially_destructible_v<E>);
	static_assert(!std::is_convertible_v<E, T> && !std::is_convertible_v<T, E>);

    expected()
	requires (std::is_default_constructible_v<T>)
    : e_{} {
        new (stor_) T{};
    }

	expected(const expected &other)
	requires (std::is_trivially_copy_constructible_v<T>)
	= default;

	expected(const expected &other)
	requires (!std::is_trivially_copy_constructible_v<T>)
	: e_{other.e_} {
		if(!indicates_error(e_))
			new (stor_) T{*std::launder(reinterpret_cast<T *>(other.stor_))};
	}

	expected(expected &&other)
	requires (std::is_trivially_move_constructible_v<T>)
	= default;

	expected(expected &&other)
	requires (!std::is_trivially_move_constructible_v<T>)
	: e_{other.e_} {
		if(!indicates_error(e_))
			new (stor_) T{std::move(*std::launder(reinterpret_cast<T *>(other.stor_)))};
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

	~expected()
	requires (std::is_trivially_destructible_v<T>)
	= default;

    ~expected()
	requires (!std::is_trivially_destructible_v<T>) {
        if(!indicates_error(e_)) {
            auto p = std::launder(reinterpret_cast<T *>(stor_));
            p->~T();
        }
    }

	expected &operator= (const expected &other)
	requires (std::is_trivially_copy_assignable_v<T>)
	= default;

	expected &operator= (const expected &other)
	requires (!std::is_trivially_copy_assignable_v<T>) {
		if(indicates_error(other.e_)) {
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

	expected &operator= (expected &&other)
	requires (std::is_trivially_move_assignable_v<T>)
	= default;

	expected &operator= (expected &&other)
	requires (!std::is_trivially_move_assignable_v<T>) {
		if(indicates_error(other.e_)) {
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

template<typename E>
struct expected<E, void> {
	static_assert(std::is_default_constructible_v<E>
			&& std::is_trivially_copy_constructible_v<E>
			&& std::is_trivially_move_constructible_v<E>
			&& std::is_trivially_destructible_v<E>);

    expected()
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

    template<typename F>
    expected<std::invoke_result_t<F, E>> map_error(F fun) {
        if(!(*this))
            return fun(error());
        return {};
    }

private:
    E e_;
};

} // namespace frg
