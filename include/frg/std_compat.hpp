#pragma once

namespace frg {

struct stl_allocator {
	void *allocate(size_t size) {
		return operator new(size);
	}

	void free(void *ptr) {
		operator delete(ptr);
	}
};

} // namespace frg
