#include "allo.h"
#include "test_harness.h"

void test_pool_allocator(void) {
  ALLO_ALIGNED_BUF(child_buf, 1024);
  allo_t child, pool;
  assert(make_fixed_buf_allocator(&child, child_buf, 1024) == ALLO_OK);
  assert(make_pool_allocator(&pool, &child, NULL, 64, 10) == ALLO_OK);
  void *p1 = allo_alloc(&pool, 64);
  assert(p1 != NULL);

  void *p2 = allo_alloc(&pool, 64);
  assert(p2 != NULL);
  assert(p1 != p2);

  allo_free(&pool, p1, 64);
  void *p3 = allo_alloc(&pool, 64);
  assert(p3 == p1); // Should reuse the block

  allo_destroy(&pool);
  printf("Pool allocator test passed\n");
}

void test_pool_exhaustive(void) {
  size_t count = 10;
  size_t buf_size = 64 * count; // Enough for 10 blocks
  allo_t child, pool;

  ALLO_ALIGNED_BUF(child_buf, buf_size);
  assert(make_fixed_buf_allocator(&child, child_buf, buf_size) == ALLO_OK);
  assert(make_pool_allocator(&pool, &child, NULL, 64, count) == ALLO_OK);
  void *pointers[10];

  for (size_t i = 0; i < count; ++i) {
    pointers[i] = allo_alloc(&pool, 64);
    assert(pointers[i] != NULL);
  }

  // Exhausted
  assert(allo_alloc(&pool, 64) == NULL);

  // Free one and reuse
  allo_free(&pool, pointers[5], 64);
  void *p = allo_alloc(&pool, 64);
  assert(p == pointers[5]);

  allo_destroy(&pool);
  printf("Pool exhaustive test passed\n");
}

void test_pool_validation(void) {
  printf("Testing Pool Allocator: Validation\n");
  ALLO_ALIGNED_BUF(buffer, 1024);
  allo_t pool, child;
  assert(make_fixed_buf_allocator(&child, buffer, sizeof(buffer)) == ALLO_OK);
  assert(make_pool_allocator(NULL, &child, NULL, 64, 10) == ALLO_ERR_INVAL);

  // Special case: if buffer is NULL, child MUST NOT be NULL
  assert(make_pool_allocator(&pool, NULL, NULL, 64, 10) == ALLO_ERR_INVAL);
  assert(make_pool_allocator(&pool, &child, NULL, 0, 10) == ALLO_ERR_INVAL);
  assert(make_pool_allocator(&pool, &child, NULL, 64, 0) == ALLO_ERR_INVAL);

  allo_destroy(&child);
  printf("Pool validation tests passed!\n");
}

int main(void) {
  test_pool_validation();
  test_pool_allocator();
  test_pool_exhaustive();
  return 0;
}
