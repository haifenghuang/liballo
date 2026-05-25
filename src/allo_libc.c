#include "allo.h"
#include "asan.h"
#include <stdlib.h>
#include <string.h>

void *c_alloc_fn(allo_t *self, size_t size) {
  (void)self;
  if (size == 0) {
    return NULL;
  }
  void *ptr = malloc(size);
  if (ptr) {
    ALLOC_UNPOISON(ptr, size);
  }
  return ptr;
}

void *c_realloc_fn(allo_t *self, void *ptr, size_t old_size, size_t new_size) {
  (void)self;
  // Unpoison before realloc just in case realloc needs to read it
  if (ptr) {
    ALLOC_UNPOISON(ptr, old_size);
  }
  void *new_ptr = realloc(ptr, new_size);
  if (new_ptr) {
    ALLOC_UNPOISON(new_ptr, new_size);
  } else if (ptr) {
    // Restore poisoning on failure
    ALLOC_UNPOISON(ptr, old_size);
  }
  return new_ptr;
}

void c_free_fn(allo_t *self, void *ptr, size_t size) {
  (void)self;
  if (ptr == NULL) {
    return;
  }
  ALLOC_POISON(ptr, size);
  free(ptr);
}

// Factory to create a standard C allocator
allo_error_t make_c_allocator(allo_t *out) {
  if (!out)
    return ALLO_ERR_INVAL;
  *out = (allo_t){._alloc = c_alloc_fn,
                  ._realloc = c_realloc_fn,
                  ._free_mem = c_free_fn,
                  ._destroy = NULL,
                  ._contains = NULL};
  return ALLO_OK;
}
