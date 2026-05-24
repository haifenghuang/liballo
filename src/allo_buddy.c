#include "allo.h"
#include "asan.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct buddy_node {
  struct buddy_node *next;
} buddy_node_t;

typedef struct {
  void *buffer;
  size_t total_size;
  size_t min_block_size;
  int max_order;
  buddy_node_t **free_lists;
  uint8_t *bitset;
  uint8_t *block_orders;
  int own_buffer;
  allo_t *child;
} buddy_context_t;

static_assert(sizeof(buddy_context_t) <= ALLO_MAX_ALLOCATOR_CTX_SIZE,
              "Buddy allocator context exceeds maximum size");

static inline size_t node_index(int level, size_t index) {
  return (1ULL << level) + index - 1;
}

static inline void set_bit(uint8_t *bitset, size_t bit) {
  bitset[bit / 8] |= (1 << (bit % 8));
}

static inline void clear_bit(uint8_t *bitset, size_t bit) {
  bitset[bit / 8] &= ~(1 << (bit % 8));
}

static inline int get_bit(uint8_t *bitset, size_t bit) {
  return (bitset[bit / 8] >> (bit % 8)) & 1;
}

static int get_order(size_t size) {
  int order = 0;
  size_t s = 1;
  while (s < size) {
    s <<= 1;
    order++;
  }
  return order;
}

void *buddy_alloc_fn(allo_t *self, size_t size) {
  if (size == 0)
    return NULL;
  buddy_context_t *ctx = (buddy_context_t *)self->_state;

  int order = get_order(size);
  int min_order = get_order(ctx->min_block_size);
  if (order < min_order)
    order = min_order;

  int target_level = ctx->max_order - order;
  if (target_level < 0)
    return NULL;

  int level = target_level;
  while (level >= 0 && ctx->free_lists[level] == NULL) {
    level--;
  }

  if (level < 0)
    return NULL;

  buddy_node_t *node = ctx->free_lists[level];
  ALLOC_UNPOISON(node, sizeof(buddy_node_t));
  ctx->free_lists[level] = node->next;

  size_t index =
      ((char *)node - (char *)ctx->buffer) / (ctx->total_size >> level);
  clear_bit(ctx->bitset, node_index(level, index));

  while (level < target_level) {
    level++;
    index <<= 1;
    size_t buddy_size = ctx->total_size >> level;
    buddy_node_t *buddy = (buddy_node_t *)((char *)node + buddy_size);
    ALLOC_UNPOISON(buddy, sizeof(buddy_node_t));
    buddy->next = ctx->free_lists[level];
    ctx->free_lists[level] = buddy;
    set_bit(ctx->bitset, node_index(level, index + 1));
    clear_bit(ctx->bitset, node_index(level, index));
  }

  // Record the level for this allocation
  size_t block_idx = ((char *)node - (char *)ctx->buffer) / ctx->min_block_size;
  ctx->block_orders[block_idx] = (uint8_t)target_level;

  ALLOC_UNPOISON(node, ctx->total_size >> target_level);
  return node;
}

void buddy_free_fn(allo_t *self, void *ptr) {
  if (!ptr)
    return;
  buddy_context_t *ctx = (buddy_context_t *)self->_state;

  size_t offset = (char *)ptr - (char *)ctx->buffer;
  size_t block_idx = offset / ctx->min_block_size;
  int level = ctx->block_orders[block_idx];
  size_t index = offset / (ctx->total_size >> level);

  ALLOC_POISON(ptr, ctx->total_size >> level);

  while (level >= 0) {
    size_t buddy_idx = index ^ 1;
    if (level == 0 || get_bit(ctx->bitset, node_index(level, buddy_idx)) == 0) {
      set_bit(ctx->bitset, node_index(level, index));
      buddy_node_t *node = (buddy_node_t *)((char *)ctx->buffer +
                                            index * (ctx->total_size >> level));
      ALLOC_UNPOISON(node, sizeof(buddy_node_t));
      node->next = ctx->free_lists[level];
      ctx->free_lists[level] = node;
      break;
    }

    buddy_node_t **curr = &ctx->free_lists[level];
    void *buddy_ptr =
        (char *)ctx->buffer + buddy_idx * (ctx->total_size >> level);
    while (*curr && *curr != (buddy_node_t *)buddy_ptr) {
      ALLOC_UNPOISON(*curr, sizeof(buddy_node_t));
      buddy_node_t *prev = *curr;
      curr = &(prev->next);
    }
    if (*curr) {
      buddy_node_t *to_remove = *curr;
      ALLOC_UNPOISON(to_remove, sizeof(buddy_node_t));
      *curr = to_remove->next;
      ALLOC_POISON(to_remove, sizeof(buddy_node_t));
    }
    clear_bit(ctx->bitset, node_index(level, buddy_idx));

    level--;
    index >>= 1;
  }
}

void *buddy_realloc_fn(allo_t *self, void *ptr, size_t old_size,
                       size_t new_size) {
  if (!ptr)
    return buddy_alloc_fn(self, new_size);
  if (new_size == 0) {
    buddy_free_fn(self, ptr);
    return NULL;
  }

  void *new_ptr = buddy_alloc_fn(self, new_size);
  if (!new_ptr)
    return NULL;

  size_t copy_size = old_size < new_size ? old_size : new_size;
  memcpy(new_ptr, ptr, copy_size);
  buddy_free_fn(self, ptr);
  return new_ptr;
}

void buddy_destroy_fn(allo_t *self) {
  buddy_context_t *ctx = (buddy_context_t *)self->_state;
  if (ctx->own_buffer) {
    allo_free(ctx->child, ctx->buffer);
  }
  free(ctx->free_lists);
  free(ctx->bitset);
  free(ctx->block_orders);
}

allo_t make_buddy_allocator(allo_t *child, void *buffer, size_t size) {
  allo_t a = {._alloc = buddy_alloc_fn,
              ._realloc = buddy_realloc_fn,
              ._free_mem = buddy_free_fn,
              ._destroy = buddy_destroy_fn};

  buddy_context_t *ctx = (buddy_context_t *)a._state;
  ctx->child = child;

  size_t actual_size = 1;
  while (actual_size < size)
    actual_size <<= 1;
  ctx->total_size = actual_size;
  ctx->min_block_size = 32;
  ctx->max_order = get_order(ctx->total_size);
  ctx->own_buffer = (buffer == NULL);

  if (ctx->own_buffer) {
    ctx->buffer = allo_alloc(ctx->child, ctx->total_size);
  } else {
    ctx->buffer = buffer;
  }

  int num_levels = ctx->max_order - get_order(ctx->min_block_size) + 1;
  ctx->free_lists = calloc(num_levels + 1, sizeof(buddy_node_t *));

  size_t max_nodes = (ctx->total_size / ctx->min_block_size) * 2;
  ctx->bitset = calloc((max_nodes + 7) / 8, 1);
  ctx->block_orders = calloc(ctx->total_size / ctx->min_block_size, 1);

  ctx->free_lists[0] = (buddy_node_t *)ctx->buffer;
  ctx->free_lists[0]->next = NULL;
  set_bit(ctx->bitset, node_index(0, 0));

  ALLOC_POISON(ctx->buffer, ctx->total_size);

  return a;
}
