#pragma once

#include <type_traits>
#include <utility>

#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

// Non-owning reference to a callable, similar to std::function_ref.
// Useful to pass lambdas across virtual function boundaries without allocating.
// The referenced callable must outlive the function_ref (binding a temporary is fine
// as long as the function_ref is only used within the full expression, e.g., as an argument).
template<typename Signature>
struct function_ref;

template<typename R, typename... Args>
struct function_ref<R(Args...)> {
	// Pointers-to-member are excluded since the thunk uses plain call syntax.
	template<typename F>
	requires (!std::is_same_v<std::remove_cvref_t<F>, function_ref>
			&& !std::is_function_v<std::remove_reference_t<F>>
			&& !std::is_member_pointer_v<std::remove_cvref_t<F>>
			&& std::is_invocable_r_v<R, F &, Args...>)
	function_ref(F &&f) noexcept
	: thunk_{&invoke_object<std::remove_reference_t<F>>} {
		storage_.obj = const_cast<void *>(static_cast<const void *>(__builtin_addressof(f)));
	}

	template<typename F>
	requires (std::is_function_v<F> && std::is_invocable_r_v<R, F *, Args...>)
	function_ref(F *f) noexcept
	: thunk_{&invoke_function<F>} {
		storage_.fn = reinterpret_cast<void (*)()>(f);
	}

	// Assigning a temporary callable would leave the function_ref dangling
	// as soon as the assignment expression ends.
	template<typename F>
	requires (!std::is_lvalue_reference_v<F>
			&& !std::is_same_v<std::remove_cvref_t<F>, function_ref>
			&& !std::is_pointer_v<std::remove_cvref_t<F>>)
	function_ref &operator= (F &&) = delete;

	R operator() (Args... args) const {
		return thunk_(storage_, std::forward<Args>(args)...);
	}

private:
	union storage {
		void *obj;
		void (*fn)();
	};

	// The thunks take their arguments by reference to avoid moving
	// by-value arguments a second time.
	template<typename T>
	static R invoke_object(storage s, Args &&... args) {
		auto &f = *static_cast<T *>(s.obj);
		if constexpr (std::is_void_v<R>)
			f(std::forward<Args>(args)...);
		else
			return f(std::forward<Args>(args)...);
	}

	template<typename F>
	static R invoke_function(storage s, Args &&... args) {
		auto f = reinterpret_cast<F *>(s.fn);
		if constexpr (std::is_void_v<R>)
			f(std::forward<Args>(args)...);
		else
			return f(std::forward<Args>(args)...);
	}

	storage storage_;
	R (*thunk_)(storage, Args &&...);
};

template<auto Ptr>
struct bound_mem_fn;

template<typename C, typename R, typename... Args, R (C:: *Ptr) (Args...)>
struct bound_mem_fn<Ptr> {
	bound_mem_fn(C *object)
	: object_{object} { }

	template<typename... X>
	R operator() (X &&... args) {
		return (object_->*Ptr)(std::forward<X>(args)...);
	}

private:
	C *object_;
};

} // namespace frg
