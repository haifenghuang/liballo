#include "allo.h"
#include "test_harness.h"

#include <threads.h>

#define THREAD_COUNT 4
#define ALLOC_COUNT 1000

struct thread_data {
  allo_t *a;
  size_t size;
};

static int thread_func(void *arg) {
  struct thread_data *data = (struct thread_data *)arg;
  void *ptrs[ALLOC_COUNT];

  for (int i = 0; i < ALLOC_COUNT; i++) {
    ptrs[i] = allo_alloc(data->a, data->size);
    assert(ptrs[i] != NULL);
  }

  for (int i = 0; i < ALLOC_COUNT; i++) {
    allo_free(data->a, ptrs[i], data->size);
  }

  return 0;
}

int main(void) {
  allo_t base, mt;
  make_c_allocator(&base);
  assert(make_mtx_allocator(&mt, &base) == ALLO_OK);

  thrd_t threads[THREAD_COUNT];
  struct thread_data data = {&mt, 16};

  for (int i = 0; i < THREAD_COUNT; i++) {
    assert(thrd_create(&threads[i], thread_func, &data) == thrd_success);
  }

  for (int i = 0; i < THREAD_COUNT; i++) {
    thrd_join(threads[i], NULL);
  }

  allo_destroy(&mt);
  allo_destroy(&base);

  return 0;
}
