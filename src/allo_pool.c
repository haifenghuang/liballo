#include "allo.h"
#include "asan.h"
#include <assert.h>

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

static_assert(sizeof(pool_context_t) <= ALLO_MAX_ALLOCATOR_CTX_SIZE,
              "Pool allocator context exceeds maximum size");

allo_contains_t pool_contains_fn(allo_t *self, void *ptr) {
  pool_context_t *ctx = (pool_context_t *)self->_state;
  size_t total_size = ctx->block_size * ctx->total_blocks;
  return (ptr >= ctx->buffer && (char *)ptr < (char *)ctx->buffer + total_size)
             ? ALLO_CONTAINS_YES
             : ALLO_CONTAINS_NO;
}

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
  assert(ptr == NULL || pool_contains_fn(self, ptr) == ALLO_CONTAINS_YES);
  pool_context_t *ctx = (pool_context_t *)self->_state;
  (void)old_size;

  if (new_size <= ctx->block_size) {
    return ptr;
  }

  return NULL;
}

void pool_free_fn(allo_t *self, void *ptr, size_t size) {
  if (!ptr)
    return;
  assert(pool_contains_fn(self, ptr) == ALLO_CONTAINS_YES);
  (void)size;
  pool_context_t *ctx = (pool_context_t *)self->_state;
  pool_free_node_t *node = (pool_free_node_t *)ptr;

  ALLOC_POISON(node, ctx->block_size);
  ALLOC_UNPOISON(node, sizeof(pool_free_node_t));

  node->next = ctx->free_list;
  ctx->free_list = node;
}

void pool_destroy_fn(allo_t *self) {
  pool_context_t *ctx = (pool_context_t *)self->_state;
  if (ctx->own_buffer) {
    allo_free(ctx->child, ctx->buffer, ctx->block_size * ctx->total_blocks);
  }
}

allo_error_t make_pool_allocator(allo_t *out, allo_t *child, void *buffer,
                                 size_t block_size, size_t total_blocks) {
  if (!out || (buffer == NULL && child == NULL) || block_size == 0 ||
      total_blocks == 0)
    return ALLO_ERR_INVAL;

  *out = (allo_t){._alloc = pool_alloc_fn,
                  ._realloc = pool_realloc_fn,
                  ._free_mem = pool_free_fn,
                  ._destroy = pool_destroy_fn,
                  ._contains = pool_contains_fn};

  pool_context_t *ctx = (pool_context_t *)out->_state;
  ctx->child = child;
  size_t min_size = sizeof(pool_free_node_t);
  size_t actual_block_size = block_size < min_size ? min_size : block_size;
  // Ensure block_size is aligned to pointer size
  ctx->block_size = ALLO_ALIGN_UP(actual_block_size, 8);
  ctx->total_blocks = total_blocks;
  ctx->own_buffer = (buffer == NULL);

  if (ctx->own_buffer) {
    ctx->buffer = allo_alloc(ctx->child, ctx->block_size * total_blocks);
    if (!ctx->buffer)
      return ALLO_ERR_NOMEM;
  } else {
    ctx->buffer = buffer;
  }

  ALLOC_POISON(ctx->buffer, ctx->block_size * total_blocks);

  // Initialize free list
  ctx->free_list = (pool_free_node_t *)ctx->buffer;
  pool_free_node_t *curr = ctx->free_list;
  for (size_t i = 0; i < total_blocks; ++i) {
    ALLOC_UNPOISON(curr, sizeof(pool_free_node_t));
    if (i < total_blocks - 1) {
      curr->next = (pool_free_node_t *)((char *)curr + ctx->block_size);
      curr = (pool_free_node_t *)((char *)curr + ctx->block_size);
    } else {
      curr->next = NULL;
    }
  }

  return ALLO_OK;
}
