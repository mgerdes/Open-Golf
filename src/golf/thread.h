#ifndef _GOLF_THREAD_H
#define _GOLF_THREAD_H

typedef void* golf_thread_t;

golf_thread_t golf_thread_create(int (*proc)(void*), void *user_data, const char *name);
void golf_thread_destroy(golf_thread_t thread);
int golf_thread_join(golf_thread_t thread);

typedef struct golf_mutex {
    void *align;
    char data[64];
} golf_mutex_t; 

void golf_mutex_init(golf_mutex_t *mutex);
void golf_mutex_deinit(golf_mutex_t *mutex);
void golf_mutex_lock(golf_mutex_t *mutex);
void golf_mutex_unlock(golf_mutex_t *mutex);

#endif
