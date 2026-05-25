#include "allo.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void test_buddy_basic(void) {
  printf("Testing Buddy Allocator: Basic\n");
  allo_t c_alloc;
  assert(make_c_allocator(&c_alloc) == ALLO_OK);
  // 1MB buddy allocator
  allo_t buddy;
  assert(make_buddy_allocator(&buddy, &c_alloc, NULL, 1024 * 1024) == ALLO_OK);

  void *p1 = allo_alloc(
      &buddy,
      100); // Should get 128 (min is 32, but we use 128 here or whatever)
  assert(p1 != NULL);
  memset(p1, 0xAA, 100);

  void *p2 = allo_alloc(&buddy, 200); // Should get 256
  assert(p2 != NULL);
  memset(p2, 0xBB, 200);

  void *p3 = allo_alloc(&buddy, 512 * 1024); // 512KB
  assert(p3 != NULL);

  allo_free(&buddy, p1);
  allo_free(&buddy, p2);

  // p1 and p2 are buddies? No, 128 and 256.
  // But they should eventually coalesce if we free everything.

  allo_free(&buddy, p3);

  // Try a large one after coalescing
  void *p4 = allo_alloc(&buddy, 1024 * 1024);
  assert(p4 != NULL);
  allo_free(&buddy, p4);

  allo_destroy(&buddy);
  allo_destroy(&c_alloc);
  printf("Basic Buddy tests passed!\n");
}

void test_buddy_alignment(void) {
  printf("Testing Buddy Allocator: Alignment\n");
  allo_t c_alloc;
  assert(make_c_allocator(&c_alloc) == ALLO_OK);
  allo_t buddy;
  assert(make_buddy_allocator(&buddy, &c_alloc, NULL, 64 * 1024) == ALLO_OK);

  for (int i = 0; i < 100; ++i) {
    void *p = allo_alloc(&buddy, 1);
    assert(((uintptr_t)p % 8) == 0);
    allo_free(&buddy, p);
  }

  allo_destroy(&buddy);
  allo_destroy(&c_alloc);
  printf("Alignment Buddy tests passed!\n");
}

void test_buddy_torture(void) {
  printf("Testing Buddy Allocator: Torture\n");
  allo_t c_alloc;
  assert(make_c_allocator(&c_alloc) == ALLO_OK);
  // 128KB buddy allocator
  allo_t buddy;
  assert(make_buddy_allocator(&buddy, &c_alloc, NULL, 128 * 1024) == ALLO_OK);

  void *ptrs[1024];
  int count = 0;

  // 1. Allocate many small blocks to force deep splits
  for (int i = 0; i < 64; ++i) {
    ptrs[count] = allo_alloc(&buddy, 1024);
    assert(ptrs[count] != NULL);
    memset(ptrs[count], 0xCC, 1024);
    count++;
  }

  // 2. Allocate some large blocks
  ptrs[count] = allo_alloc(&buddy, 32 * 1024);
  assert(ptrs[count] != NULL);
  count++;

  // 3. Free everything to force coalescing
  for (int i = 0; i < count; ++i) {
    allo_free(&buddy, ptrs[i]);
  }

  // 4. Try to allocate the whole thing again (proves perfect coalescing)
  void *big = allo_alloc(&buddy, 128 * 1024);
  assert(big != NULL);
  allo_free(&buddy, big);

  // 5. Randomish sequence
  for (int i = 0; i < 100; ++i) {
    void *p = allo_alloc(&buddy, (i % 7 + 1) * 1024);
    if (p)
      allo_free(&buddy, p);
  }

  allo_destroy(&buddy);
  allo_destroy(&c_alloc);
  printf("Buddy torture tests passed!\n");
}

int main(void) {
  test_buddy_basic();
  test_buddy_alignment();
  test_buddy_torture();
  return 0;
}
