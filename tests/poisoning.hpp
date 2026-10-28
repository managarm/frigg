#pragma once

#include <stddef.h>

#if defined(__SANITIZE_ADDRESS__)
#define FRG_TESTS_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define FRG_TESTS_ASAN 1
#endif
#endif

#ifdef FRG_TESTS_ASAN
#include <sanitizer/asan_interface.h>
#endif

// Base class that gives test policies poisoning support.
// Under ASan, accesses to poisoned memory are reported; otherwise, poisoning has no effect.
// Like a kernel heap, policies should poison memory in map() until the allocator unpoisons it.
struct asan_poisoning {
	void poison(void *p, size_t size) {
#ifdef FRG_TESTS_ASAN
		ASAN_POISON_MEMORY_REGION(p, size);
#else
		(void)p;
		(void)size;
#endif
	}

	void unpoison(void *p, size_t size) {
#ifdef FRG_TESTS_ASAN
		ASAN_UNPOISON_MEMORY_REGION(p, size);
#else
		(void)p;
		(void)size;
#endif
	}

	void unpoison_expand(void *p, size_t size) {
		unpoison(p, size);
	}
};
