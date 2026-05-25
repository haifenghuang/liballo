#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#ifdef ALLO_NOSTDLIB

  #pragma GCC diagnostic ignored "-Wpedantic"

  #include <stddef.h>
  #include <stdint.h>

// Manual system calls for x86_64 Linux
static inline long sys_syscall1(long n, long a1) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(n), "D"(a1)
                   : "rcx", "r11", "memory");
  return ret;
}

static inline long sys_syscall3(long n, long a1, long a2, long a3) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(n), "D"(a1), "S"(a2), "d"(a3)
                   : "rcx", "r11", "memory");
  return ret;
}

  #define SYS_write 1
  #define SYS_exit_group 231

// Minimal printf-like functionality
static inline void sys_print(const char *s) {
  size_t len = 0;
  while (s[len])
    len++;
  sys_syscall3(SYS_write, 1, (long)s, len);
}

  #undef assert
  #define assert(expr)                                                         \
    do {                                                                       \
      if (!(expr)) {                                                           \
        sys_print("Assertion failed: " #expr "\n");                            \
        sys_syscall1(SYS_exit_group, 1);                                       \
      }                                                                        \
    } while (0)

  #define printf(fmt, ...)                                                     \
    do {                                                                       \
      (void)sizeof("" #__VA_ARGS__);                                           \
      sys_print(fmt);                                                          \
    } while (0)

  // Use macros to avoid conflict with potential builtins or declarations
  #define memset __builtin_memset
  #define memcpy __builtin_memcpy

static inline void *__builtin_memset(void *s, int c, size_t n) {
  unsigned char *p = (unsigned char *)s;
  while (n--)
    *p++ = (unsigned char)c;
  return s;
}

static inline void *__builtin_memcpy(void *dest, const void *src, size_t n) {
  unsigned char *d = (unsigned char *)dest;
  const unsigned char *s = (const unsigned char *)src;
  while (n--)
    *d++ = *s++;
  return dest;
}

  // malloc/free are NOT available in nostdlib mode without page allocator.
  // Tests using them should be gated with #ifndef ALLO_NOSTDLIB.

#else
  #include <assert.h>
  #include <stdint.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
#endif

#endif // TEST_HARNESS_H
