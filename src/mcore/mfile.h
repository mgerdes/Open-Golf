#ifndef _MFILE_H
#define _MFILE_H

#define MFILE_MAX_PATH 1024
#define MFILE_MAX_NAME 256
#define MFILE_MAX_EXT 32

#include <stdbool.h>
#include <stdint.h>

#include "3rd_party/vec/vec.h"

typedef struct mfiletime {
    uint64_t unix_time;
} mfiletime_t;

typedef vec_t(mfiletime_t) vec_mfiletime_t;

// Returns < 0 if (t0 < t1), > 0 if (t0 > t1)
int mfiletime_cmp(mfiletime_t t0, mfiletime_t t1);

typedef struct mfile {
    char path[MFILE_MAX_PATH];
    char name[MFILE_MAX_NAME];
    char ext[MFILE_MAX_EXT];
    char dirname[MFILE_MAX_PATH];

    int data_len;
    char *data;

    int data_copy_len;
    char *data_copy_pos;
} mfile_t;

typedef vec_t(mfile_t) vec_mfile_t;

mfile_t mfile(const char *path);
mfile_t mfile_new_ext(mfile_t *file, const char *ext);
mfile_t mfile_append_extension(const char *path, const char *ext);
bool mfile_load_data(mfile_t *file);
void mfile_free_data(mfile_t *file);
bool mfile_set_data(mfile_t *file, char *data, int data_len);
bool mfile_copy_data(mfile_t *file, void *buffer, int num_bytes);
bool mfile_copy_line(mfile_t *file, char **line_buffer, int *line_buffer_len);
bool mfile_get_time(mfile_t *file, mfiletime_t *time);

typedef struct mdir {
    int num_files;
    mfile_t *files;
} mdir_t;

void mdir_init(mdir_t *dir, const char *dir_name, bool recurse);
void mdir_deinit(mdir_t *dir);

#endif
