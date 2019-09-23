#include "xallocator.h"
#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdlib.h>

void* xallocate(size_t len) {
    void* ptr = nullptr;
    if (posix_memalign(&ptr,4096,len))
        return nullptr;
    return ptr;
}

void xdeallocate(void *buf) {
    free(buf);
}
