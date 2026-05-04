#pragma once

#include <expected>
#include <type_traits>

#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

template<std::integral T>
[[nodiscard]] bool checked_add(T x, T y, T &r) {
	return !__builtin_add_overflow(x, y, &r);
}

template<std::integral T>
[[nodiscard]] bool checked_sub(T x, T y, T &r) {
	return !__builtin_sub_overflow(x, y, &r);
}

template<std::integral T>
[[nodiscard]] bool checked_mul(T x, T y, T &r) {
	return !__builtin_mul_overflow(x, y, &r);
}

struct bad_safe_int { };

template<std::integral T>
class safe_int {
public:
	using value_type = T;

	static safe_int invalid() {
		return safe_int{0, false};
	}

	safe_int()
	: safe_int{0, true} { }

	safe_int(T value)
	: safe_int{value, true} { }

	bool valid() const {
		return valid_;
	}

	std::expected<T, bad_safe_int> expected() const {
		return or_unexpected(bad_safe_int{});
	}

	template<typename E>
	std::expected<T, E> or_unexpected(E error) const {
		if(!valid_)
			return std::unexpected{error};
		return val_;
	}

	[[nodiscard]] bool into(T &result) const {
		if (!valid_)
			return false;
		result = val_;
		return true;
	}

	friend safe_int operator+(safe_int a, safe_int b) {
		return checked_op_([] (T x, T y, T &result) {
			return checked_add(x, y, result);
		}, a, b);
	}

	friend safe_int operator-(safe_int a, safe_int b) {
		return checked_op_([] (T x, T y, T &result) {
			return checked_sub(x, y, result);
		}, a, b);
	}

	friend safe_int operator*(safe_int a, safe_int b) {
		return checked_op_([] (T x, T y, T &result) {
			return checked_mul(x, y, result);
		}, a, b);
	}

	friend safe_int operator&(safe_int a, safe_int b) {
		return {a.val_ & b.val_, a.valid_ && b.valid_};
	}
	friend safe_int operator|(safe_int a, safe_int b) {
		return {a.val_ | b.val_, a.valid_ && b.valid_};
	}
	friend safe_int operator^(safe_int a, safe_int b) {
		return {a.val_ ^ b.val_, a.valid_ && b.valid_};
	}

private:
	safe_int(T value, bool valid)
	: val_{value}, valid_{valid} { }

	template<typename Op>
	static safe_int checked_op_(Op op, safe_int a, safe_int b) {
		T result;
		auto valid = op(a.val_, b.val_, result);
		return {result, valid && a.valid_ && b.valid_};
	}

	T val_;
	bool valid_;
};

} // namespace frg
