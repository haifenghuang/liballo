#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "allo.h"

// We need a reasonable upper bound to avoid enormous stack allocations in the harness,
// but the *actual* limits used in each run will be determined by the input.
#define ABSOLUTE_MAX_POINTERS 128
#define ABSOLUTE_MAX_ALLOC 65536

typedef enum {
    OP_ALLOC = 0,
    OP_FREE = 1,
    OP_REALLOC = 2,
    OP_COUNT
} operation_t;

typedef struct {
    void *ptr;
    size_t size;
    uint8_t magic; // For data integrity verification
} tracker_t;

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    // Need at least enough bytes for global config (selector + max_ptrs + max_alloc)
    if (Size < 6) return 0;

    // --- 1. Global Configuration via Input ---
    uint8_t selector = Data[0] % 5;
    
    // Determine how many pointers we will track in this run (1 to ABSOLUTE_MAX_POINTERS)
    size_t active_max_pointers = (Data[1] % ABSOLUTE_MAX_POINTERS) + 1;
    
    // Determine the maximum allocation size for this run (1 to ABSOLUTE_MAX_ALLOC)
    size_t active_max_alloc = ((Data[2] << 8) | Data[3]) % ABSOLUTE_MAX_ALLOC + 1;
    
    size_t offset = 4;

    allo_t a;
    allo_t child;
    void *backing_buffer = NULL;
    int is_complex = 0;

    // --- 2. Allocator Instantiation via Input ---
    switch (selector) {
        case 0: // C Allocator
            a = make_c_allocator();
            break;
        case 1: // Page Allocator
            a = make_page_allocator();
            break;
        case 2: // Fixed Buffer Allocator
        {
            if (offset + 2 > Size) return 0;
            // Buffer size between 1 and active_max_alloc * active_max_pointers
            size_t buf_size = ((Data[offset] << 8) | Data[offset+1]) % (active_max_alloc * active_max_pointers + 1) + 1;
            offset += 2;
            backing_buffer = malloc(buf_size);
            a = make_fixed_buf_allocator(backing_buffer, buf_size);
            is_complex = 1;
            break;
        }
        case 3: // Arena Allocator
        {
            if (offset + 2 > Size) return 0;
            // Block size between 1 and 65536
            size_t block_size = ((Data[offset] << 8) | Data[offset+1]) + 1;
            offset += 2;
            child = make_c_allocator();
            a = make_arena_allocator(&child, block_size);
            is_complex = 2;
            break;
        }
        case 4: // Pool Allocator
        {
            if (offset + 4 > Size) return 0;
            // Block size between 1 and 4096
            size_t pool_block_size = ((Data[offset] << 8) | Data[offset+1]) % 4096 + 1;
            offset += 2;
            // Total blocks between 1 and 1024
            size_t total_blocks = ((Data[offset] << 8) | Data[offset+1]) % 1024 + 1;
            offset += 2;
            child = make_c_allocator();
            a = make_pool_allocator(&child, NULL, pool_block_size, total_blocks);
            is_complex = 3;
            break;
        }
        default:
            return 0;
    }

    // --- 3. State Setup ---
    tracker_t tracked[ABSOLUTE_MAX_POINTERS];
    memset(tracked, 0, sizeof(tracked));

    // --- 4. Operation Loop ---
    while (offset < Size) {
        uint8_t op_byte = Data[offset++];
        operation_t op = (op_byte & 0x03) % OP_COUNT;
        
        // Target an index within our currently active max pointers
        uint8_t index = (op_byte >> 2) % active_max_pointers;

        if (op == OP_ALLOC) {
            if (offset + 2 > Size) break;
            size_t alloc_size = ((Data[offset] << 8) | Data[offset+1]) % active_max_alloc;
            offset += 2;

            // Verify integrity before overwriting
            if (tracked[index].ptr && tracked[index].size > 0) {
                uint8_t *mem = (uint8_t *)tracked[index].ptr;
                for (size_t i = 0; i < tracked[index].size; i++) {
                    if (mem[i] != tracked[index].magic) {
                        abort(); // Data corruption detected
                    }
                }
                allo_free(&a, tracked[index].ptr);
            }
            
            tracked[index].ptr = allo_alloc(&a, alloc_size);
            tracked[index].size = alloc_size;
            tracked[index].magic = op_byte; // Use op_byte as unique magic for this allocation
            
            if (tracked[index].ptr && alloc_size > 0) {
                memset(tracked[index].ptr, tracked[index].magic, alloc_size);
            }
        } else if (op == OP_FREE) {
            if (tracked[index].ptr) {
                // Verify integrity before freeing
                if (tracked[index].size > 0) {
                    uint8_t *mem = (uint8_t *)tracked[index].ptr;
                    for (size_t i = 0; i < tracked[index].size; i++) {
                        if (mem[i] != tracked[index].magic) {
                            abort(); // Data corruption detected
                        }
                    }
                }
                allo_free(&a, tracked[index].ptr);
                tracked[index].ptr = NULL;
                tracked[index].size = 0;
            }
        } else if (op == OP_REALLOC) {
            if (offset + 2 > Size) break;
            size_t new_size = ((Data[offset] << 8) | Data[offset+1]) % active_max_alloc;
            offset += 2;

            // Verify integrity before reallocating
            if (tracked[index].ptr && tracked[index].size > 0) {
                uint8_t *mem = (uint8_t *)tracked[index].ptr;
                for (size_t i = 0; i < tracked[index].size; i++) {
                    if (mem[i] != tracked[index].magic) {
                        abort(); // Data corruption detected
                    }
                }
            }

            void *new_ptr = allo_realloc(&a, tracked[index].ptr, tracked[index].size, new_size);
            
            if (new_ptr || new_size == 0) {
                tracked[index].ptr = new_ptr;
                tracked[index].size = new_size;
                tracked[index].magic = op_byte; // Update magic for new state
                
                if (tracked[index].ptr && new_size > 0) {
                    // Re-fill the entire new block to ensure integrity moving forward
                    memset(tracked[index].ptr, tracked[index].magic, new_size);
                }
            }
        }
    }

    // --- 5. Teardown ---
    for (int i = 0; i < (int)active_max_pointers; i++) {
        if (tracked[i].ptr) {
            // Final integrity check before teardown
            if (tracked[i].size > 0) {
                uint8_t *mem = (uint8_t *)tracked[i].ptr;
                for (size_t j = 0; j < tracked[i].size; j++) {
                    if (mem[j] != tracked[i].magic) {
                        abort();
                    }
                }
            }
            allo_free(&a, tracked[i].ptr);
        }
    }

    allo_destroy(&a);
    if (is_complex == 1) {
        free(backing_buffer);
    } else if (is_complex >= 2) {
        allo_destroy(&child);
    }

    return 0;
}

#ifndef __AFL_COMPILER
#include <stdio.h>
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(size);
    if (fread(data, 1, size, f) != (size_t)size) {
        perror("fread");
        return 1;
    }
    fclose(f);
    LLVMFuzzerTestOneInput(data, size);
    free(data);
    return 0;
}
#endif
