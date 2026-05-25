#include "allo.h"
#include "test_harness.h"

void test_buffer_validation(void) {
  printf("Testing Fixed Buffer Allocator: Validation\n");
  allo_t a;
  assert(make_fixed_buf_allocator(NULL, NULL, 0) == ALLO_ERR_INVAL);
  assert(make_fixed_buf_allocator(&a, NULL, 0) == ALLO_ERR_INVAL);
  assert(make_fixed_buf_allocator(&a, (void *)1, 1024) ==
         ALLO_ERR_INVAL); // Unaligned

  char buffer[64];
  void *unaligned = (void *)((uintptr_t)buffer + 1);
  assert(make_fixed_buf_allocator(&a, unaligned, 63) == ALLO_ERR_INVAL);

  printf("Fixed Buffer validation tests passed!\n");
}

void test_buffer_allocator(void) {
  printf("Testing Fixed Buffer Allocator: Basic\n");
  ALLO_ALIGNED_BUF(buffer, 1024);
  allo_t a;
  assert(make_fixed_buf_allocator(&a, buffer, 1024) == ALLO_OK);

  void *p1 = allo_alloc(&a, 100);
  assert(p1 != NULL);
  // Metadata is in the struct now, not in the buffer
  assert(p1 == buffer);

  void *p2 = allo_alloc(&a, 100);
  assert(p2 != NULL);
  // 100 aligned up to 8 is 104
  assert(p2 == (char *)p1 + 104);

  void *p3 = allo_alloc(&a, 1000); // Should fail
  assert(p3 == NULL);

  allo_destroy(&a);
  printf("Fixed Buffer basic tests passed!\n");
}

void test_buffer_boundary(void) {
  printf("Testing Fixed Buffer Allocator: Boundary\n");
  char buffer[128];
  void *aligned = (void *)(((uintptr_t)buffer + 7) & ~((uintptr_t)7));
  allo_t a;
  assert(make_fixed_buf_allocator(&a, aligned, 64) == ALLO_OK);

  void *p = allo_alloc(&a, 64);
  assert(p != NULL);
  void *q = allo_alloc(&a, 1);
  assert(q == NULL);
  allo_destroy(&a);
  printf("Fixed Buffer boundary tests passed!\n");
}

int main(void) {
  test_buffer_validation();
  test_buffer_allocator();
  test_buffer_boundary();
  return 0;
}
