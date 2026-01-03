#include <stdio.h>
#include <stdlib.h>

#include <frg/macros.hpp>

extern "C" void frg_log(const char *msg) {
	fprintf(stderr, "frg: %s\n", msg);
}

extern "C" void frg_panic(const char *msg) {
	fprintf(stderr, "frg panic: %s\n", msg);
	abort();
}
