#include <stddef.h>
#include <stdint.h>

// Manual system calls for x86_64 Linux
static inline long sys_syscall2(long n, long a1, long a2) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(n), "D"(a1), "S"(a2)
                   : "rcx", "r11", "memory");
  return ret;
}

static inline long sys_syscall4(long n, long a1, long a2, long a3, long a4) {
  long ret;
  register long r10 __asm__("r10") = a4;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
                   : "rcx", "r11", "memory");
  return ret;
}

static inline long sys_syscall6(long n, long a1, long a2, long a3, long a4,
                                long a5, long a6) {
  long ret;
  register long r10 __asm__("r10") = a4;
  register long r8 __asm__("r8") = a5;
  register long r9 __asm__("r9") = a6;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8),
                     "r"(r9)
                   : "rcx", "r11", "memory");
  return ret;
}

#define SYS_mmap 9
#define SYS_munmap 11
#define SYS_mremap 163

#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED ((void *)-1)
#define MREMAP_MAYMOVE 1

static inline void *allo_os_mmap(size_t length) {
  void *ptr = (void *)sys_syscall6(SYS_mmap, 0, length, PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED) {
    return NULL;
  }
  return ptr;
}

static inline int allo_os_munmap(void *addr, size_t length) {
  return (int)sys_syscall2(SYS_munmap, (long)addr, length);
}

static inline void *allo_os_mremap(void *addr, size_t old_len, size_t new_len) {
  void *ptr = (void *)sys_syscall4(SYS_mremap, (long)addr, old_len, new_len,
                                   MREMAP_MAYMOVE);
  if (ptr == MAP_FAILED) {
    return NULL;
  }
  return ptr;
}

static inline size_t allo_os_get_page_size(void) {
  return 4096;
}
