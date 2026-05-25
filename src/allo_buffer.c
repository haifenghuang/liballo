#include "allo.h"
#include "asan.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

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

  assert(ctx->offset <= ctx->size && "Offset should never exceed buffer size");

  size_t aligned_offset = ALLO_ALIGN_UP(ctx->offset, 8);
  if (aligned_offset > ctx->size || size > ctx->size - aligned_offset) {
    return NULL;
  }

  void *ptr = (char *)ctx->buffer + aligned_offset;
  ctx->offset = aligned_offset + size;
  ALLOC_UNPOISON(ptr, size);
  return ptr;
}

void *buf_realloc_fn(allo_t *self, void *ptr, size_t old_size,
                     size_t new_size) {
  allocator_buf_context_t *ctx = (allocator_buf_context_t *)self->_state;

  // Optimization: if the pointer is the most recent allocation and there's
  // enough space to grow, just adjust the offset.
  bool is_last_alloc =
      (char *)ptr + old_size == (char *)ctx->buffer + ctx->offset;

  if (is_last_alloc) {
    size_t base_offset = (size_t)((char *)ptr - (char *)ctx->buffer);
    bool fits_in_current = base_offset + new_size <= ctx->size;

    if (fits_in_current) {
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

void buf_free_fn(allo_t *self, void *ptr, size_t size) {
  (void)self;
  if (ptr) {
    ALLOC_POISON(ptr, size);
  }
}

void buf_destroy_fn(allo_t *self) {
  allocator_buf_context_t *ctx = (allocator_buf_context_t *)self->_state;
  ALLOC_POISON(ctx->buffer, ctx->size);
}

// Factory to create a buffer allocator
allo_error_t make_fixed_buf_allocator(allo_t *out, void *buffer, size_t size) {
  if (!out || !buffer || size == 0)
    return ALLO_ERR_INVAL;

  /* Buffer must be 8-byte aligned for allocator alignment guarantees */
  if (!ALLO_IS_ALIGNED(buffer, 8))
    return ALLO_ERR_INVAL;

  *out = (allo_t){._alloc = buf_alloc_fn,
                  ._realloc = buf_realloc_fn,
                  ._free_mem = buf_free_fn,
                  ._destroy = buf_destroy_fn};

  allocator_buf_context_t *ctx = (allocator_buf_context_t *)out->_state;
  ctx->buffer = buffer;
  ctx->size = size;
  ctx->offset = 0;

  ALLOC_POISON(buffer, size);

  return ALLO_OK;
}
