#include "allo.h"
#include "allo_asan.h"
#include "allo_assert.h"
#include "allo_mem.h"

#include <stdint.h>

typedef struct buddy_node {
  struct buddy_node *next;
  struct buddy_node *prev;
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

static inline void list_push(buddy_context_t *ctx, int level,
                             buddy_node_t *node) {
  node->next = ctx->free_lists[level];
  node->prev = NULL;
  if (ctx->free_lists[level]) {
    ctx->free_lists[level]->prev = node;
  }
  ctx->free_lists[level] = node;
}

static inline void list_remove(buddy_context_t *ctx, int level,
                               buddy_node_t *node) {
  if (node->prev) {
    node->prev->next = node->next;
  } else {
    ctx->free_lists[level] = node->next;
  }
  if (node->next) {
    node->next->prev = node->prev;
  }
}

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

static inline int get_order(size_t size) {
  if (size <= 1)
    return 0;
#if defined(__GNUC__) || defined(__clang__)
  if (sizeof(size_t) == sizeof(unsigned long long)) {
    return (int)(64 - __builtin_clzll((unsigned long long)size - 1));
  } else {
    return (int)(32 - __builtin_clz((unsigned int)size - 1));
  }
#else
  // Portable branchless fallback for non-GCC/Clang
  size_t val = size - 1;
  int order = 0;
  if (sizeof(size_t) == 8) {
    if (val & 0xFFFFFFFF00000000ULL) {
      val >>= 32;
      order += 32;
    }
  }
  if (val & 0xFFFF0000U) {
    val >>= 16;
    order += 16;
  }
  if (val & 0xFF00U) {
    val >>= 8;
    order += 8;
  }
  if (val & 0xF0U) {
    val >>= 4;
    order += 4;
  }
  if (val & 0xCU) {
    val >>= 2;
    order += 2;
  }
  if (val & 0x2U) {
    val >>= 1;
    order += 1;
  }
  if (val & 0x1U) {
    order += 1;
  }
  return order;
#endif
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
  list_remove(ctx, level, node);

  size_t index =
      ((char *)node - (char *)ctx->buffer) / (ctx->total_size >> level);
  clear_bit(ctx->bitset, node_index(level, index));

  while (level < target_level) {
    level++;
    index <<= 1;
    size_t buddy_size = ctx->total_size >> level;
    buddy_node_t *buddy = (buddy_node_t *)((char *)node + buddy_size);
    ALLOC_UNPOISON(buddy, sizeof(buddy_node_t));
    list_push(ctx, level, buddy);
    set_bit(ctx->bitset, node_index(level, index + 1));
    clear_bit(ctx->bitset, node_index(level, index));
  }

  // Record the level for this allocation
  size_t block_idx = ((char *)node - (char *)ctx->buffer) / ctx->min_block_size;
  ctx->block_orders[block_idx] = (uint8_t)target_level;

  ALLOC_UNPOISON(node, ctx->total_size >> target_level);
  return node;
}

allo_contains_t buddy_contains_fn(allo_t *self, void *ptr) {
  buddy_context_t *ctx = (buddy_context_t *)self->_state;
  return (ptr >= ctx->buffer &&
          (char *)ptr < (char *)ctx->buffer + ctx->total_size)
             ? ALLO_CONTAINS_YES
             : ALLO_CONTAINS_NO;
}

void buddy_free_fn(allo_t *self, void *ptr, size_t size) {
  if (!ptr)
    return;
  ALLO_ASSERT(buddy_contains_fn(self, ptr) == ALLO_CONTAINS_YES);
  buddy_context_t *ctx = (buddy_context_t *)self->_state;

  int order = get_order(size);
  int min_order = get_order(ctx->min_block_size);
  if (order < min_order)
    order = min_order;

  int level = ctx->max_order - order;
  size_t offset = (char *)ptr - (char *)ctx->buffer;
  size_t index = offset / (ctx->total_size >> level);

  // Poison the whole block being freed
  ALLOC_POISON(ptr, ctx->total_size >> level);

  while (level >= 0) {
    size_t buddy_idx = index ^ 1;
    if (level == 0 || get_bit(ctx->bitset, node_index(level, buddy_idx)) == 0) {
      set_bit(ctx->bitset, node_index(level, index));
      buddy_node_t *node = (buddy_node_t *)((char *)ctx->buffer +
                                            index * (ctx->total_size >> level));
      // Unpoison only the header for the free list
      ALLOC_UNPOISON(node, sizeof(buddy_node_t));
      list_push(ctx, level, node);
      break;
    }

    // Buddy is free, merge it
    void *buddy_ptr =
        (char *)ctx->buffer + buddy_idx * (ctx->total_size >> level);
    buddy_node_t *buddy_node = (buddy_node_t *)buddy_ptr;
    list_remove(ctx, level, buddy_node);
    // Poison the header of the buddy we are removing from the free list
    ALLOC_POISON(buddy_node, sizeof(buddy_node_t));

    clear_bit(ctx->bitset, node_index(level, buddy_idx));

    level--;
    index >>= 1;
  }
}

void *buddy_realloc_fn(allo_t *self, void *ptr, size_t old_size,
                       size_t new_size) {
  ALLO_ASSERT(ptr == NULL || buddy_contains_fn(self, ptr) == ALLO_CONTAINS_YES);
  if (!ptr)
    return buddy_alloc_fn(self, new_size);
  if (new_size == 0) {
    buddy_free_fn(self, ptr, old_size);
    return NULL;
  }

  void *new_ptr = buddy_alloc_fn(self, new_size);
  if (!new_ptr)
    return NULL;

  size_t copy_size = old_size < new_size ? old_size : new_size;
  allo_memcpy(new_ptr, ptr, copy_size);
  buddy_free_fn(self, ptr, old_size);
  return new_ptr;
}

void buddy_destroy_fn(allo_t *self) {
  buddy_context_t *ctx = (buddy_context_t *)self->_state;
  if (ctx->own_buffer && ctx->buffer) {
    allo_free(ctx->child, ctx->buffer, ctx->total_size);
  }
  if (ctx->free_lists) {
    int num_levels = ctx->max_order - get_order(ctx->min_block_size) + 1;
    size_t free_lists_count = (size_t)num_levels + 1;
    allo_free(ctx->child, ctx->free_lists,
              free_lists_count * sizeof(buddy_node_t *));
  }
  if (ctx->bitset) {
    size_t max_nodes = (ctx->total_size / ctx->min_block_size) * 2;
    size_t bit_bytes = (max_nodes + 7) / 8;
    allo_free(ctx->child, ctx->bitset, bit_bytes);
  }
  if (ctx->block_orders) {
    size_t block_orders_len = ctx->total_size / ctx->min_block_size;
    allo_free(ctx->child, ctx->block_orders, block_orders_len);
  }
}

allo_error_t make_buddy_allocator(allo_t *out, allo_t *child, void *buffer,
                                  size_t size) {
  if (!out || !child || size == 0) {
    return ALLO_ERR_INVAL;
  }

  *out = (allo_t){._alloc = buddy_alloc_fn,
                  ._realloc = buddy_realloc_fn,
                  ._free_mem = buddy_free_fn,
                  ._destroy = buddy_destroy_fn,
                  ._contains = buddy_contains_fn};

  buddy_context_t *ctx = (buddy_context_t *)out->_state;
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
    if (!ctx->buffer)
      return ALLO_ERR_NOMEM;
  } else {
    ctx->buffer = buffer;
  }

  int num_levels = ctx->max_order - get_order(ctx->min_block_size) + 1;
  size_t free_lists_count = (size_t)num_levels + 1;

  ctx->free_lists = (buddy_node_t **)allo_calloc(ctx->child, free_lists_count,
                                                 sizeof(buddy_node_t *));
  if (!ctx->free_lists) {
    if (ctx->own_buffer)
      allo_free(ctx->child, ctx->buffer, ctx->total_size);
    return ALLO_ERR_NOMEM;
  }

  size_t max_nodes = (ctx->total_size / ctx->min_block_size) * 2;
  size_t bit_bytes = (max_nodes + 7) / 8;
  size_t block_orders_len = ctx->total_size / ctx->min_block_size;

  ctx->bitset = (uint8_t *)allo_calloc(ctx->child, bit_bytes, 1);
  if (!ctx->bitset) {
    allo_free(ctx->child, ctx->free_lists,
              free_lists_count * sizeof(buddy_node_t *));
    if (ctx->own_buffer)
      allo_free(ctx->child, ctx->buffer, ctx->total_size);
    return ALLO_ERR_NOMEM;
  }

  ctx->block_orders = (uint8_t *)allo_calloc(ctx->child, block_orders_len, 1);
  if (!ctx->block_orders) {
    allo_free(ctx->child, ctx->bitset, bit_bytes);
    allo_free(ctx->child, ctx->free_lists,
              free_lists_count * sizeof(buddy_node_t *));
    if (ctx->own_buffer)
      allo_free(ctx->child, ctx->buffer, ctx->total_size);
    return ALLO_ERR_NOMEM;
  }

  list_push(ctx, 0, (buddy_node_t *)ctx->buffer);
  set_bit(ctx->bitset, node_index(0, 0));

  ALLOC_POISON(ctx->buffer, ctx->total_size);
  ALLOC_UNPOISON(ctx->buffer, sizeof(buddy_node_t));

  return ALLO_OK;
}
