#ifndef _GOLF_ALLOC_H
#define _GOLF_ALLOC_H

#include <stdint.h>
#include <stddef.h>

#define golf_alloc(size) golf_alloc_tracked((size), (const char*)__FILE__)
#define golf_calloc(n, size) golf_alloc_tracked(((n)*(size)), (const char*)__FILE__)
#define golf_realloc(mem, size) golf_realloc_tracked((mem), (size), (const char*)__FILE__)
#define golf_free(mem) golf_free_tracked((mem))

void golf_alloc_init(void);
void *golf_alloc_tracked(size_t size, const char *category);
void *golf_realloc_tracked(void *mem, size_t size, const char *category);
void golf_free_tracked(void *mem);
void golf_alloc_get_debug_info(size_t *total_size);
void golf_debug_print_allocations(void);

#endif
