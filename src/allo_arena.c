#include "allo.h"
#include "asan.h"

#include <assert.h>
#include <stdbool.h>

typedef struct arena_block {
  struct arena_block *next;
  size_t size;
  size_t offset;
} arena_block_t;

typedef struct {
  arena_block_t *first;
  arena_block_t *current;
  size_t block_size;
  allo_t *child;
} arena_context_t;

static_assert(sizeof(arena_context_t) <= ALLO_MAX_ALLOCATOR_CTX_SIZE,
              "Arena allocator context exceeds maximum size");

allo_contains_t arena_contains_fn(allo_t *self, void *ptr) {
  arena_context_t *ctx = (arena_context_t *)self->_state;

  // Optimization: check current block first
  char *block_start = (char *)ctx->current + sizeof(arena_block_t);
  char *block_end = block_start + ctx->current->size;
  if (ptr >= (void *)block_start && ptr < (void *)block_end) {
    return ALLO_CONTAINS_YES;
  }

  // Fallback to iterating all blocks
  arena_block_t *curr = ctx->first;
  while (curr) {
    if (curr != ctx->current) { // Already checked
      block_start = (char *)curr + sizeof(arena_block_t);
      block_end = block_start + curr->size;
      if (ptr >= (void *)block_start && ptr < (void *)block_end) {
        return ALLO_CONTAINS_YES;
      }
    }
    curr = curr->next;
  }
  return ALLO_CONTAINS_NO;
}

static arena_block_t *new_arena_block(allo_t *child, size_t size) {
  arena_block_t *block = allo_alloc(child, sizeof(arena_block_t) + size);
  if (!block)
    return NULL;
  block->next = NULL;
  block->size = size;
  block->offset = 0;
  ALLOC_POISON((char *)block + sizeof(arena_block_t), size);
  return block;
}

void *arena_alloc_fn(allo_t *self, size_t size) {
  if (size == 0) {
    return NULL;
  }
  arena_context_t *ctx = (arena_context_t *)self->_state;

  size_t aligned_offset = ALLO_ALIGN_UP(ctx->current->offset, 8);

  if (size > ctx->block_size) {
    // Large allocation, create a dedicated block
    arena_block_t *block = new_arena_block(ctx->child, size);
    if (!block)
      return NULL;
    // Insert after current block
    block->next = ctx->current->next;
    ctx->current->next = block;
    block->offset = size;
    void *ptr = (char *)block + sizeof(arena_block_t);
    ALLOC_UNPOISON(ptr, size);
    return ptr;
  }

  if (aligned_offset + size > ctx->current->size) {
    // Current block is full, create a new one
    arena_block_t *block = new_arena_block(ctx->child, ctx->block_size);
    if (!block)
      return NULL;
    ctx->current->next = block;
    ctx->current = block;
    aligned_offset = 0; // New block starts at offset 0
  }

  void *ptr = (char *)ctx->current + sizeof(arena_block_t) + aligned_offset;
  ctx->current->offset = aligned_offset + size;
  ALLOC_UNPOISON(ptr, size);
  return ptr;
}

void *arena_realloc_fn(allo_t *self, void *ptr, size_t old_size,
                       size_t new_size) {
  arena_context_t *ctx = (arena_context_t *)self->_state;

  // We can grow/shrink in-place only if:
  // 1. The pointer points to the very last allocation made in this block
  void *block_start = (char *)ctx->current + sizeof(arena_block_t);
  void *last_alloc_end = (char *)block_start + ctx->current->offset;
  bool is_last_alloc = (char *)ptr + old_size == last_alloc_end;

  // 2. The pointer actually belongs to the current block
  bool belongs_to_current = (char *)ptr >= (char *)block_start;

  if (is_last_alloc && belongs_to_current) {
    size_t offset_without_this = (size_t)((char *)ptr - (char *)block_start);

    // 3. The new size still fits within the bounds of the current block.
    bool fits_in_current = offset_without_this + new_size <= ctx->current->size;

    if (fits_in_current) {
      // Update the offset to reflect the new size.
      ctx->current->offset = offset_without_this + new_size;
      if (new_size > old_size) {
        ALLOC_UNPOISON((char *)ptr + old_size, new_size - old_size);
      } else {
        ALLOC_POISON((char *)ptr + new_size, old_size - new_size);
      }
      return ptr;
    }
  }

  // Arenas don't reclaim space
  if (new_size <= old_size) {
    return ptr;
  }

  // Allocate new memory and "leak" the old one
  void *new_ptr = arena_alloc_fn(self, new_size);
  if (new_ptr == NULL) {
    return NULL;
  }

  memcpy(new_ptr, ptr, old_size);
  return new_ptr;
}

void arena_free_fn(allo_t *self, void *ptr, size_t size) {
  if (ptr && arena_contains_fn(self, ptr) == ALLO_CONTAINS_YES) {
    ALLOC_POISON(ptr, size);
  }
}

void arena_destroy_fn(allo_t *self) {
  arena_context_t *ctx = (arena_context_t *)self->_state;
  allo_t *child = ctx->child;
  arena_block_t *curr = ctx->first;
  while (curr) {
    arena_block_t *next = curr->next;
    allo_free(child, curr, sizeof(arena_block_t) + curr->size);
    curr = next;
  }
}

allo_error_t make_arena_allocator(allo_t *out, allo_t *child,
                                  size_t block_size) {
  if (!out || !child || block_size == 0)
    return ALLO_ERR_INVAL;

  *out = (allo_t){._alloc = arena_alloc_fn,
                  ._realloc = arena_realloc_fn,
                  ._free_mem = arena_free_fn,
                  ._destroy = arena_destroy_fn,
                  ._contains = arena_contains_fn};

  arena_context_t *ctx = (arena_context_t *)out->_state;
  ctx->child = child;
  ctx->block_size = block_size;
  ctx->first = new_arena_block(ctx->child, block_size);
  if (!ctx->first) {
    return ALLO_ERR_NOMEM;
  }
  ctx->current = ctx->first;

  return ALLO_OK;
}
