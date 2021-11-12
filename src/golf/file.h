#ifndef _GOLF_FILE_H
#define _GOLF_FILE_H

#define GOLF_FILE_MAX_PATH 1024
#define GOLF_FILE_MAX_NAME 256
#define GOLF_FILE_MAX_EXT 32

#include <stdbool.h>
#include <stdint.h>

#include "vec/vec.h"

typedef struct golf_filetime {
    uint64_t unix_time;
} golf_filetime_t;

typedef vec_t(golf_filetime_t) vec_golf_filetime_t;

// Returns < 0 if (t0 < t1), > 0 if (t0 > t1)
int golf_filetime_cmp(golf_filetime_t t0, golf_filetime_t t1);

typedef struct golf_file {
    char path[GOLF_FILE_MAX_PATH];
    char name[GOLF_FILE_MAX_NAME];
    char ext[GOLF_FILE_MAX_EXT];
    char dirname[GOLF_FILE_MAX_PATH];

    int data_len;
    char *data;

    int data_copy_len;
    char *data_copy_pos;
} golf_file_t;

typedef vec_t(golf_file_t) vec_golf_file_t;

golf_file_t golf_file(const char *path);
golf_file_t golf_file_new_ext(golf_file_t *file, const char *ext);
golf_file_t golf_file_append_extension(const char *path, const char *ext);
bool golf_file_load_data(golf_file_t *file);
void golf_file_free_data(golf_file_t *file);
bool golf_file_set_data(golf_file_t *file, char *data, int data_len);
bool golf_file_copy_data(golf_file_t *file, void *buffer, int num_bytes);
bool golf_file_copy_line(golf_file_t *file, char **line_buffer, int *line_buffer_len);
bool golf_file_get_time(golf_file_t *file, golf_filetime_t *time);
int golf_file_cmp_time(golf_file_t *f0, golf_file_t *f1);

typedef struct golf_dir {
    int num_files;
    golf_file_t *files;
} golf_dir_t;

void golf_dir_init(golf_dir_t *dir, const char *dir_name, bool recurse);
void golf_dir_deinit(golf_dir_t *dir);

#endif
