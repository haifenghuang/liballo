#include "allo.h"
#include "test_harness.h"

void test_realloc_common(allo_t *a, const char *name) {
  printf("Testing realloc for %s\n", name);

  // 1. Initial allocation
  void *p = allo_alloc(a, 100);
  assert(p != NULL);
  memset(p, 0xAA, 100);

  // 2. Shrink
  void *p2 = allo_realloc(a, p, 100, 50);
  assert(p2 != NULL);
  // For most of our allocators, shrink might return the same pointer
  for (int i = 0; i < 50; i++) {
    assert(((unsigned char *)p2)[i] == 0xAA);
  }

  // 3. Grow
  void *p3 = allo_realloc(a, p2, 50, 200);
  assert(p3 != NULL);
  for (int i = 0; i < 50; i++) {
    assert(((unsigned char *)p3)[i] == 0xAA);
  }
  memset(p3, 0xBB, 200);

  // 4. Realloc NULL (should act like alloc)
  void *p4 = allo_realloc(a, NULL, 0, 80);
  assert(p4 != NULL);
  memset(p4, 0xCC, 80);

  // 5. Realloc to 0 (should act like free)
  void *p5 = allo_realloc(a, p4, 80, 0);
  assert(p5 == NULL);

  allo_free(a, p3, 200);
  allo_destroy(a);
  printf("Realloc test for %s passed\n", name);
}

void test_buffer_realloc_inplace(void) {
  ALLO_ALIGNED_BUF(buffer, 1024);
  allo_t a;
  assert(make_fixed_buf_allocator(&a, buffer, 1024) == ALLO_OK);

  void *p1 = allo_alloc(&a, 100);
  void *p2 = allo_realloc(&a, p1, 100, 200);
  assert(p1 == p2); // Should be in-place for last allocation

  (void)allo_alloc(&a, 50); // Just to take space
  void *p4 = allo_realloc(&a, p2, 200, 300);
  assert(p4 != p2); // Should NOT be in-place (p3 is in the way)

  allo_destroy(&a);
  printf("Buffer in-place realloc test passed\n");
}

int main(void) {
#ifndef ALLO_NOSTDLIB
  allo_t c;
  assert(make_c_allocator(&c) == ALLO_OK);
  test_realloc_common(&c, "C Allocator");
#endif

  char buf[2048];
  allo_t b;
  assert(make_fixed_buf_allocator(&b, buf, 2048) == ALLO_OK);
  test_realloc_common(&b, "Fixed Buffer");

#ifndef ALLO_NOSTDLIB
  allo_t root;
  assert(make_c_allocator(&root) == ALLO_OK);
  allo_t arena;
  assert(make_arena_allocator(&arena, &root, 1024) == ALLO_OK);
  test_realloc_common(&arena, "Arena");

  allo_t pool;
  assert(make_pool_allocator(&pool, &root, NULL, 256, 10) == ALLO_OK);
  // Pool only supports realloc within block_size
  void *pp = allo_alloc(&pool, 100);
  void *pp2 = allo_realloc(&pool, pp, 100, 200);
  assert(pp == pp2);
  allo_destroy(&pool);

  allo_t page;
  assert(make_page_allocator(&page) == ALLO_OK);
  test_realloc_common(&page, "Page Allocator");
#endif

  test_buffer_realloc_inplace();

  return 0;
}
