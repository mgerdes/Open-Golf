#ifndef _GOLF_THREAD_H
#define _GOLF_THREAD_H

#include <stdint.h>

typedef void* golf_thread_t;

#if GOLF_PLATFORM_WINDOWS

typedef int golf_thread_result_t;
#define GOLF_THREAD_RESULT_SUCCESS 1

#elif GOLF_PLATFORM_LINUX || GOLF_PLATFORM_IOS || GOLF_PLATFORM_ANDROID || GOLF_PLATFORM_EMSCRIPTEN

typedef void* golf_thread_result_t;
#define GOLF_THREAD_RESULT_SUCCESS ((void*)(uintptr_t)1)

#else
#error Unknown platform
#endif

golf_thread_t golf_thread_create(golf_thread_result_t (*proc)(void*), void *user_data, const char *name);
void golf_thread_destroy(golf_thread_t thread);
int golf_thread_join(golf_thread_t thread);

typedef union golf_mutex {
    void *align;
    char data[64];
} golf_mutex_t; 

void golf_mutex_init(golf_mutex_t *mutex);
void golf_mutex_deinit(golf_mutex_t *mutex);
void golf_mutex_lock(golf_mutex_t *mutex);
void golf_mutex_unlock(golf_mutex_t *mutex);

typedef union golf_thread_timer_t {
    void *data;
    char d[8];
} golf_thread_timer_t;

void golf_thread_timer_init(golf_thread_timer_t* timer);
void golf_thread_timer_deinit(golf_thread_timer_t* timer);
void golf_thread_timer_wait(golf_thread_timer_t* timer, uint64_t nanoseconds);

#endif
