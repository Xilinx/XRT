#ifndef XALLOCATOR_ALLOCATOR_H_
#define XALLOCATOR_ALLOCATOR_H_

#if defined (__cplusplus)
extern "C" {
#endif

#include<stddef.h>

void* xallocate(size_t len);
void xdeallocate(void *buf);

#if defined (__cplusplus)
}
#endif

#endif //XALLOCATOR_ALLOCATOR_H_ 
