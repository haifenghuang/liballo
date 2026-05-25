#include "allo.h"
#include "test_harness.h"

void test_gen_basic(void) {
  printf("Testing General-Purpose Allocator: Basic\n");
  allo_t gen;
  assert(make_gen_allocator(&gen) == ALLO_OK);

  // 1. Small Tier (Pools)
  void *p1 = allo_alloc(&gen, 64);
  assert(p1 != NULL);
  assert(allo_contains(&gen, p1) == ALLO_CONTAINS_YES);
  memset(p1, 0xAA, 64);

  // 2. Medium Tier (Buddy)
  void *p2 = allo_alloc(&gen, 64 * 1024);
  assert(p2 != NULL);
  assert(allo_contains(&gen, p2) == ALLO_CONTAINS_YES);
  memset(p2, 0xBB, 64 * 1024);

  // 3. Large Tier (Page)
  void *p3 = allo_alloc(&gen, 2 * 1024 * 1024);
  assert(p3 != NULL);
  assert(allo_contains(&gen, p3) == ALLO_CONTAINS_YES);
  memset(p3, 0xCC, 2 * 1024 * 1024);

  // Realloc cross-tier: Small -> Medium
  p1 = allo_realloc(&gen, p1, 64, 128 * 1024);
  assert(p1 != NULL);
  assert(((unsigned char *)p1)[0] == 0xAA);

  // Realloc cross-tier: Medium -> Large
  p2 = allo_realloc(&gen, p2, 64 * 1024, 4 * 1024 * 1024);
  assert(p2 != NULL);
  assert(((unsigned char *)p2)[0] == 0xBB);

  allo_free(&gen, p1, 128 * 1024);
  allo_free(&gen, p2, 4 * 1024 * 1024);
  allo_free(&gen, p3, 2 * 1024 * 1024);

  allo_destroy(&gen);
  printf("Basic Gen tests passed!\n");
}

void test_gen_realloc_same_tier(void) {
  printf("Testing General-Purpose Allocator: Same-Tier Realloc\n");
  allo_t gen;
  assert(make_gen_allocator(&gen) == ALLO_OK);

  // Pool realloc (same pool)
  void *p1 = allo_alloc(&gen, 10);
  memset(p1, 0x11, 10);
  p1 = allo_realloc(&gen, p1, 10, 15); // stays in 16b pool
  assert(p1 != NULL);
  assert(((unsigned char *)p1)[0] == 0x11);

  // Pool realloc (different pool)
  p1 = allo_realloc(&gen, p1, 15, 40); // 16b -> 64b pool
  assert(p1 != NULL);
  assert(((unsigned char *)p1)[0] == 0x11);

  // Buddy realloc
  void *p2 = allo_alloc(&gen, 4096);
  memset(p2, 0x22, 4096);
  p2 = allo_realloc(&gen, p2, 4096, 8192);
  assert(p2 != NULL);
  assert(((unsigned char *)p2)[0] == 0x22);

  allo_free(&gen, p1, 40);
  allo_free(&gen, p2, 8192);

  allo_destroy(&gen);
  printf("Same-tier realloc tests passed!\n");
}

int main(void) {
  test_gen_basic();
  test_gen_realloc_same_tier();
  return 0;
}
