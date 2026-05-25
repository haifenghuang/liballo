#ifndef ALLO_MEM_H
#define ALLO_MEM_H

#include <stddef.h>

#ifndef ALLO_FREESTANDING
  #include <string.h>

  // Use standard library functions
  #define allo_memcpy memcpy
  #define allo_memset memset
#else
  // Provide custom implementations
  #define memcpy allo_memcpy
  #define memset allo_memset

void *allo_memcpy(void *dest, const void *src, size_t n);
void *allo_memset(void *s, int c, size_t n);
#endif

#endif /* ALLO_MEM_H */
