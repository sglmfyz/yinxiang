#include "common.h"


#define ALIGNED_HEAP_SIZE (HEAP_SIZE - HEAP_ALIGNMENT)
static uint8_t heap[ HEAP_SIZE ];
static size_t next_free_bytes = ( size_t ) 0;
static uint8_t *aligned_heap = NULL;

void *pMalloc(int size)
{
    void *rt = NULL;
    
#if HEAP_ALIGNMENT != 1
    if (size & HEAP_ALIGN_MASK) {
        size += HEAP_ALIGNMENT - (size & HEAP_ALIGN_MASK);
    }
#endif
    if (aligned_heap == NULL) {
        aligned_heap =  (uint8_t *)(((unsigned long)&heap[ HEAP_ALIGNMENT ]) & ( ( unsigned long ) ~HEAP_ALIGN_MASK ));
    }
    if (next_free_bytes + size < ALIGNED_HEAP_SIZE && 
        next_free_bytes + size > next_free_bytes) {
        rt = aligned_heap + next_free_bytes;
        next_free_bytes += size;
    }
    return rt;
}

uint32_t heap_free_bytes()
{
    return ALIGNED_HEAP_SIZE - next_free_bytes;
}
uint32_t heap_used_bytes()
{
    return next_free_bytes;
}
uint32_t heap_total_bytes()
{
    return ALIGNED_HEAP_SIZE;
}
