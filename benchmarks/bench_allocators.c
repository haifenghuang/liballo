// Workaround for strict C11 which disables 'asm'
#ifndef __cplusplus
  #define asm __asm__
#endif

#include "allo.h"
#include "ubench.h"
#include <stdlib.h>
#include <string.h>

#define TORTURE_COUNT 256
#define RACE_COUNT 512

// --- Fixtures ---

// Libc
struct libc_fixture {
  allo_t a;
};
UBENCH_F_SETUP(libc_fixture) {
  make_c_allocator(&ubench_fixture->a);
}
UBENCH_F_TEARDOWN(libc_fixture) {
  allo_destroy(&ubench_fixture->a);
}

// Page
struct page_fixture {
  allo_t a;
};
UBENCH_F_SETUP(page_fixture) {
  make_page_allocator(&ubench_fixture->a);
}
UBENCH_F_TEARDOWN(page_fixture) {
  allo_destroy(&ubench_fixture->a);
}

// Arena
struct arena_fixture {
  allo_t child;
  allo_t a;
};
UBENCH_F_SETUP(arena_fixture) {
  make_c_allocator(&ubench_fixture->child);
  size_t size = 256 * 1024 * 1024;
  make_arena_allocator(&ubench_fixture->a, &ubench_fixture->child, size);
  void *p = allo_alloc(&ubench_fixture->a, size - 1024);
  if (p)
    memset(p, 0, size - 1024);
  allo_destroy(&ubench_fixture->a);
  make_arena_allocator(&ubench_fixture->a, &ubench_fixture->child, size);
}
UBENCH_F_TEARDOWN(arena_fixture) {
  allo_destroy(&ubench_fixture->a);
  allo_destroy(&ubench_fixture->child);
}

// Pool
struct pool_fixture {
  allo_t child;
  allo_t a;
};
UBENCH_F_SETUP(pool_fixture) {
  make_c_allocator(&ubench_fixture->child);
  size_t block_size = 64;
  size_t num_blocks = 10000;
  make_pool_allocator(&ubench_fixture->a, &ubench_fixture->child, NULL,
                      block_size, num_blocks);
  void *p = allo_alloc(&ubench_fixture->a, block_size * num_blocks);
  if (p)
    memset(p, 0, block_size * num_blocks);
  allo_destroy(&ubench_fixture->a);
  make_pool_allocator(&ubench_fixture->a, &ubench_fixture->child, NULL,
                      block_size, num_blocks);
}
UBENCH_F_TEARDOWN(pool_fixture) {
  allo_destroy(&ubench_fixture->a);
  allo_destroy(&ubench_fixture->child);
}

// Buddy
struct buddy_fixture {
  allo_t child;
  allo_t a;
  size_t sizes[TORTURE_COUNT];
  size_t realloc_sizes[TORTURE_COUNT];
};
UBENCH_F_SETUP(buddy_fixture) {
  srand(42);
  make_c_allocator(&ubench_fixture->child);
  size_t total_size = 64 * 1024 * 1024;
  make_buddy_allocator(&ubench_fixture->a, &ubench_fixture->child, NULL,
                       total_size);
  void *p = allo_alloc(&ubench_fixture->a, total_size);
  if (p) {
    memset(p, 0, total_size);
    allo_free(&ubench_fixture->a, p, total_size);
  }
  for (int i = 0; i < TORTURE_COUNT; i++) {
    ubench_fixture->sizes[i] = (size_t)(rand() % 4096) + 1;
    ubench_fixture->realloc_sizes[i] = (size_t)(rand() % 2048) + 1;
  }
}
UBENCH_F_TEARDOWN(buddy_fixture) {
  allo_destroy(&ubench_fixture->a);
  allo_destroy(&ubench_fixture->child);
}

// Buffer
struct buffer_fixture {
  char *buf;
  allo_t a;
};
UBENCH_F_SETUP(buffer_fixture) {
  size_t size = 256 * 1024 * 1024;
  ubench_fixture->buf = malloc(size);
  memset(ubench_fixture->buf, 0, size);
  make_fixed_buf_allocator(&ubench_fixture->a, ubench_fixture->buf, size);
}
UBENCH_F_TEARDOWN(buffer_fixture) {
  allo_destroy(&ubench_fixture->a);
  free(ubench_fixture->buf);
}

// --- Basic Alloc/Free Benchmarks ---

#define DEFINE_ALLOC_FREE_BENCHMARK(NAME, FIXTURE, SIZE)                       \
  UBENCH_F(FIXTURE, alloc_free_##NAME) {                                       \
    void *ptr = allo_alloc(&ubench_fixture->a, SIZE);                          \
    ubench_do_nothing(ptr);                                                    \
    allo_free(&ubench_fixture->a, ptr, SIZE);                                  \
  }

DEFINE_ALLOC_FREE_BENCHMARK(8b, libc_fixture, 8)
DEFINE_ALLOC_FREE_BENCHMARK(1kb, libc_fixture, 1024)
DEFINE_ALLOC_FREE_BENCHMARK(64kb, libc_fixture, 64 * 1024)

