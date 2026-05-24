#define _GNU_SOURCE
#include "allo.h"

#ifdef ALLO_NOSTDLIB
  #include "allo_page_x86.c"
#else
  #include "allo_page_posix.c"
#endif

#include <assert.h>

typedef struct {
  size_t page_size;
} page_context_t;

static_assert(sizeof(page_context_t) <= ALLO_MAX_ALLOCATOR_CTX_SIZE,
              "Page allocator context exceeds maximum size");

void *page_alloc_fn(allo_t *self, size_t size) {
  if (size == 0) {
    return NULL;
  }
  page_context_t *ctx = (page_context_t *)self->_state;

  size_t rounded_size = (size + ctx->page_size - 1) & ~(ctx->page_size - 1);
  size_t total_size = rounded_size + ctx->page_size;

  void *ptr = os_mmap(total_size);
  if (!ptr) {
    return NULL;
  }

  *(size_t *)ptr = total_size;
  return (char *)ptr + ctx->page_size;
}

void *page_realloc_fn(allo_t *self, void *ptr, size_t old_size,
                      size_t new_size) {
  page_context_t *ctx = (page_context_t *)self->_state;

  size_t rounded_old = (old_size + ctx->page_size - 1) & ~(ctx->page_size - 1);
  size_t total_old = rounded_old + ctx->page_size;

  size_t rounded_new = (new_size + ctx->page_size - 1) & ~(ctx->page_size - 1);
  size_t total_new = rounded_new + ctx->page_size;

  if (total_old == total_new) {
    return ptr;
  }

  void *real_start = (char *)ptr - ctx->page_size;
  void *new_real_start = os_mremap(real_start, total_old, total_new);

  if (!new_real_start) {
    return NULL;
  }

  *(size_t *)new_real_start = total_new;
  return (char *)new_real_start + ctx->page_size;
}

void page_free_fn(allo_t *self, void *ptr) {
  if (ptr == NULL) {
    return;
  }
  page_context_t *ctx = (page_context_t *)self->_state;
  void *real_start = (char *)ptr - ctx->page_size;
  size_t total_size = *(size_t *)real_start;

  os_munmap(real_start, total_size);
}

void page_destroy_fn(allo_t *self) {
  (void)self;
}

allo_t make_page_allocator(void) {
  allo_t a = {._alloc = page_alloc_fn,
              ._realloc = page_realloc_fn,
              ._free_mem = page_free_fn,
              ._destroy = page_destroy_fn};

  page_context_t *ctx = (page_context_t *)a._state;
  ctx->page_size = os_get_page_size();

  return a;
}
