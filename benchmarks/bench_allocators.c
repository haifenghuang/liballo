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
#define ARENA_SIZE (256 * 1024UL * 1024UL)
struct arena_fixture {
  allo_t child;
  allo_t a;
};
UBENCH_F_SETUP(arena_fixture) {
  make_c_allocator(&ubench_fixture->child);
  make_arena_allocator(&ubench_fixture->a, &ubench_fixture->child, ARENA_SIZE);
}
UBENCH_F_TEARDOWN(arena_fixture) {
  allo_destroy(&ubench_fixture->a);
  allo_destroy(&ubench_fixture->child);
}

// Pool
#define POOL_BLOCK_SIZE 64UL
#define POOL_NUM_BLOCKS 10000
struct pool_fixture {
  allo_t child;
  allo_t a;
};
UBENCH_F_SETUP(pool_fixture) {
  make_c_allocator(&ubench_fixture->child);
  make_pool_allocator(&ubench_fixture->a, &ubench_fixture->child, NULL,
                      POOL_BLOCK_SIZE, POOL_NUM_BLOCKS);
}
UBENCH_F_TEARDOWN(pool_fixture) {
  allo_destroy(&ubench_fixture->a);
  allo_destroy(&ubench_fixture->child);
}

// Buddy
#define BUDDY_TOTAL_SIZE (64 * 1024UL * 1024UL)
struct buddy_fixture {
  allo_t child;
  allo_t a;
  size_t sizes[TORTURE_COUNT];
  size_t realloc_sizes[TORTURE_COUNT];
};
UBENCH_F_SETUP(buddy_fixture) {
  srand(42);
  make_c_allocator(&ubench_fixture->child);
  make_buddy_allocator(&ubench_fixture->a, &ubench_fixture->child, NULL,
                       BUDDY_TOTAL_SIZE);

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
#define BUFFER_SIZE (256 * 1024UL * 1024UL)
struct buffer_fixture {
  char *buf;
  allo_t a;
};
UBENCH_F_SETUP(buffer_fixture) {
  ubench_fixture->buf = malloc(BUFFER_SIZE);
  memset(ubench_fixture->buf, 0, BUFFER_SIZE);
  make_fixed_buf_allocator(&ubench_fixture->a, ubench_fixture->buf,
                           BUFFER_SIZE);
}
UBENCH_F_TEARDOWN(buffer_fixture) {
  allo_destroy(&ubench_fixture->a);
  free(ubench_fixture->buf);
}

// Gen
struct gen_fixture {
  allo_t a;
};
UBENCH_F_SETUP(gen_fixture) {
  make_gen_allocator(&ubench_fixture->a);
}
UBENCH_F_TEARDOWN(gen_fixture) {
  allo_destroy(&ubench_fixture->a);
}

#define REPEAT_COUNT 1000

// --- Basic Alloc/Free Benchmarks ---

#define DEFINE_ALLOC_FREE_BENCHMARK(NAME, FIXTURE, SIZE)                       \
  UBENCH_F(FIXTURE, alloc_free_##NAME) {                                       \
    for (int i = 0; i < REPEAT_COUNT; i++) {                                   \
      void *ptr = allo_alloc(&ubench_fixture->a, SIZE);                        \
      ubench_do_nothing(ptr);                                                  \
      allo_free(&ubench_fixture->a, ptr, SIZE);                                \
    }                                                                          \
  }

DEFINE_ALLOC_FREE_BENCHMARK(8b, libc_fixture, 8)
DEFINE_ALLOC_FREE_BENCHMARK(1kb, libc_fixture, 1024)
DEFINE_ALLOC_FREE_BENCHMARK(64kb, libc_fixture, 64 * 1024)

DEFINE_ALLOC_FREE_BENCHMARK(8b, buddy_fixture, 8)
DEFINE_ALLOC_FREE_BENCHMARK(1kb, buddy_fixture, 1024)
DEFINE_ALLOC_FREE_BENCHMARK(64kb, buddy_fixture, 64 * 1024)

DEFINE_ALLOC_FREE_BENCHMARK(8b, gen_fixture, 8)
DEFINE_ALLOC_FREE_BENCHMARK(1kb, gen_fixture, 1024)
DEFINE_ALLOC_FREE_BENCHMARK(64kb, gen_fixture, 64 * 1024)

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
    for (int i = 0; i < REPEAT_COUNT; i++) {                                   \
      void *ptr = allo_calloc(&ubench_fixture->a, 1, SIZE);                    \
      ubench_do_nothing(ptr);                                                  \
      allo_free(&ubench_fixture->a, ptr, SIZE);                                \
    }                                                                          \
  }

DEFINE_CALLOC_BENCHMARK(1kb, libc_fixture, 1024)
DEFINE_CALLOC_BENCHMARK(1kb, buddy_fixture, 1024)

// --- Realloc Benchmarks ---

UBENCH_F(libc_fixture, realloc_move) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, 1024);
    ptr = allo_realloc(&ubench_fixture->a, ptr, 1024, 2048);
    ubench_do_nothing(ptr);
    ptr = allo_realloc(&ubench_fixture->a, ptr, 2048, 1024);
    ubench_do_nothing(ptr);
    allo_free(&ubench_fixture->a, ptr, 1024);
  }
}

UBENCH_F(arena_fixture, realloc_inplace) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, 8);
    ptr = allo_realloc(&ubench_fixture->a, ptr, 8, 16);
    ubench_do_nothing(ptr);
  }
}

UBENCH_F(buddy_fixture, realloc_move) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, 1024);
    ptr = allo_realloc(&ubench_fixture->a, ptr, 1024, 2048);
    ubench_do_nothing(ptr);
    ptr = allo_realloc(&ubench_fixture->a, ptr, 2048, 1024);
    ubench_do_nothing(ptr);
    allo_free(&ubench_fixture->a, ptr, 1024);
  }
}

UBENCH_F(buffer_fixture, realloc_inplace) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, 8);
    ptr = allo_realloc(&ubench_fixture->a, ptr, 8, 16);
    ubench_do_nothing(ptr);
  }
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
DEFINE_RACE_BENCHMARK(gen, gen_fixture)

// --- Individual Allocator "Signature" ---

UBENCH_F(page_fixture, single_page_alloc_free) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, 4096);
    ubench_do_nothing(ptr);
    allo_free(&ubench_fixture->a, ptr, 4096);
  }
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
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, 8);
    ubench_do_nothing(ptr);
    allo_free(&ubench_fixture->a, ptr, 8);
  }
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
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->fb, 8);
    ubench_do_nothing(ptr);
    allo_free(&ubench_fixture->fb, ptr, 8);
  }
}

UBENCH_F(fallback_fixture, slow_path_1kb) {
  // 1KB doesn't fit in 64B pool, goes to fallback
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->fb, 1024);
    ubench_do_nothing(ptr);
    allo_free(&ubench_fixture->fb, ptr, 1024);
  }
}

UBENCH_STATE();

int main(int argc, const char *const argv[]) {
  init_race_state();
  return ubench_main(argc, argv);
}
