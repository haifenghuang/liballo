# liballo

`liballo` is a lightweight, extensible memory allocator library for C. It provides a unified interface for various memory management strategies, ranging from simple standard library wrappers to high-performance arena and pool allocators.

## Usage Example

```c
#include "allo.h"

int main() {
    // Create a growable arena backed by the standard C allocator
    allo_t c_alloc = make_c_allocator();
    allo_t arena = make_arena_allocator(&c_alloc, 4096);

    // Allocate memory
    void *ptr = allo_alloc(&arena, 128);
    
    // Reallocate
    ptr = allo_realloc(&arena, ptr, 128, 256);

    // Arena reclaims everything at once
    allo_destroy(&arena);
    return 0;
}
```

## Getting Started

### Prerequisites

- [Meson](https://mesonbuild.com/) (>= 1.3.0)
- [Ninja](https://ninja-build.org/)
- A C11-compliant compiler (Clang recommended for best diagnostic support)

### Building

```bash
# Setup the build directory
meson setup build

# Compile the library and tests
ninja -C build
```

### Running Tests

```bash
# Run all unit tests
meson test -C build
```

## Features

- **Unified Interface**: All allocators share the same `allo_t` structure and API.
- **Pluggable Architecture**: Easily chain allocators (e.g., an Arena backed by a Page allocator).
- **Multiple Allocator Types**:
  - **C Allocator**: A thin wrapper around `malloc` and `free`.
  - **Page Allocator**: Directly requests pages from the OS (`mmap`). Each allocation is page-aligned.
  - **Fixed Buffer Allocator**: A fast, linear allocator that operates on a pre-allocated memory block.
  - **Arena Allocator**: A growable region-based allocator that handles allocations in large blocks. Reclaims memory all at once.
  - **Pool Allocator**: Manages fixed-size blocks with a free list for $O(1)$ allocation and deallocation.
- **No-Stdlib Support**: Can be compiled without the standard C library (`ALLO_NOSTDLIB`) for embedded or kernel-space use.

## Advanced Testing & Verification

`liballo` is built for stability and includes a comprehensive suite of verification tools.

### Sanitizers and Valgrind

The project is fully instrumented for ASan, UBSan, and MSan.

```bash
# Run tests under Valgrind
meson test -C build --wrapper valgrind
```

### Fuzzing (AFL++)

We provide a high-entropy fuzzing harness and an automation script for AFL++:

```bash
# 1. Setup the AFL++ build environment and seeds
./scripts/fuzz.sh

# 2. Start the fuzzer
AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 AFL_SKIP_CPUFREQ=1 \
afl-fuzz -i fuzz_in -o fuzz_out -- ./build-afl/tests/fuzz_allocator @@
```

The fuzzer includes **data integrity verification** using magic bytes to catch overlapping allocations and memory corruption bugs.

## License

MIT
