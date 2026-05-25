#include "allo.h"
#include "test_harness.h"

void test_page_allocator(void) {
  allo_t a;
  assert(make_page_allocator(&a) == ALLO_OK);
  void *p1 = allo_alloc(&a, 100);
  assert(p1 != NULL);
  memset(p1, 0xAA, 100);

  void *p2 = allo_alloc(&a, 5000); // Usually > 1 page
  assert(p2 != NULL);
  assert(p2 != p1);
  memset(p2, 0xBB, 5000);

  allo_free(&a, p1, 100);
  allo_free(&a, p2, 5000);
  allo_destroy(&a);
  printf("Page allocator test passed\n");
}

void test_page_validation(void) {
  printf("Testing Page Allocator: Validation\n");
  assert(make_page_allocator(NULL) == ALLO_ERR_INVAL);
  printf("Page validation tests passed!\n");
}

int main(void) {
  test_page_validation();
  test_page_allocator();
  return 0;
}
