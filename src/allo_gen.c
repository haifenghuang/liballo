#include "allo.h"
#include "allo_assert.h"
#include "allo_mem.h"

#define POOL_COUNT 8
#define BUDDY_TOTAL_SIZE (16UL * 1024UL * 1024UL) // 16MB for buddy
#define SMALL_OBJ_LIMIT 2048UL
#define MEDIUM_OBJ_LIMIT (1024UL * 1024UL)

static const size_t pool_sizes[POOL_COUNT] = {16,  32,  64,   128,
                                              256, 512, 1024, 2048};
static const size_t pool_blocks[POOL_COUNT] = {4096, 2048, 1024, 512,
                                               256,  128,  64,   32};

typedef struct {
  allo_t page;
  allo_t buddy;
  allo_t pools[POOL_COUNT];
} gen_state_t;

typedef struct {
  gen_state_t *state;
} gen_context_t;

static inline int get_pool_index(size_t size) {
  for (int i = 0; i < POOL_COUNT; i++) {
    if (size <= pool_sizes[i]) {
      return i;
    }
  }
  return -1;
}

static void *gen_alloc_fn(allo_t *self, size_t size) {
  if (size == 0)
    return NULL;
  gen_context_t *ctx = (gen_context_t *)self->_state;
  gen_state_t *s = ctx->state;

  if (size <= SMALL_OBJ_LIMIT) {
    int idx = get_pool_index(size);
    return allo_alloc(&s->pools[idx], size);
  } else if (size <= MEDIUM_OBJ_LIMIT) {
    return allo_alloc(&s->buddy, size);
  } else {
    return allo_alloc(&s->page, size);
  }
}

static void *gen_realloc_fn(allo_t *self, void *ptr, size_t old_size,
                            size_t new_size) {
  if (!ptr)
    return gen_alloc_fn(self, new_size);
  if (new_size == 0) {
    self->_free_mem(self, ptr, old_size);
    return NULL;
  }

  gen_context_t *ctx = (gen_context_t *)self->_state;
  gen_state_t *s = ctx->state;

  // If it stays in the same tier, try tier-specific realloc
  if (old_size <= SMALL_OBJ_LIMIT && new_size <= SMALL_OBJ_LIMIT) {
    int old_idx = get_pool_index(old_size);
    int new_idx = get_pool_index(new_size);
    if (old_idx == new_idx) {
      return allo_realloc(&s->pools[old_idx], ptr, old_size, new_size);
    }
  } else if (old_size > SMALL_OBJ_LIMIT && old_size <= MEDIUM_OBJ_LIMIT &&
             new_size > SMALL_OBJ_LIMIT && new_size <= MEDIUM_OBJ_LIMIT) {
    return allo_realloc(&s->buddy, ptr, old_size, new_size);
  } else if (old_size > MEDIUM_OBJ_LIMIT && new_size > MEDIUM_OBJ_LIMIT) {
    return allo_realloc(&s->page, ptr, old_size, new_size);
  }

  // Crossing tiers or moving between pools: manual move
  void *new_ptr = gen_alloc_fn(self, new_size);
  if (!new_ptr)
    return NULL;

  size_t copy_size = old_size < new_size ? old_size : new_size;
  allo_memcpy(new_ptr, ptr, copy_size);
  self->_free_mem(self, ptr, old_size);
  return new_ptr;
}

static void gen_free_fn(allo_t *self, void *ptr, size_t size) {
  if (!ptr)
    return;
  gen_context_t *ctx = (gen_context_t *)self->_state;
  gen_state_t *s = ctx->state;

  if (size <= SMALL_OBJ_LIMIT) {
    int idx = get_pool_index(size);
    allo_free(&s->pools[idx], ptr, size);
  } else if (size <= MEDIUM_OBJ_LIMIT) {
    allo_free(&s->buddy, ptr, size);
  } else {
    allo_free(&s->page, ptr, size);
  }
}

static void gen_destroy_fn(allo_t *self) {
  gen_context_t *ctx = (gen_context_t *)self->_state;
  gen_state_t *s = ctx->state;

  // Pools and Buddy are backed by child allocators, we should destroy them
  // but they don't destroy their child.
  // Actually, Pools are backed by Buddy, Buddy by Page.
  for (int i = 0; i < POOL_COUNT; i++) {
    allo_destroy(&s->pools[i]);
  }
  allo_destroy(&s->buddy);

  // Save page allocator to free the state itself
  allo_t page = s->page;
  allo_destroy(&page);
}

static allo_contains_t gen_contains_fn(allo_t *self, void *ptr) {
  gen_context_t *ctx = (gen_context_t *)self->_state;
  gen_state_t *s = ctx->state;

  // Check tiers from smallest to largest
  for (int i = 0; i < POOL_COUNT; i++) {
    if (allo_contains(&s->pools[i], ptr) == ALLO_CONTAINS_YES)
      return ALLO_CONTAINS_YES;
  }
  if (allo_contains(&s->buddy, ptr) == ALLO_CONTAINS_YES)
    return ALLO_CONTAINS_YES;
  if (allo_contains(&s->page, ptr) == ALLO_CONTAINS_YES)
    return ALLO_CONTAINS_YES;

  return ALLO_CONTAINS_NO;
}

allo_error_t make_gen_allocator(allo_t *out) {
  if (!out)
    return ALLO_ERR_INVAL;

  // Use page allocator to bootstrap the internal state
  allo_t page;
  allo_error_t err = make_page_allocator(&page);
  if (err != ALLO_OK)
    return err;

  gen_state_t *s = allo_alloc(&page, sizeof(gen_state_t));
  if (!s) {
    allo_destroy(&page);
    return ALLO_ERR_NOMEM;
  }

  s->page = page;

  // Create buddy allocator backed by page allocator
  err = make_buddy_allocator(&s->buddy, &s->page, NULL, BUDDY_TOTAL_SIZE);
  if (err != ALLO_OK) {
    allo_free(&s->page, s, sizeof(gen_state_t));
    allo_destroy(&s->page);
    return err;
  }

  // Create pools backed by buddy allocator
  for (int i = 0; i < POOL_COUNT; i++) {
    err = make_pool_allocator(&s->pools[i], &s->buddy, NULL, pool_sizes[i],
                              pool_blocks[i]);
    if (err != ALLO_OK) {
      // Cleanup already created pools
      for (int j = 0; j < i; j++)
        allo_destroy(&s->pools[j]);
      allo_destroy(&s->buddy);
      allo_free(&s->page, s, sizeof(gen_state_t));
      allo_destroy(&s->page);
      return err;
    }
  }

  *out = (allo_t){._alloc = gen_alloc_fn,
                  ._realloc = gen_realloc_fn,
                  ._free_mem = gen_free_fn,
                  ._destroy = gen_destroy_fn,
                  ._contains = gen_contains_fn};

  gen_context_t *ctx = (gen_context_t *)out->_state;
  ctx->state = s;

  return ALLO_OK;
}
