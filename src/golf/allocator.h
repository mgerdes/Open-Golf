#ifndef _GOLF_ALLOCATOR_H
#define _GOLF_ALLOCATOR_H

#include <stddef.h>

typedef struct golf_allocator {
    void *(*alloc)(size_t);
    void (*free)(void*);
} golf_allocator_t;

#define GOLF_DEFAULT_ALLOCATOR ((golf_allocator_t) { .alloc = malloc, .free = free })

#endif
