#include "allo.h"
#include "test_harness.h"

int main(void) {
  allo_t primary, fallback, fb;
  ALLO_ALIGNED_BUF(pool_buf, 64);
  ALLO_ALIGNED_BUF(fallback_buf, 256);

  // Primary: Pool with only 1 block of 64 bytes
  assert(make_pool_allocator(&primary, NULL, pool_buf, 64, 1) == ALLO_OK);
  // Fallback: Fixed buffer
  assert(make_fixed_buf_allocator(&fallback, fallback_buf, 256) == ALLO_OK);
  assert(make_fallback_allocator(&fb, &primary, &fallback) == ALLO_OK);

  // 1. First allocation fits in primary
  void *p1 = allo_alloc(&fb, 64);
  assert(p1 != NULL);
  assert(allo_contains(&primary, p1) == ALLO_CONTAINS_YES);
  assert(allo_contains(&fb, p1) == ALLO_CONTAINS_YES);

  // 2. Second allocation fails in primary, spills to fallback
  void *p2 = allo_alloc(&fb, 64);
  assert(p2 != NULL);
  assert(allo_contains(&primary, p2) == ALLO_CONTAINS_NO);
  assert(allo_contains(&fallback, p2) == ALLO_CONTAINS_YES);
  assert(allo_contains(&fb, p2) == ALLO_CONTAINS_YES);

  // 3. Free both
  allo_free(&fb, p1, 64);
  allo_free(&fb, p2, 64);

  // 4. Test realloc spillover
  void *p3 = allo_alloc(&fb, 32);
  assert(allo_contains(&primary, p3) == ALLO_CONTAINS_YES);

  // Realloc to 128 bytes (pool only has 64)
  void *p4 = allo_realloc(&fb, p3, 32, 128);
  assert(p4 != NULL);
  assert(allo_contains(&primary, p4) == ALLO_CONTAINS_NO);
  assert(allo_contains(&fallback, p4) == ALLO_CONTAINS_YES);

  allo_free(&fb, p4, 128);

  allo_destroy(&fb);
  allo_destroy(&primary);
  allo_destroy(&fallback);

  return 0;
}
