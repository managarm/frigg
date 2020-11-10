#pragma once

namespace frg {

struct stl_allocator {
	void *allocate(size_t size) {
		return operator new(size);
	}

	void deallocate(void *ptr, size_t size) {
		operator delete(ptr, size);
	}

	void free(void *ptr) {
		operator delete(ptr);
	}
};

} // namespace frg