DEFINE_ALLOC_FREE_BENCHMARK(8b, buddy_fixture, 8)
DEFINE_ALLOC_FREE_BENCHMARK(1kb, buddy_fixture, 1024)
DEFINE_ALLOC_FREE_BENCHMARK(64kb, buddy_fixture, 64 * 1024)

DEFINE_ALLOC_FREE_BENCHMARK(64b, pool_fixture, 64)

#define DEFINE_ALLOC_BENCHMARK(NAME, FIXTURE, SIZE)                            \
  UBENCH_F(FIXTURE, alloc_only_##NAME) {                                       \
    void *ptr = allo_alloc(&ubench_fixture->a, SIZE);                          \
    ubench_do_nothing(ptr);                                                    \
  }

DEFINE_ALLOC_BENCHMARK(8b, arena_fixture, 8)
DEFINE_ALLOC_BENCHMARK(1kb, arena_fixture, 1024)
DEFINE_ALLOC_BENCHMARK(64kb, arena_fixture, 64 * 1024)

DEFINE_ALLOC_BENCHMARK(8b, buffer_fixture, 8)
DEFINE_ALLOC_BENCHMARK(1kb, buffer_fixture, 1024)
DEFINE_ALLOC_BENCHMARK(64kb, buffer_fixture, 64 * 1024)

// --- Calloc Benchmarks ---

#define DEFINE_CALLOC_BENCHMARK(NAME, FIXTURE, SIZE)                           \
  UBENCH_F(FIXTURE, calloc_free_##NAME) {                                      \
    void *ptr = allo_calloc(&ubench_fixture->a, 1, SIZE);                      \
    ubench_do_nothing(ptr);                                                    \
    allo_free(&ubench_fixture->a, ptr, SIZE);                                  \
  }

DEFINE_CALLOC_BENCHMARK(1kb, libc_fixture, 1024)
DEFINE_CALLOC_BENCHMARK(1kb, buddy_fixture, 1024)

// --- Realloc Benchmarks ---

UBENCH_F(libc_fixture, realloc_move) {
  void *ptr = allo_alloc(&ubench_fixture->a, 1024);
  ptr = allo_realloc(&ubench_fixture->a, ptr, 1024, 2048);
  ubench_do_nothing(ptr);
  ptr = allo_realloc(&ubench_fixture->a, ptr, 2048, 1024);
  ubench_do_nothing(ptr);
  allo_free(&ubench_fixture->a, ptr, 1024);
}

UBENCH_F(arena_fixture, realloc_inplace) {
  void *ptr = allo_alloc(&ubench_fixture->a, 8);
  ptr = allo_realloc(&ubench_fixture->a, ptr, 8, 16);
  ubench_do_nothing(ptr);
}

UBENCH_F(buddy_fixture, realloc_move) {
  void *ptr = allo_alloc(&ubench_fixture->a, 1024);
  ptr = allo_realloc(&ubench_fixture->a, ptr, 1024, 2048);
  ubench_do_nothing(ptr);
  ptr = allo_realloc(&ubench_fixture->a, ptr, 2048, 1024);
  ubench_do_nothing(ptr);
  allo_free(&ubench_fixture->a, ptr, 1024);
}

UBENCH_F(buffer_fixture, realloc_inplace) {
  void *ptr = allo_alloc(&ubench_fixture->a, 8);
  ptr = allo_realloc(&ubench_fixture->a, ptr, 8, 16);
  ubench_do_nothing(ptr);
}

// --- Complex Sequences (These maintain loops as they define the pattern) ---

UBENCH_F(buddy_fixture, torture_mix) {
  void *ptrs[TORTURE_COUNT];
  for (int i = 0; i < TORTURE_COUNT; i++) {
    ptrs[i] = allo_alloc(&ubench_fixture->a, ubench_fixture->sizes[i]);
    ubench_do_nothing(ptrs[i]);
  }
  for (int i = 0; i < TORTURE_COUNT; i += 2) {
    if (ptrs[i]) {
      allo_free(&ubench_fixture->a, ptrs[i], ubench_fixture->sizes[i]);
      ptrs[i] = NULL;
    }
  }
  for (int i = 0; i < TORTURE_COUNT; i += 2) {
    ptrs[i] = allo_alloc(&ubench_fixture->a, ubench_fixture->realloc_sizes[i]);
    ubench_do_nothing(ptrs[i]);
  }
  for (int i = 0; i < TORTURE_COUNT; i++) {
    if (ptrs[i]) {
      size_t sz = (i % 2 == 0) ? ubench_fixture->realloc_sizes[i]
                               : ubench_fixture->sizes[i];
      allo_free(&ubench_fixture->a, ptrs[i], sz);
    }
  }
}

struct race_state {
  size_t sizes[RACE_COUNT];
};
static struct race_state global_race_state;
static void init_race_state(void) {
  srand(123);
  for (int i = 0; i < RACE_COUNT; i++)
    global_race_state.sizes[i] = (rand() % 2048) + 1;
}

