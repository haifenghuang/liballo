#include "allo_assert.h"
#include "allo_mem.h"

#ifdef ALLO_FREESTANDING

void *allo_memcpy(void *dest, const void *src, size_t n) {
  ALLO_ASSERT(dest != NULL && src != NULL);
  unsigned char *d = (unsigned char *)dest;
  const unsigned char *s = (const unsigned char *)src;
  for (size_t i = 0; i < n; i++) {
    d[i] = s[i];
  }
  return dest;
}

void *allo_memset(void *s, int c, size_t n) {
  ALLO_ASSERT(s != NULL);
  unsigned char *p = (unsigned char *)s;
  while (n--) {
    *p++ = (unsigned char)c;
  }
  return s;
}

#endif
