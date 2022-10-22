#pragma once

#if __has_include(<string.h>)
#  include <string.h>
#else
extern "C" {
void *memcpy(void *dest, const void *src, size_t n);
}
#endif
