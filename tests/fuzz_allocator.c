#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "allo.h"

#define MAX_POINTERS 16
#define MAX_ALLOC_SIZE 8192

typedef enum {
    OP_ALLOC = 0,
    OP_FREE = 1,
    OP_REALLOC = 2,
    OP_COUNT
} operation_t;

typedef struct {
    void *ptr;
    size_t size;
} tracker_t;

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    if (Size < 1) return 0;

    allo_t a = make_c_allocator();
    tracker_t tracked[MAX_POINTERS];
    memset(tracked, 0, sizeof(tracked));

    size_t offset = 0;
    while (offset < Size) {
        uint8_t op_byte = Data[offset++];
        operation_t op = (op_byte & 0x03) % OP_COUNT;
        uint8_t index = (op_byte >> 2) % MAX_POINTERS;

        if (op == OP_ALLOC) {
            if (offset + 2 > Size) break;
            size_t alloc_size = ((Data[offset] << 8) | Data[offset+1]) % MAX_ALLOC_SIZE;
            offset += 2;

            if (tracked[index].ptr) {
                allo_free(&a, tracked[index].ptr);
            }
            tracked[index].ptr = allo_alloc(&a, alloc_size);
            tracked[index].size = alloc_size;
            if (tracked[index].ptr && alloc_size > 0) {
                memset(tracked[index].ptr, 0xAA, alloc_size);
            }
        } else if (op == OP_FREE) {
            if (tracked[index].ptr) {
                allo_free(&a, tracked[index].ptr);
                tracked[index].ptr = NULL;
                tracked[index].size = 0;
            }
        } else if (op == OP_REALLOC) {
            if (offset + 2 > Size) break;
            size_t new_size = ((Data[offset] << 8) | Data[offset+1]) % MAX_ALLOC_SIZE;
            offset += 2;

            void *new_ptr = allo_realloc(&a, tracked[index].ptr, tracked[index].size, new_size);
            if (new_ptr || new_size == 0) {
                tracked[index].ptr = new_ptr;
                tracked[index].size = new_size;
                if (tracked[index].ptr && new_size > 0) {
                    memset(tracked[index].ptr, 0xBB, new_size);
                }
            }
        }
    }

    for (int i = 0; i < MAX_POINTERS; i++) {
        if (tracked[i].ptr) {
            allo_free(&a, tracked[i].ptr);
        }
    }

    allo_destroy(&a);
    return 0;
}
