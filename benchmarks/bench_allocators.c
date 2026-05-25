// Workaround for strict C11 which disables 'asm'
#ifndef __cplusplus
#define asm __asm__
#endif

#include "allo.h"
#include "ubench.h"
#include <stdlib.h>
#include <string.h>

#define BULK_COUNT 1000
#define TORTURE_COUNT 256

// Libc Baseline
struct libc_fixture {
  allo_t a;
};

UBENCH_F_SETUP(libc_fixture) {
  make_c_allocator(&ubench_fixture->a);
}

UBENCH_F_TEARDOWN(libc_fixture) {
  allo_destroy(&ubench_fixture->a);
}

UBENCH_F(libc_fixture, bulk_alloc_free_8b) {
  void *ptrs[BULK_COUNT];
  for (int i = 0; i < BULK_COUNT; i++) {
    ptrs[i] = allo_alloc(&ubench_fixture->a, 8);
    ubench_do_nothing(ptrs[i]);
  }
  for (int i = 0; i < BULK_COUNT; i++) {
    allo_free(&ubench_fixture->a, ptrs[i], 8);
  }
}

UBENCH_F(libc_fixture, realloc_move) {
  void *ptr = allo_alloc(&ubench_fixture->a, 1024);
  for (int i = 0; i < 100; i++) {
    ptr = allo_realloc(&ubench_fixture->a, ptr, 1024, 2048);
    ubench_do_nothing(ptr);
    ptr = allo_realloc(&ubench_fixture->a, ptr, 2048, 1024);
    ubench_do_nothing(ptr);
  }
  allo_free(&ubench_fixture->a, ptr, 1024);
}

// Arena Benchmarks
struct arena_fixture {
  allo_t child;
  allo_t a;
};

UBENCH_F_SETUP(arena_fixture) {
  make_c_allocator(&ubench_fixture->child);
  size_t size = 64 * 1024 * 1024;
  make_arena_allocator(&ubench_fixture->a, &ubench_fixture->child, size);
  
  void *p = allo_alloc(&ubench_fixture->a, size - 1024);
  if (p) {
      memset(p, 0, size - 1024);
  }
  allo_destroy(&ubench_fixture->a);
  make_arena_allocator(&ubench_fixture->a, &ubench_fixture->child, size);
}

UBENCH_F_TEARDOWN(arena_fixture) {
  allo_destroy(&ubench_fixture->a);
  allo_destroy(&ubench_fixture->child);
}

UBENCH_F(arena_fixture, bulk_alloc_8b) {
  for (int i = 0; i < BULK_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, 8);
    ubench_do_nothing(ptr);
  }
}

UBENCH_F(arena_fixture, realloc_inplace) {
  void *ptr = allo_alloc(&ubench_fixture->a, 8);
  for (int i = 0; i < 100; i++) {
    ptr = allo_realloc(&ubench_fixture->a, ptr, 8 + (size_t)i, 9 + (size_t)i);
    ubench_do_nothing(ptr);
  }
}

// Pool Benchmarks
struct pool_fixture {
  allo_t child;
  allo_t a;
};

UBENCH_F_SETUP(pool_fixture) {
  make_c_allocator(&ubench_fixture->child);
  size_t block_size = 64;
  size_t num_blocks = 10000;
  make_pool_allocator(&ubench_fixture->a, &ubench_fixture->child, NULL, block_size, num_blocks);
  
  void *p = allo_alloc(&ubench_fixture->a, block_size * num_blocks);
  if (p) {
      memset(p, 0, block_size * num_blocks);
  }
  allo_destroy(&ubench_fixture->a);
  make_pool_allocator(&ubench_fixture->a, &ubench_fixture->child, NULL, block_size, num_blocks);
}

UBENCH_F_TEARDOWN(pool_fixture) {
  allo_destroy(&ubench_fixture->a);
  allo_destroy(&ubench_fixture->child);
}

UBENCH_F(pool_fixture, bulk_alloc_free_64b) {
  void *ptrs[BULK_COUNT];
  for (int i = 0; i < BULK_COUNT; i++) {
    ptrs[i] = allo_alloc(&ubench_fixture->a, 64);
    ubench_do_nothing(ptrs[i]);
  }
  for (int i = 0; i < BULK_COUNT; i++) {
    allo_free(&ubench_fixture->a, ptrs[i], 64);
  }
}

