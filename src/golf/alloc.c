#include "golf/alloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mattiasgustavsson_libs/thread.h"
#include "golf/log.h"
#include "golf/map.h"

typedef struct _golf_alloc_info {
    const char *category;
    size_t size;
    struct _golf_alloc_info *prev, *next;
} _golf_alloc_info_t;

static thread_mutex_t _alloc_lock;
static _golf_alloc_info_t *head;

void golf_alloc_init(void) {
    thread_mutex_init(&_alloc_lock);
    head = malloc(sizeof(_golf_alloc_info_t));
    head->category = NULL;
    head->size = 0;
    head->prev = head;
    head->next = head;
}

void *golf_alloc_tracked(size_t size, const char *category) {
    thread_mutex_lock(&_alloc_lock);
    _golf_alloc_info_t *mem = malloc(sizeof(_golf_alloc_info_t) + size);
    mem->category = category;
    mem->size = size;
    mem->prev = head;
    mem->next = head->next;
    mem->next->prev = mem;
    head->next = mem;
    thread_mutex_unlock(&_alloc_lock);
    return mem + 1;
}

void *golf_realloc_tracked(void *mem, size_t size, const char *category) {
    if (mem == NULL) {
        return golf_alloc_tracked(size, category);
    }
    else if (size == 0) {
        golf_free_tracked(mem);
        return NULL;
    }
    else {
        _golf_alloc_info_t *info = (_golf_alloc_info_t*)mem - 1;
        if (size <= info->size) {
            return mem;
        }
        else {
            void *mem2 = golf_alloc_tracked(size, category);
            if (mem2) {
                memcpy(mem2, mem, info->size);
                golf_free_tracked(mem);
            }
            return mem2;
        }
    }
}

void golf_free_tracked(void *mem) {
    if (!mem) {
        return;
    }

    thread_mutex_lock(&_alloc_lock);
    _golf_alloc_info_t *info = (_golf_alloc_info_t*)mem - 1;
    info->prev->next = info->next;
    info->next->prev = info->prev;
    free(info);
    thread_mutex_unlock(&_alloc_lock);
}

void golf_alloc_get_debug_info(size_t *total_size) {
    thread_mutex_lock(&_alloc_lock);
    *total_size = 0;
    _golf_alloc_info_t *info = head;
    while (info->next != head) {
        info = info->next;
        *total_size += info->size;
    }
    thread_mutex_unlock(&_alloc_lock);
}

void golf_debug_print_allocations(void) {
    golf_log_note("=====Memory Allocations=====");
    _golf_alloc_info_t *info = head;
    map_int_t category_size;
    map_init(&category_size, "alloc");
    while (info->next != head) {
        info = info->next;
        int *size;
        if (info->category) {
            size = map_get(&category_size, info->category);
        }
        else {
            size = map_get(&category_size, "null");
        }
        if (size) {
            *size += (int)info->size;
        }
        else {
            if (info->category) {
                map_set(&category_size, info->category, info->size);
            }
            else {
                map_set(&category_size, "null", info->size);
            }
        }
    }

    {
        const char *key;
        map_iter_t iter = map_iter(&category_size);

        while ((key = map_next(&category_size, &iter))) {
            printf("%s -> %d\n", key, *map_get(&category_size, key));
        }
    }

    map_deinit(&category_size);
}
