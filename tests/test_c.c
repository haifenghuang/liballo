#include "allo.h"
#include "test_harness.h"

void test_c_allocator(void) {
  allo_t a;
  assert(make_c_allocator(&a) == ALLO_OK);
  void *p1 = allo_alloc(&a, 100);
  assert(p1 != NULL);
  memset(p1, 0xAA, 100);
  allo_free(&a, p1, 100);
  allo_destroy(&a);
  printf("C allocator test passed\n");
}

void test_c_validation(void) {
  printf("Testing C Allocator: Validation\n");
  assert(make_c_allocator(NULL) == ALLO_ERR_INVAL);
  printf("C validation tests passed!\n");
}

int main(void) {
  test_c_validation();
  test_c_allocator();
  return 0;
}
