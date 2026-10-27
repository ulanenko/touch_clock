#pragma once

#include <stdlib.h>
#include <string.h>

#define MALLOC_CAP_SPIRAM (1 << 10)
#define MALLOC_CAP_INTERNAL (1 << 11)
#define MALLOC_CAP_DMA (1 << 3)
#define MALLOC_CAP_8BIT (1 << 2)
#define MALLOC_CAP_DEFAULT (1 << 12)

static inline void *heap_caps_malloc(size_t size, unsigned caps)
{
    (void)caps;
    return malloc(size);
}

static inline void *heap_caps_calloc(size_t n, size_t size, unsigned caps)
{
    (void)caps;
    return calloc(n, size);
}

static inline void *heap_caps_aligned_alloc(size_t alignment, size_t size, unsigned caps)
{
    void *ptr = NULL;
    (void)caps;
    if (posix_memalign(&ptr, alignment < sizeof(void *) ? sizeof(void *) : alignment, size) != 0) {
        return NULL;
    }
    return ptr;
}

static inline void *heap_caps_aligned_calloc(size_t alignment, size_t n, size_t size, unsigned caps)
{
    void *ptr = heap_caps_aligned_alloc(alignment, n * size, caps);
    if (ptr != NULL) {
        memset(ptr, 0, n * size);
    }
    return ptr;
}

static inline void heap_caps_free(void *ptr)
{
    free(ptr);
}

static inline size_t heap_caps_get_free_size(unsigned caps)
{
    (void)caps;
    return 8u * 1024u * 1024u;
}

static inline size_t heap_caps_get_largest_free_block(unsigned caps)
{
    (void)caps;
    return 4u * 1024u * 1024u;
}
