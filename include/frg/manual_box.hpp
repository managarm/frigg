#ifndef FRG_MANUAL_BOX_HPP
#define FRG_MANUAL_BOX_HPP

#include <new>
#include <utility>

#include <frg/eternal.hpp>
#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

template<typename T>
class manual_box {
public:
	constexpr manual_box()
	: _initialized{false} { }

	template<typename... Args>
	void initialize(Args &&... args) {
		FRG_ASSERT(!_initialized);
		new(&_storage) T{std::forward<Args>(args)...};
		_initialized = true;
	}

	T *get() {
		FRG_ASSERT(_initialized);
		return reinterpret_cast<T *>(&_storage);
	}
	
	bool valid() {
		return _initialized;
	}

	T *operator-> () {
		return get();
	}
	T &operator* () {
		return *get();
	}

private:
	aligned_storage<sizeof(T), alignof(T)> _storage;
	bool _initialized;
};

} // namespace frigg

#endif // FRG_MANUAL_BOX_HPP
