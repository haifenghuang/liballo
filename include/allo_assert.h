#ifndef ALLO_ASSERT_H
#define ALLO_ASSERT_H

#ifndef ALLO_FREESTANDING
  #include <assert.h>
  #define ALLO_ASSERT(x) assert(x)
#else
  #define ALLO_ASSERT(x) ((void)0)
  #ifndef static_assert
    #define static_assert _Static_assert
  #endif
#endif

#endif /* ALLO_ASSERT_H */
