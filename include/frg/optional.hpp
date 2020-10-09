#ifndef FRG_OPTIONAL_HPP
#define FRG_OPTIONAL_HPP

#include <new>
#include <utility>

#include <frg/eternal.hpp>
#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

struct null_opt_type { };
static constexpr null_opt_type null_opt;

template<typename T>
class optional {
public:
	using value_type = T;

	optional()
	: _non_null{false} { }
	
	optional(null_opt_type)
	: _non_null{false} { }

	optional(const T &object)
	: _non_null{true} {
		new (_stor.buffer) T(object);
	}

	optional(T &&object)
	: _non_null{true} {
		new (_stor.buffer) T(std::move(object));
	}

	template<typename U = value_type, typename =
		std::enable_if_t<std::is_constructible_v<T, U&&>>>
	constexpr optional(U &&value)
	: _non_null{true} {
		new (_stor.buffer) T(std::forward<U>(value));
	}

	optional(const optional &other)
	: _non_null{other._non_null} {
		if(_non_null)
			new (_stor.buffer) T(*other._object());
	}

	optional(optional &&other)
	: _non_null{other._non_null} {
		if(_non_null)
			new (_stor.buffer) T(std::move(*other._object()));
	}

	~optional() {
		if(_non_null)
			_reset();
	}

	optional &operator= (const optional &other) {
		if (other._non_null) {
			if (_non_null) {
				*_object() = *other._object();
			} else {
				new (_stor.buffer) T(*other._object());
				_non_null = true;
			}
		} else {
			if(_non_null)
				_reset();
		}
		return *this;
	}

	optional &operator= (optional &&other) {
		if (other._non_null) {
			if (_non_null) {
				*_object() = std::move(*other._object());
			} else {
				new (_stor.buffer) T(std::move(*other._object()));
				_non_null = true;
			}
		} else {
			if(_non_null)
				_reset();
		}
		return *this;
	}

	// Exactly the same as above but for more general types.
	template<class U>
	optional &operator= (const optional<U> &other) {
		if (other) {
			if (_non_null) {
				*_object() = *other;
			} else {
				new (_stor.buffer) T(*other);
				_non_null = true;
			}
		} else {
			if(_non_null)
				_reset();
		}
		return *this;
	}

	// Exactly the same as above but for more general types.
	template<class U>
	optional &operator= (optional<U> &&other) {
		if (other) {
			if (_non_null) {
				*_object() = std::move(*other);
			} else {
				new (_stor.buffer) T(std::move(*other));
				_non_null = true;
			}
		} else {
			if(_non_null)
				_reset();
		}
		return *this;
	}


	constexpr operator bool() const {
		return _non_null;
	}
	constexpr operator bool() {
		return _non_null;
	}

	constexpr bool has_value() const {
		return _non_null;
	}

	constexpr const T &operator* () const {
		FRG_ASSERT(_non_null);
		return *_object();
	}
	constexpr T &operator* () {
		FRG_ASSERT(_non_null);
		return *_object();
	}
	T *operator-> () {
		FRG_ASSERT(_non_null);
		return _object();
	}
	T &value() {
		FRG_ASSERT(_non_null);
		return *_object();
	}

	template <typename ...Args>
	void emplace(Args &&...args) {
		if(_non_null)
			_reset();
		new (_stor.buffer) T(std::forward<Args>(args)...);
		_non_null = true;
	}

private:
	const T *_object() const {
		return std::launder(reinterpret_cast<const T *>(_stor.buffer));
	}
	T *_object() {
		return std::launder(reinterpret_cast<T *>(_stor.buffer));
	}

	void _reset() {
		_object()->~T();
		_non_null = false;
	}

	aligned_storage<sizeof(T), alignof(T)> _stor;
	bool _non_null;
};

template<class T, class U>
constexpr bool operator==(const optional<T> &opt, const U &value) {
	return opt ? (*opt == value) : false;
}
template<class T, class U>
constexpr bool operator==(const U &value, const optional<T> &opt) {
	return opt ? (value == *opt) : false;
}

template<class T, class U>
constexpr bool operator!=(const optional<T> &opt, const U &value) {
	return opt ? (*opt != value) : true;
}
template<class T, class U>
constexpr bool operator!=(const U &value, const optional<T> &opt) {
	return opt ? (value != *opt) : true;
}

template<class T, class U>
constexpr bool operator<(const optional<T> &opt, const U &value) {
	return opt ? (*opt < value) : true;
}
template<class T, class U>
constexpr bool operator<(const T &value, const optional<U> &opt) {
	return opt ? (value < *opt) : false;
}

} // namespace frg

#endif // FRG_OPTIONAL_HPP
