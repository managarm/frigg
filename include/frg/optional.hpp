#ifndef FRG_OPTIONAL_HPP
#define FRG_OPTIONAL_HPP

#include <new>
#include <utility>

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
		new (&_stor.object) T(object);
	}

	optional(T &&object)
	: _non_null{true} {
		new (&_stor.object) T(std::move(object));
	}

	template<typename U = value_type, typename =
		std::enable_if_t<std::is_constructible_v<T, U&&>>>
	constexpr optional(U &&value)
	: _non_null{true} {
		new (&_stor.object) T(std::forward<U>(value));
	}

	optional(const optional &other)
	: _non_null{other._non_null} {
		if(_non_null)
			new (&_stor.object) T(other._stor.object);
	}

	optional(optional &&other)
	: _non_null{other._non_null} {
		if(_non_null) {
			new (&_stor.object) T(std::move(other._stor.object));
			other._non_null = false;
		}
	}

	~optional() {
		if(_non_null)
			_stor.object.~T();
	}

	optional &operator= (optional other) {
		swap(*this, other);
		return *this;
	}

	template<class U>
	optional &operator= (const optional<U> &other) {
		if (other) {
			if (_non_null) {
				_stor.object = *other;
			} else {
				new (&_stor.object) T(*other);
				_non_null = true;
			}
		} else {
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
		return _stor.object;
	}
	constexpr T &operator* () {
		FRG_ASSERT(_non_null);
		return _stor.object;
	}
	T *operator-> () {
		FRG_ASSERT(_non_null);
		return &_stor.object;
	}

	friend void swap(optional &first, optional &second) {
		using std::swap;

		if(first._non_null && second._non_null) {
			swap(first._stor.object, second._stor.object);
		}else if(first._non_null && !second._non_null) {
			T tmp{std::move(first._stor.object)};
			first._stor.object.~T();
			new (&second._stor.object) T(std::move(tmp));
		}else if(!first._non_null && second._non_null) {
			T tmp{std::move(second._stor.object)};
			second._stor.object.~T();
			new (&first._stor.object) T(std::move(tmp));
		}

		swap(first._non_null, second._non_null);
	}
private:
	union storage_union {
		T object;
		
		storage_union() { }
		~storage_union() { } // handled by super class destructor
	};

	void _reset() {
		if (_non_null)
			_stor.object.~T();
		_non_null = false;
	}

	storage_union _stor;
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
