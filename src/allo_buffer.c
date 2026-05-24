#include "allo.h"
#include "asan.h"
#include <assert.h>

typedef struct {
  void *buffer;
  size_t size;
  size_t offset;
} allocator_buf_context_t;

static_assert(sizeof(allocator_buf_context_t) <= ALLO_MAX_ALLOCATOR_CTX_SIZE,
              "Buffer allocator context exceeds maximum size");

void *buf_alloc_fn(allo_t *self, size_t size) {
  if (size == 0) {
    return NULL;
  }
  allocator_buf_context_t *ctx = (allocator_buf_context_t *)self->_state;

  size_t aligned_offset = (ctx->offset + 7) & ~7;

  if (aligned_offset + size > ctx->size) {
    return NULL; // Not enough space
  }
  void *ptr = (char *)ctx->buffer + aligned_offset;
  ctx->offset = aligned_offset + size;
  ALLOC_UNPOISON(ptr, size);
  return ptr;
}

void *buf_realloc_fn(allo_t *self, void *ptr, size_t old_size,
                     size_t new_size) {
  allocator_buf_context_t *ctx = (allocator_buf_context_t *)self->_state;

  // Check if this was the last allocation
  if ((char *)ptr + old_size == (char *)ctx->buffer + ctx->offset) {
    size_t base_offset = (size_t)((char *)ptr - (char *)ctx->buffer);
    if (base_offset + new_size <= ctx->size) {
      ctx->offset = base_offset + new_size;
      if (new_size > old_size) {
        ALLOC_UNPOISON((char *)ptr + old_size, new_size - old_size);
      } else {
        ALLOC_POISON((char *)ptr + new_size, old_size - new_size);
      }
      return ptr;
    }
  }

  // Fallback: if shrinking, just return. If growing, try to allocate new.
  if (new_size <= old_size) {
    return ptr;
  }

  void *new_ptr = buf_alloc_fn(self, new_size);
  if (!new_ptr) {
    return NULL;
  }

  memcpy(new_ptr, ptr, old_size);
  return new_ptr;
}

void buf_free_fn(allo_t *self, void *ptr) {
  (void)self; // Unused
  (void)ptr;  // Unused
}

void buf_destroy_fn(allo_t *self) {
  (void)self; // No-op for fixed buffer
}

// Factory to create a buffer allocator
allo_t make_fixed_buf_allocator(void *buffer, size_t size) {
  allo_t a = {._alloc = buf_alloc_fn,
              ._realloc = buf_realloc_fn,
              ._free_mem = buf_free_fn,
              ._destroy = buf_destroy_fn};

  allocator_buf_context_t *ctx = (allocator_buf_context_t *)a._state;
  ctx->buffer = buffer;
  ctx->size = size;
  ctx->offset = 0;

  ALLOC_POISON(buffer, size);

  return a;
}
