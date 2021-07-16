#include "mfile.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int mfiletime_cmp(mfiletime_t time0, mfiletime_t time1) {
    return (int)difftime(time0.time, time1.time);
}

mfile_t mfile(const char *path) {
    mfile_t file;
    strncpy(file.path, path, MFILE_MAX_PATH);
    file.path[MFILE_MAX_PATH - 1] = 0;

    int i = (int) strlen(path);
    while (i >= 0 && path[i--] != '.');
    if (i < 0) {
        i = (int) strlen(path);
        file.ext[0] = 0;
    }
    else {
        strncpy(file.ext, path + i + 1, MFILE_MAX_EXT);
        file.ext[MFILE_MAX_EXT - 1] = 0;
    }

    while (i >= 0 && path[i--] != '/');
    if (i < 0) {
        strncpy(file.name, path + i + 1, MFILE_MAX_NAME);
        file.name[MFILE_MAX_NAME - 1] = 0;
    }
    else {
        strncpy(file.name, path + i + 2, MFILE_MAX_NAME);
        file.name[MFILE_MAX_NAME - 1] = 0;
    }

    int path_len = (int) strlen(file.path);
    int name_len = (int) strlen(file.name);
    strncpy(file.dirname, file.path, MFILE_MAX_PATH);
    file.dirname[path_len - name_len] = 0;

    file.data_len = 0;
    file.data = NULL;

    file.data_copy_len = 0;
    file.data_copy_pos = NULL;

    return file;
}

mfile_t mfile_new_ext(mfile_t *file, const char *ext) {
    mfile_t new_file = *file;

    int ext_len = (int) strlen(new_file.ext);

    int path_len = (int) strlen(new_file.path);
    strncpy(new_file.path + path_len - ext_len, ext, MFILE_MAX_PATH - path_len + ext_len);
    new_file.path[MFILE_MAX_PATH - 1] = 0;

    int name_len = (int) strlen(new_file.name);
    strncpy(new_file.name + name_len - ext_len, ext, MFILE_MAX_NAME - name_len + ext_len);
    new_file.name[MFILE_MAX_NAME - 1] = 0;

    strncpy(new_file.ext, ext, MFILE_MAX_EXT);
    new_file.ext[MFILE_MAX_EXT - 1] = 0;

    new_file.data_len = 0;
    new_file.data = NULL;

    new_file.data_copy_len = 0;
    new_file.data_copy_pos = NULL;

    return new_file;
}

bool mfile_load_data(mfile_t *file) {
    FILE *f = fopen(file->path, "rb");
    if (!f) {
        return false;
    }

    fseek(f, 0, SEEK_END);
    int num_bytes = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *bytes = (char *)malloc(num_bytes + 1);
    bytes[num_bytes] = 0;
    int ret = (int) fread(bytes, sizeof(char), num_bytes, f);
    if (ret == -1) {
        fclose(f);
        free(bytes);
        return false;
    }
    fclose(f);

    file->data_len = num_bytes;
    file->data = bytes;
    file->data_copy_len = num_bytes;
    file->data_copy_pos = bytes;
    return true;
}

void mfile_free_data(mfile_t *file) {
    free(file->data);
    file->data_len = 0;
    file->data = NULL;
    file->data_copy_len = 0;
    file->data_copy_pos = NULL;
}

bool mfile_set_data(mfile_t *file, char *data, int data_len) {
    if (file->data) {
        mfile_free_data(file);
    }

    FILE *f = fopen(file->path, "wb");
    if (f) {
        fwrite(data, sizeof(char), data_len, f);
        fclose(f);

        file->data_len = data_len;
        file->data = data;
        file->data_copy_len = data_len;
        file->data_copy_pos = data;
        return true;
    }
    else {
        return false;
    }
}

