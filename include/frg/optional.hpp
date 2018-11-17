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

	operator bool() {
		return _non_null;
	}

	T &operator* () {
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

	storage_union _stor;
	bool _non_null;
};

} // namespace frg

#endif // FRG_OPTIONAL_HPP
