#include "allo.h"
#include "asan.h"
#include <assert.h>

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

  size_t aligned_offset = (ctx->current->offset + 7) & ~7;

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

  // Check if it's the last allocation in the current block
  void *current_block_data = (char *)ctx->current + sizeof(arena_block_t);
  if ((char *)ptr >= (char *)current_block_data &&
      (char *)ptr < (char *)current_block_data + ctx->current->size) {
    if ((char *)ptr + old_size ==
        (char *)current_block_data + ctx->current->offset) {
      size_t base_offset = (size_t)((char *)ptr - (char *)current_block_data);
      if (base_offset + new_size <= ctx->current->size) {
        ctx->current->offset = base_offset + new_size;
        if (new_size > old_size) {
          ALLOC_UNPOISON((char *)ptr + old_size, new_size - old_size);
        } else {
          ALLOC_POISON((char *)ptr + new_size, old_size - new_size);
        }
        return ptr;
      }
    }
  }

  // Fallback: if shrinking, just return (we can't reclaim anyway).
  // If growing, allocate new and copy.
  if (new_size <= old_size) {
    return ptr;
  }

  void *new_ptr = arena_alloc_fn(self, new_size);
  if (!new_ptr) {
    return NULL;
  }

  memcpy(new_ptr, ptr, old_size);
  return new_ptr;
}

void arena_free_fn(allo_t *self, void *ptr) {
  (void)self;
  (void)ptr;
  // Individual free is not supported in Arena
}

void arena_destroy_fn(allo_t *self) {
  arena_context_t *ctx = (arena_context_t *)self->_state;
  allo_t *child = ctx->child;
  arena_block_t *curr = ctx->first;
  while (curr) {
    arena_block_t *next = curr->next;
    allo_free(child, curr);
    curr = next;
  }
}

allo_t make_arena_allocator(allo_t *child, size_t block_size) {
  allo_t a = {._alloc = arena_alloc_fn,
              ._realloc = arena_realloc_fn,
              ._free_mem = arena_free_fn,
              ._destroy = arena_destroy_fn};

  arena_context_t *ctx = (arena_context_t *)a._state;
  ctx->child = child;
  ctx->block_size = block_size;
  ctx->first = new_arena_block(ctx->child, block_size);
  if (!ctx->first) {
    return (allo_t){0};
  }
  ctx->current = ctx->first;

  return a;
}