// Buddy Benchmarks
struct buddy_fixture {
  allo_t child;
  allo_t a;
  size_t sizes[TORTURE_COUNT];
  size_t realloc_sizes[TORTURE_COUNT];
};

UBENCH_F_SETUP(buddy_fixture) {
  srand(42);
  make_c_allocator(&ubench_fixture->child);
  size_t total_size = 32 * 1024 * 1024;
  make_buddy_allocator(&ubench_fixture->a, &ubench_fixture->child, NULL, total_size);
  
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

UBENCH_F(buddy_fixture, bulk_alloc_free_8b) {
  void *ptrs[BULK_COUNT];
  for (int i = 0; i < BULK_COUNT; i++) {
    ptrs[i] = allo_alloc(&ubench_fixture->a, 8);
    ubench_do_nothing(ptrs[i]);
  }
  for (int i = 0; i < BULK_COUNT; i++) {
    allo_free(&ubench_fixture->a, ptrs[i], 8);
  }
}

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
      size_t sz = (i % 2 == 0) ? ubench_fixture->realloc_sizes[i] : ubench_fixture->sizes[i];
      allo_free(&ubench_fixture->a, ptrs[i], sz);
    }
  }
}

UBENCH_F(buddy_fixture, realloc_move) {
  void *ptr = allo_alloc(&ubench_fixture->a, 1024);
  for (int i = 0; i < 100; i++) {
    ptr = allo_realloc(&ubench_fixture->a, ptr, 1024, 2048);
    ubench_do_nothing(ptr);
    ptr = allo_realloc(&ubench_fixture->a, ptr, 2048, 1024);
    ubench_do_nothing(ptr);
  }
  allo_free(&ubench_fixture->a, ptr, 1024);
}

// Fixed Buffer Benchmarks
struct buffer_fixture {
  char *buf;
  allo_t a;
};

UBENCH_F_SETUP(buffer_fixture) {
  size_t size = 32 * 1024 * 1024;
  ubench_fixture->buf = malloc(size);
  memset(ubench_fixture->buf, 0, size);
  make_fixed_buf_allocator(&ubench_fixture->a, ubench_fixture->buf, size);
}

UBENCH_F_TEARDOWN(buffer_fixture) {
  allo_destroy(&ubench_fixture->a);
  free(ubench_fixture->buf);
}

UBENCH_F(buffer_fixture, bulk_alloc_8b) {
  for (int i = 0; i < BULK_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, 8);
    ubench_do_nothing(ptr);
  }
}

UBENCH_F(buffer_fixture, realloc_inplace) {
  void *ptr = allo_alloc(&ubench_fixture->a, 8);
  for (int i = 0; i < 100; i++) {
    ptr = allo_realloc(&ubench_fixture->a, ptr, 8 + (size_t)i, 9 + (size_t)i);
    ubench_do_nothing(ptr);
  }
}

// Lifecycle Benchmarks
UBENCH(lifecycle, libc) {
  for (int i = 0; i < 100; i++) {
    allo_t a;
    make_c_allocator(&a);
    allo_destroy(&a);
  }
}

UBENCH(lifecycle, arena) {
  for (int i = 0; i < 100; i++) {
    allo_t child, a;
    make_c_allocator(&child);
    make_arena_allocator(&a, &child, 64 * 1024);
    allo_destroy(&a);
    allo_destroy(&child);
  }
}

UBENCH(lifecycle, pool) {
  for (int i = 0; i < 100; i++) {
    allo_t child, a;
    make_c_allocator(&child);
    make_pool_allocator(&a, &child, NULL, 64, 100);
    allo_destroy(&a);
    allo_destroy(&child);
  }
}

UBENCH(lifecycle, buddy) {
  for (int i = 0; i < 100; i++) {
    allo_t child, a;
    make_c_allocator(&child);
    make_buddy_allocator(&a, &child, NULL, 64 * 1024);
    allo_destroy(&a);
    allo_destroy(&child);
  }
}

UBENCH_MAIN();
