#include "allo.h"
#include "test_harness.h"
#include <stdint.h>

void test_buddy_basic(void) {
  printf("Testing Buddy Allocator: Basic\n");
  allo_t c_alloc, buddy;
  ALLO_ALIGNED_BUF(buffer, (1024 * 1024UL) + (128 * 1024UL));
  assert(make_fixed_buf_allocator(&c_alloc, buffer, sizeof(buffer)) == ALLO_OK);
  assert(make_buddy_allocator(&buddy, &c_alloc, NULL, 1024 * 1024UL) ==
         ALLO_OK);

  void *p1 = allo_alloc(
      &buddy,
      100); // Should get 128 (min is 32, but we use 128 here or whatever)
  assert(p1 != NULL);
  memset(p1, 0xAA, 100);

  void *p2 = allo_alloc(&buddy, 200); // Should get 256
  assert(p2 != NULL);
  memset(p2, 0xBB, 200);

  void *p3 = allo_alloc(&buddy, 512 * 1024UL); // 512KB
  assert(p3 != NULL);

  allo_free(&buddy, p1, 100);
  allo_free(&buddy, p2, 200);

  // p1 and p2 are buddies? No, 128 and 256.
  // But they should eventually coalesce if we free everything.

  allo_free(&buddy, p3, 512 * 1024UL);

  // Try a large one after coalescing
  void *p4 = allo_alloc(&buddy, 1024 * 1024UL);
  assert(p4 != NULL);
  allo_free(&buddy, p4, 1024 * 1024UL);

  allo_destroy(&buddy);
  allo_destroy(&c_alloc);
  printf("Basic Buddy tests passed!\n");
}

void test_buddy_alignment(void) {
  printf("Testing Buddy Allocator: Alignment\n");
  ALLO_ALIGNED_BUF(buffer, (64 * 1024UL) + (16 * 1024UL));
  allo_t fixed;
  assert(make_fixed_buf_allocator(&fixed, buffer, sizeof(buffer)) == ALLO_OK);
  allo_t buddy;
  assert(make_buddy_allocator(&buddy, &fixed, NULL, 64 * 1024UL) == ALLO_OK);

  for (int i = 0; i < 100; ++i) {
    void *p = allo_alloc(&buddy, 1);
    assert(((uintptr_t)p % 8) == 0);
    allo_free(&buddy, p, 1);
  }

  allo_destroy(&buddy);
  allo_destroy(&fixed);
  printf("Alignment Buddy tests passed!\n");
}

void test_buddy_torture(void) {
  printf("Testing Buddy Allocator: Torture\n");
  ALLO_ALIGNED_BUF(buffer, 256 * 1024UL);
  allo_t fixed, buddy;
  assert(make_fixed_buf_allocator(&fixed, buffer, sizeof(buffer)) == ALLO_OK);
  assert(make_buddy_allocator(&buddy, &fixed, NULL, 128 * 1024UL) == ALLO_OK);

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
  ptrs[count] = allo_alloc(&buddy, 32 * 1024UL);
  assert(ptrs[count] != NULL);
  count++;

  // 3. Free everything to force coalescing
  for (int i = 0; i < 64; ++i) {
    allo_free(&buddy, ptrs[i], 1024);
  }
  allo_free(&buddy, ptrs[64], 32 * 1024UL);

  // 4. Try to allocate the whole thing again (proves perfect coalescing)
  void *big = allo_alloc(&buddy, 128 * 1024UL);
  assert(big != NULL);
  allo_free(&buddy, big, 128 * 1024UL);

  // 5. Randomish sequence
  for (int i = 0; i < 100; ++i) {
    size_t sz = (i % 7 + 1) * 1024UL;
    void *p = allo_alloc(&buddy, sz);
    if (p)
      allo_free(&buddy, p, sz);
  }

  allo_destroy(&buddy);
  allo_destroy(&fixed);
  printf("Buddy torture tests passed!\n");
}

void test_buddy_validation(void) {
  printf("Testing Buddy Allocator: Validation\n");
  ALLO_ALIGNED_BUF(buffer, 1024);
  allo_t child, a;
  assert(make_fixed_buf_allocator(&child, buffer, sizeof(buffer)) == ALLO_OK);

  assert(make_buddy_allocator(NULL, &child, NULL, 1024) == ALLO_ERR_INVAL);
  assert(make_buddy_allocator(&a, NULL, NULL, 1024) == ALLO_ERR_INVAL);
  assert(make_buddy_allocator(&a, &child, NULL, 0) == ALLO_ERR_INVAL);

  allo_destroy(&child);
  printf("Buddy validation tests passed!\n");
}

int main(void) {
  test_buddy_validation();
  test_buddy_basic();
  test_buddy_alignment();
  test_buddy_torture();
  return 0;
}
