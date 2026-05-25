#define _GNU_SOURCE
#include "allo.h"
#include "allo_asan.h"
#include "allo_assert.h"
#include "allo_mem.h"

#ifdef ALLO_NOSTDLIB
  #include "allo_page_x86.c"
#else
  #include "allo_page_posix.c"
#endif

typedef struct {
  void *addr;
  size_t size;
} page_entry_t;

typedef struct {
  size_t page_size;
  page_entry_t *registry;
  size_t count;
  size_t capacity;
} page_context_t;

static_assert(sizeof(page_context_t) <= ALLO_MAX_ALLOCATOR_CTX_SIZE,
              "Page allocator context exceeds maximum size");

static bool registry_ensure_capacity(page_context_t *ctx) {
  if (ctx->count == ctx->capacity) {
    size_t old_size = ctx->capacity * sizeof(page_entry_t);
    size_t new_size = old_size + ctx->page_size;
    void *new_reg = os_mremap(ctx->registry, old_size, new_size);
    if (!new_reg) {
      return false;
    }
    ctx->registry = (page_entry_t *)new_reg;
    ctx->capacity = new_size / sizeof(page_entry_t);
  }
  return true;
}

static void registry_add(page_context_t *ctx, void *addr, size_t size) {
  ctx->registry[ctx->count].addr = addr;
  ctx->registry[ctx->count].size = size;
  ctx->count++;
}

static void registry_remove(page_context_t *ctx, void *addr) {
  for (size_t i = 0; i < ctx->count; i++) {
    if (ctx->registry[i].addr == addr) {
      // Swap with last element to keep deletion O(1)
      ctx->registry[i] = ctx->registry[ctx->count - 1];
      ctx->count--;
      return;
    }
  }
}

void *page_alloc_fn(allo_t *self, size_t size) {
  if (size == 0) {
    return NULL;
  }
  page_context_t *ctx = (page_context_t *)self->_state;

  if (!registry_ensure_capacity(ctx)) {
    return NULL;
  }

  size_t rounded_size = (size + ctx->page_size - 1) & ~(ctx->page_size - 1);
  void *ptr = os_mmap(rounded_size);
  if (!ptr) {
    return NULL;
  }

  registry_add(ctx, ptr, rounded_size);

  ALLOC_POISON(ptr, rounded_size);
  ALLOC_UNPOISON(ptr, size);
  return ptr;
}

void *page_realloc_fn(allo_t *self, void *ptr, size_t old_size,
                      size_t new_size) {
  if (!ptr) {
    return page_alloc_fn(self, new_size);
  }

  page_context_t *ctx = (page_context_t *)self->_state;

  size_t rounded_old = (old_size + ctx->page_size - 1) & ~(ctx->page_size - 1);
  size_t rounded_new = (new_size + ctx->page_size - 1) & ~(ctx->page_size - 1);

  if (rounded_old == rounded_new) {
    if (new_size > old_size) {
      ALLOC_UNPOISON((char *)ptr + old_size, new_size - old_size);
    } else {
      ALLOC_POISON((char *)ptr + new_size, old_size - new_size);
    }
    return ptr;
  }

  // Unpoison everything before realloc to avoid issues with mremap
  ALLOC_UNPOISON(ptr, rounded_old);
  void *new_ptr = os_mremap(ptr, rounded_old, rounded_new);

  if (!new_ptr) {
    // Restore poisoning on failure
    ALLOC_POISON((char *)ptr + old_size, rounded_old - old_size);
    return NULL;
  }

  // Update registry
  for (size_t i = 0; i < ctx->count; i++) {
    if (ctx->registry[i].addr == ptr) {
      ctx->registry[i].addr = new_ptr;
      ctx->registry[i].size = rounded_new;
      break;
    }
  }

  ALLOC_POISON(new_ptr, rounded_new);
  ALLOC_UNPOISON(new_ptr, new_size);
  return new_ptr;
}

void page_free_fn(allo_t *self, void *ptr, size_t size) {
  if (ptr == NULL) {
    return;
  }
  page_context_t *ctx = (page_context_t *)self->_state;
  size_t rounded_size = (size + ctx->page_size - 1) & ~(ctx->page_size - 1);

  registry_remove(ctx, ptr);

  // Unpoison before unmapping
  ALLOC_UNPOISON(ptr, rounded_size);
  os_munmap(ptr, rounded_size);
}

void page_destroy_fn(allo_t *self) {
  page_context_t *ctx = (page_context_t *)self->_state;
  // Reclaim all leaked pages
  for (size_t i = 0; i < ctx->count; i++) {
    os_munmap(ctx->registry[i].addr, ctx->registry[i].size);
  }
  // Free the registry itself
  os_munmap(ctx->registry, ctx->capacity * sizeof(page_entry_t));
}

allo_contains_t page_contains_fn(allo_t *self, void *ptr) {
  page_context_t *ctx = (page_context_t *)self->_state;
  for (size_t i = 0; i < ctx->count; i++) {
    if (ptr >= ctx->registry[i].addr &&
        (char *)ptr < (char *)ctx->registry[i].addr + ctx->registry[i].size) {
      return ALLO_CONTAINS_YES;
    }
  }
  return ALLO_CONTAINS_NO;
}

allo_error_t make_page_allocator(allo_t *out) {
  if (!out)
    return ALLO_ERR_INVAL;

  *out = (allo_t){._alloc = page_alloc_fn,
                  ._realloc = page_realloc_fn,
                  ._free_mem = page_free_fn,
                  ._destroy = page_destroy_fn,
                  ._contains = page_contains_fn};

  page_context_t *ctx = (page_context_t *)out->_state;
  ctx->page_size = os_get_page_size();
  ctx->count = 0;
  ctx->capacity = ctx->page_size / sizeof(page_entry_t);
  ctx->registry = (page_entry_t *)os_mmap(ctx->page_size);

  if (!ctx->registry) {
    return ALLO_ERR_NOMEM;
  }

  return ALLO_OK;
}
