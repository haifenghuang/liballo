#pragma once

// ASAN integration helpers
// We need to avoid using the Clang-specific __has_feature operator in
// preprocessor expressions unless we are sure the compiler supports it.
// GCC gained support for __has_feature in version 14; Clang exposes it
// as well. Prefer checking the compiler's sanitizer macro first. For
// Clang, test __has_feature inside a block guarded by __clang__.
#if defined(__SANITIZE_ADDRESS__)
  #include <sanitizer/asan_interface.h>
  #define ALLOC_POISON(addr, size) ASAN_POISON_MEMORY_REGION(addr, size)
  #define ALLOC_UNPOISON(addr, size) ASAN_UNPOISON_MEMORY_REGION(addr, size)
#elif defined(__clang__)
/* clang supports __has_feature */
  #if __has_feature(address_sanitizer)
    #include <sanitizer/asan_interface.h>
    #define ALLOC_POISON(addr, size) ASAN_POISON_MEMORY_REGION(addr, size)
    #define ALLOC_UNPOISON(addr, size) ASAN_UNPOISON_MEMORY_REGION(addr, size)
  #else
    #define ALLOC_POISON(addr, size) ((void)(addr), (void)(size))
    #define ALLOC_UNPOISON(addr, size) ((void)(addr), (void)(size))
  #endif
#elif defined(__GNUC__) && (__GNUC__ >= 14)
  /* GCC 14+ implements __has_feature compatibility */
  #include <sanitizer/asan_interface.h>
  #define ALLOC_POISON(addr, size) ASAN_POISON_MEMORY_REGION(addr, size)
  #define ALLOC_UNPOISON(addr, size) ASAN_UNPOISON_MEMORY_REGION(addr, size)
#else
  #define ALLOC_POISON(addr, size) ((void)(addr), (void)(size))
  #define ALLOC_UNPOISON(addr, size) ((void)(addr), (void)(size))
#endif
