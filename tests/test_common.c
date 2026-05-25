#include "allo.h"
#include "test_harness.h"

void test_alignment(void) {
  allo_t child;
  ALLO_ALIGNED_BUF(child_buf, 2048);
  assert(make_fixed_buf_allocator(&child, child_buf, 2048) == ALLO_OK);
  allo_t a;
  assert(make_arena_allocator(&a, &child, 1024) == ALLO_OK);
  void *p1 = allo_alloc(&a, 1);
  void *p2 = allo_alloc(&a, 1);
  assert(((uintptr_t)p1 % 8) == 0);
  assert(((uintptr_t)p2 % 8) == 0);
  assert((char *)p2 >= (char *)p1 + 8);

  allo_destroy(&a);

  allo_t a2;
  char buffer[1024];
  // Align buffer manually to be sure the starting point is aligned
  void *aligned_buffer = (void *)(((uintptr_t)buffer + 7) & ~7);
  assert(make_fixed_buf_allocator(&a2, aligned_buffer, 1024) == ALLO_OK);
  void *p1_b = allo_alloc(&a2, 1);
  void *p2_b = allo_alloc(&a2, 1);
  assert(((uintptr_t)p1_b % 8) == 0);
  assert(((uintptr_t)p2_b % 8) == 0);
  assert((char *)p2_b >= (char *)p1_b + 8);
  allo_destroy(&a2);

  printf("Alignment test passed\n");
}

void test_zero_alloc(void) {
  ALLO_ALIGNED_BUF(c_buf, 2048);
  allo_t b;
  assert(make_fixed_buf_allocator(&b, c_buf, 2048) == ALLO_OK);

  allo_t a;
  assert(make_arena_allocator(&a, &b, 1024) == ALLO_OK);
  allo_t p;
  assert(make_pool_allocator(&p, &b, NULL, 64, 10) == ALLO_OK);

  // Behavior for 0 is now enforced to return NULL.
  assert(allo_alloc(&b, 0) == NULL);
  assert(allo_alloc(&a, 0) == NULL);
  assert(allo_alloc(&p, 0) == NULL);

  // free(NULL) should be a safe no-op.
  allo_free(&b, NULL, 0);
  allo_free(&a, NULL, 0);
  allo_free(&p, NULL, 0);

  allo_destroy(&b);
  allo_destroy(&a);
  allo_destroy(&p);
  printf("Zero allocation test passed\n");
}

int main(void) {
  test_alignment();
  test_zero_alloc();
  return 0;
}
