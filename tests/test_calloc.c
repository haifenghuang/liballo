#include "allo.h"
#include "test_harness.h"

void test_calloc(void) {
  allo_t a;
  assert(make_c_allocator(&a) == ALLO_OK);

  size_t nmemb = 10;
  size_t size = sizeof(int);
  int *ptr = allo_calloc(&a, nmemb, size);
  assert(ptr != NULL);

  for (size_t i = 0; i < nmemb; i++) {
    assert(ptr[i] == 0);
  }

  allo_free(&a, ptr, nmemb * size);
  allo_destroy(&a);
  printf("allo_calloc test passed\n");
}

void test_calloc_overflow(void) {
  allo_t a;
  assert(make_c_allocator(&a) == ALLO_OK);

  // Use values that would overflow a size_t
  size_t nmemb = (size_t)-1 / 2 + 1;
  size_t size = 4;
  void *ptr = allo_calloc(&a, nmemb, size);
  assert(ptr == NULL);

  allo_destroy(&a);
  printf("allo_calloc overflow test passed\n");
}

int main(void) {
  test_calloc();
  test_calloc_overflow();
  return 0;
}
