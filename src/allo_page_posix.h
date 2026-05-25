#include <sys/mman.h>
#include <unistd.h>

static inline void *allo_os_mmap(size_t length) {
  void *ptr = mmap(NULL, length, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED) {
    return NULL;
  }
  return ptr;
}

static inline int allo_os_munmap(void *addr, size_t length) {
  return munmap(addr, length);
}

static inline void *allo_os_mremap(void *addr, size_t old_len, size_t new_len) {
  void *ptr = mremap(addr, old_len, new_len, MREMAP_MAYMOVE);
  if (ptr == MAP_FAILED) {
    return NULL;
  }
  return ptr;
}

static inline size_t allo_os_get_page_size(void) {
  return (size_t)sysconf(_SC_PAGESIZE);
}
