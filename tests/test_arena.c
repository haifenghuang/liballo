#include "allo.h"
#include "test_harness.h"

void test_arena_allocator(void) {
  allo_t child;
  ALLO_ALIGNED_BUF(buffer, 8192);
  assert(make_fixed_buf_allocator(&child, buffer, sizeof(buffer)) == ALLO_OK);
  allo_t a;
  assert(make_arena_allocator(&a, &child, 1024) == ALLO_OK);
  void *p1 = allo_alloc(&a, 100);
  assert(p1 != NULL);

  void *p2 = allo_alloc(&a, 2000); // Should trigger new block
  assert(p2 != NULL);

  void *p3 = allo_alloc(&a, 500);
  assert(p3 != NULL);

  allo_destroy(&a);
  printf("Arena allocator test passed\n");
}

void test_arena_stress(void) {
  allo_t child;
  ALLO_ALIGNED_BUF(buffer, 256 * 1024);
  assert(make_fixed_buf_allocator(&child, buffer, sizeof(buffer)) == ALLO_OK);
  allo_t a;
  assert(make_arena_allocator(&a, &child, 1024) == ALLO_OK);
  void *pointers[5000];
  for (int i = 0; i < 5000; ++i) {
    pointers[i] = allo_alloc(&a, 16);
    assert(pointers[i] != NULL);
    memset(pointers[i], 0xFF, 16);
  }
  allo_destroy(&a);
  printf("Arena stress test passed\n");
}

void test_arena_validation(void) {
  printf("Testing Arena Allocator: Validation\n");
  allo_t a, child;
  ALLO_ALIGNED_BUF(buffer, 1024);
  assert(make_fixed_buf_allocator(&child, buffer, sizeof(buffer)) == ALLO_OK);

  assert(make_arena_allocator(NULL, &child, 1024) == ALLO_ERR_INVAL);
  assert(make_arena_allocator(&a, NULL, 1024) == ALLO_ERR_INVAL);
  assert(make_arena_allocator(&a, &child, 0) == ALLO_ERR_INVAL);

  allo_destroy(&child);
  printf("Arena validation tests passed!\n");
}

int main(void) {
  test_arena_validation();
  test_arena_allocator();
  test_arena_stress();
  return 0;
}
