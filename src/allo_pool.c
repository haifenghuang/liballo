#include "allo.h"
#include "asan.h"

typedef struct pool_free_node {
  struct pool_free_node *next;
} pool_free_node_t;

typedef struct {
  void *buffer;
  size_t block_size;
  size_t total_blocks;
  pool_free_node_t *free_list;
  int own_buffer;
  allo_t *child;
} pool_context_t;

void *pool_alloc_fn(allo_t *self, size_t size) {
  if (size == 0) {
    return NULL;
  }
  pool_context_t *ctx = (pool_context_t *)self->_state;
  if (size > ctx->block_size) {
    return NULL; // Requested size exceeds pool block size
  }

  if (!ctx->free_list) {
    return NULL; // Out of memory
  }

  pool_free_node_t *node = ctx->free_list;
  ALLOC_UNPOISON(node, sizeof(pool_free_node_t));
  ctx->free_list = node->next;
  ALLOC_UNPOISON(node, ctx->block_size);
  return node;
}

void *pool_realloc_fn(allo_t *self, void *ptr, size_t old_size,
                      size_t new_size) {
  pool_context_t *ctx = (pool_context_t *)self->_state;
  (void)old_size;

  if (new_size <= ctx->block_size) {
    return ptr;
  }

  return NULL;
}

void pool_free_fn(allo_t *self, void *ptr) {
  if (!ptr)
    return;
  pool_context_t *ctx = (pool_context_t *)self->_state;
  pool_free_node_t *node = (pool_free_node_t *)ptr;
  ALLOC_UNPOISON(node, sizeof(pool_free_node_t));
  node->next = ctx->free_list;
  ctx->free_list = node;
  ALLOC_POISON(node, ctx->block_size);
}

void pool_destroy_fn(allo_t *self) {
  pool_context_t *ctx = (pool_context_t *)self->_state;
  if (ctx->own_buffer) {
    allo_free(ctx->child, ctx->buffer);
  }
}

allo_t make_pool_allocator(allo_t *child, void *buffer, size_t block_size,
                           size_t total_blocks) {
  allo_t a = {._alloc = pool_alloc_fn,
              ._realloc = pool_realloc_fn,
              ._free_mem = pool_free_fn,
              ._destroy = pool_destroy_fn};

  pool_context_t *ctx = (pool_context_t *)a._state;
  ctx->child = child;
  size_t min_size = sizeof(pool_free_node_t);
  size_t actual_block_size = block_size < min_size ? min_size : block_size;
  // Ensure block_size is aligned to pointer size
  ctx->block_size = (actual_block_size + 7) & ~7;
  ctx->total_blocks = total_blocks;
  ctx->own_buffer = (buffer == NULL);

  if (ctx->own_buffer) {
    ctx->buffer = allo_alloc(ctx->child, ctx->block_size * total_blocks);
  } else {
    ctx->buffer = buffer;
  }

  // Initialize free list
  ctx->free_list = (pool_free_node_t *)ctx->buffer;
  pool_free_node_t *curr = ctx->free_list;
  for (size_t i = 0; i < total_blocks - 1; ++i) {
    curr->next = (pool_free_node_t *)((char *)curr + ctx->block_size);
    curr = curr->next;
  }
  curr->next = NULL;

  ALLOC_POISON(ctx->buffer, ctx->block_size * ctx->total_blocks);

  return a;
}