#define DEFINE_RACE_BENCHMARK(NAME, FIXTURE)                                   \
  UBENCH_F(FIXTURE, race_##NAME) {                                             \
    void *ptrs[RACE_COUNT];                                                    \
    for (int i = 0; i < RACE_COUNT; i++) {                                     \
      ptrs[i] = allo_alloc(&ubench_fixture->a, global_race_state.sizes[i]);    \
      ubench_do_nothing(ptrs[i]);                                              \
    }                                                                          \
    for (int i = 0; i < RACE_COUNT; i++) {                                     \
      allo_free(&ubench_fixture->a, ptrs[i], global_race_state.sizes[i]);      \
    }                                                                          \
  }

DEFINE_RACE_BENCHMARK(libc, libc_fixture)
DEFINE_RACE_BENCHMARK(buddy, buddy_fixture)

// --- Individual Allocator "Signature" ---

UBENCH_F(page_fixture, single_page_alloc_free) {
  void *ptr = allo_alloc(&ubench_fixture->a, 4096);
  ubench_do_nothing(ptr);
  allo_free(&ubench_fixture->a, ptr, 4096);
}

// --- Lifecycle Benchmarks ---

#define DEFINE_LIFECYCLE_BENCHMARK(NAME, ...)                                  \
  UBENCH(lifecycle, NAME) {                                                    \
    __VA_ARGS__                                                                \
  }

DEFINE_LIFECYCLE_BENCHMARK(libc, {
  allo_t a;
  make_c_allocator(&a);
  allo_destroy(&a);
})
DEFINE_LIFECYCLE_BENCHMARK(arena, {
  allo_t child, a;
  make_c_allocator(&child);
  make_arena_allocator(&a, &child, 64 * 1024);
  allo_destroy(&a);
  allo_destroy(&child);
})
DEFINE_LIFECYCLE_BENCHMARK(pool, {
  allo_t child, a;
  make_c_allocator(&child);
  make_pool_allocator(&a, &child, NULL, 64, 100);
  allo_destroy(&a);
  allo_destroy(&child);
})
DEFINE_LIFECYCLE_BENCHMARK(buddy, {
  allo_t child, a;
  make_c_allocator(&child);
  make_buddy_allocator(&a, &child, NULL, 64 * 1024);
  allo_destroy(&a);
  allo_destroy(&child);
})
DEFINE_LIFECYCLE_BENCHMARK(mtx, {
  allo_t base, a;
  make_c_allocator(&base);
  make_mtx_allocator(&a, &base);
  allo_destroy(&a);
  allo_destroy(&base);
})

// --- Thread-Safe (Locking) Benchmarks ---

struct mtx_fixture {
  allo_t base;
  allo_t a;
};
UBENCH_F_SETUP(mtx_fixture) {
  make_c_allocator(&ubench_fixture->base);
  make_mtx_allocator(&ubench_fixture->a, &ubench_fixture->base);
}
UBENCH_F_TEARDOWN(mtx_fixture) {
  allo_destroy(&ubench_fixture->a);
  allo_destroy(&ubench_fixture->base);
}

UBENCH_F(mtx_fixture, alloc_free_8b) {
  void *ptr = allo_alloc(&ubench_fixture->a, 8);
  ubench_do_nothing(ptr);
  allo_free(&ubench_fixture->a, ptr, 8);
}

// --- Fallback Benchmarks ---

struct fallback_fixture {
  allo_t primary;
  allo_t fallback;
  allo_t fb;
  void *pool_buf;
};
UBENCH_F_SETUP(fallback_fixture) {
  size_t pool_size = 64 * 1024;
  ubench_fixture->pool_buf = malloc(pool_size);
  make_pool_allocator(&ubench_fixture->primary, NULL, ubench_fixture->pool_buf,
                      64, 1000);
  make_c_allocator(&ubench_fixture->fallback);
  make_fallback_allocator(&ubench_fixture->fb, &ubench_fixture->primary,
                          &ubench_fixture->fallback);
}
UBENCH_F_TEARDOWN(fallback_fixture) {
  allo_destroy(&ubench_fixture->fb);
  allo_destroy(&ubench_fixture->primary);
  allo_destroy(&ubench_fixture->fallback);
  free(ubench_fixture->pool_buf);
}

UBENCH_F(fallback_fixture, fast_path_8b) {
  void *ptr = allo_alloc(&ubench_fixture->fb, 8);
  ubench_do_nothing(ptr);
  allo_free(&ubench_fixture->fb, ptr, 8);
}

UBENCH_F(fallback_fixture, slow_path_1kb) {
  // 1KB doesn't fit in 64B pool, goes to fallback
  void *ptr = allo_alloc(&ubench_fixture->fb, 1024);
  ubench_do_nothing(ptr);
  allo_free(&ubench_fixture->fb, ptr, 1024);
}

UBENCH_STATE();

int main(int argc, const char *const argv[]) {
  init_race_state();
  return ubench_main(argc, argv);
}
