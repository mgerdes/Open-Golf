#ifndef _GOLF_FILE_H
#define _GOLF_FILE_H

#define GOLF_FILE_MAX_PATH 1024
#define GOLF_FILE_MAX_NAME 256
#define GOLF_FILE_MAX_EXT 32

#include <stdbool.h>
#include <stdint.h>

#include "common/map.h"
#include "common/vec.h"

typedef struct golf_file {
    char path[GOLF_FILE_MAX_PATH];
    char name[GOLF_FILE_MAX_NAME];
    char ext[GOLF_FILE_MAX_EXT];
    char dirname[GOLF_FILE_MAX_PATH];
} golf_file_t;

typedef vec_t(golf_file_t) vec_golf_file_t;
typedef map_t(golf_file_t) map_golf_file_t;

golf_file_t golf_file(const char *path);
golf_file_t golf_file_new_ext(golf_file_t *file, const char *ext);
golf_file_t golf_file_append_extension(const char *path, const char *ext);
uint64_t golf_file_get_time(const char *path);
bool golf_file_load_data(const char *path, char **data, int *data_len); 
const char *golf_file_copy_line(const char *string, char **line_buffer, int *line_buffer_len);

typedef struct golf_dir {
    int num_files;
    golf_file_t *files;
} golf_dir_t;

void golf_dir_recurse(const char *dir_name, void (*fn)(const char *file_path, void *udata), void *udata);
void golf_dir_init(golf_dir_t *dir, const char *dir_name, bool recurse);
void golf_dir_deinit(golf_dir_t *dir);

#endif