bool mfile_copy_data(mfile_t *file, void *buffer, int num_bytes) {
    if (file->data_copy_len < num_bytes) {
        return false;
    }

    memcpy(buffer, file->data_copy_pos, num_bytes);
    file->data_copy_len -= num_bytes;
    file->data_copy_pos += num_bytes;
    return true;
}

static void grow_buffer(char **buffer, int *buffer_len) {
    int new_buffer_len = 2 * (*buffer_len + 1);
    char *new_buffer = malloc(new_buffer_len);
    memcpy(new_buffer, *buffer, *buffer_len);

    if (*buffer) {
        free(*buffer);
    }
    *buffer_len = new_buffer_len;
    *buffer = new_buffer;
}

bool mfile_copy_line(mfile_t *file, char **line_buffer, int *line_buffer_len) {
    if (file->data_copy_len == 0) {
        return false;
    }

    int i = 0;
    while (file->data_copy_pos[i] != '\n' && i < file->data_copy_len) {
        if (i == *line_buffer_len) {
            grow_buffer(line_buffer, line_buffer_len);
        }
        (*line_buffer)[i] = file->data_copy_pos[i];
        i++;
    }
    if (i > 0 && (*line_buffer)[i - 1] == '\r') {
        (*line_buffer)[i - 1] = 0;
    }

    file->data_copy_pos += i;
    file->data_copy_len -= i;

    // Skip over the new line if not at the end of the file.
    if (i < file->data_copy_len) {
        file->data_copy_pos += 1;
        file->data_copy_len -= 1;
    }

    // Allocate room for the null character if needed.
    if (i == *line_buffer_len) {
        grow_buffer(line_buffer, line_buffer_len);
    }

    (*line_buffer)[i] = 0;

    return true;
}

bool mfile_get_time(mfile_t *file, mfiletime_t *time) {
    struct stat info;
    if (stat(file->path, &info)) {
        return false;
    }
    time->time = info.st_mtime;
    return true;
}

static void _directory_add_file(const char *file_path, void *data) {
    mfile_t file = mfile(file_path);
    vec_mfile_t *vec = (vec_mfile_t*) data;
    vec_push(vec, file);
}

static void _directory_recurse(const char *dir_name, void (*fn)(const char *file_path, void*), void *data, bool recurse) {
    DIR *dir_ptr = opendir(dir_name);
    if (dir_ptr == NULL) {
        return;
    }

    int dir_name_len = (int) strlen(dir_name);

    struct dirent *entry = NULL;
    while ((entry = readdir(dir_ptr))) {
        char file_path[MFILE_MAX_PATH];
        strncpy(file_path, dir_name, MFILE_MAX_PATH);
        file_path[MFILE_MAX_PATH - 1] = 0;

        strncat(file_path, "/", MFILE_MAX_PATH - dir_name_len);
        file_path[MFILE_MAX_PATH - 1] = 0;

        strncat(file_path, entry->d_name, MFILE_MAX_PATH - dir_name_len - 1);
        file_path[MFILE_MAX_PATH - 1] = 0;

        struct stat info;
        stat(file_path, &info);
        if (S_ISDIR(info.st_mode)) {
            if ((strcmp(entry->d_name, ".") != 0) &&
                    (strcmp(entry->d_name, "..") != 0)) {
                if (recurse) {
                    _directory_recurse(file_path, fn, data, true);
                }
            }
        }
        else if (S_ISREG(info.st_mode)) {
            fn(file_path, data);
        }
    }

    closedir(dir_ptr);
}

void mdir_init(mdir_t *dir, const char *dir_name, bool recurse) {
    vec_mfile_t files;
    vec_init(&files);

    _directory_recurse(dir_name, _directory_add_file, &files, recurse);
    dir->files = malloc(sizeof(mfile_t) * files.length);
    dir->num_files = files.length;
    memcpy(dir->files, files.data, sizeof(mfile_t) * files.length);
    vec_deinit(&files);
}

void mdir_deinit(mdir_t *dir) {
    free(dir->files);
}
