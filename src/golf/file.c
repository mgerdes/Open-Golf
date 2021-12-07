#define _CRT_SECURE_NO_WARNINGS

#include "golf/file.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)

#include <Windows.h>
#include <Shlwapi.h>

#else

#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#endif

#if defined(_WIN32)

uint64_t _get_file_time(const char *path) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &data)) {
        // https://www.frenk.com/2009/12/convert-filetime-to-unix-timestamp/
        FILETIME ft = data.ftLastWriteTime;

        // takes the last modified date
        LARGE_INTEGER date, adjust;
        date.HighPart = ft.dwHighDateTime;
        date.LowPart = ft.dwLowDateTime;

        // 100-nanoseconds = milliseconds * 10000
        adjust.QuadPart = 11644473600000 * 10000;

        // removes the diff between 1970 and 1601
        date.QuadPart -= adjust.QuadPart;

        // converts back from 100-nanoseconds to seconds
        return (uint64_t) (date.QuadPart / 10000000);
    }
    else {
        return 0;
    }
}

#else

uint64_t _get_file_time(const char *path) {
    struct stat info;
    if (stat(path, &info)) {
        return 0;
    }
    return (uint64_t)info.st_mtime;
}

#endif

golf_file_t golf_file(const char *path) {
    golf_file_t file;
    strncpy(file.path, path, GOLF_FILE_MAX_PATH);
    file.path[GOLF_FILE_MAX_PATH - 1] = 0;

    int i = (int) strlen(path);
    while (i >= 0 && path[i--] != '.');
    if (i < 0) {
        i = (int) strlen(path);
        file.ext[0] = 0;
    }
    else {
        strncpy(file.ext, path + i + 1, GOLF_FILE_MAX_EXT);
        file.ext[GOLF_FILE_MAX_EXT - 1] = 0;
    }

    while (i >= 0 && path[i--] != '/');
    if (i < 0) {
        strncpy(file.name, path + i + 1, GOLF_FILE_MAX_NAME);
        file.name[GOLF_FILE_MAX_NAME - 1] = 0;
    }
    else {
        strncpy(file.name, path + i + 2, GOLF_FILE_MAX_NAME);
        file.name[GOLF_FILE_MAX_NAME - 1] = 0;
    }

    int path_len = (int) strlen(file.path);
    int name_len = (int) strlen(file.name);
    strncpy(file.dirname, file.path, GOLF_FILE_MAX_PATH);
    file.dirname[path_len - name_len] = 0;

    return file;
}

golf_file_t golf_file_new_ext(golf_file_t *file, const char *ext) {
    golf_file_t new_file = golf_file(file->path);

    int ext_len = (int) strlen(new_file.ext);

    int path_len = (int) strlen(new_file.path);
    strncpy(new_file.path + path_len - ext_len, ext, GOLF_FILE_MAX_PATH - path_len + ext_len);
    new_file.path[GOLF_FILE_MAX_PATH - 1] = 0;

    int name_len = (int) strlen(new_file.name);
    strncpy(new_file.name + name_len - ext_len, ext, GOLF_FILE_MAX_NAME - name_len + ext_len);
    new_file.name[GOLF_FILE_MAX_NAME - 1] = 0;

    strncpy(new_file.ext, ext, GOLF_FILE_MAX_EXT);
    new_file.ext[GOLF_FILE_MAX_EXT - 1] = 0;

    return new_file;
}

golf_file_t golf_file_append_extension(const char *path, const char *ext) {
    golf_file_t file = golf_file(path);

    //int ext_len = (int) strlen(file.ext);
    int path_len = (int) strlen(file.path);
    int name_len = (int) strlen(file.name);

    strncpy(file.path + path_len, ext, GOLF_FILE_MAX_PATH);
    file.path[GOLF_FILE_MAX_PATH - 1] = 0;

    strncpy(file.name + name_len, ext, GOLF_FILE_MAX_NAME);
    file.name[GOLF_FILE_MAX_NAME - 1] = 0;

    strncpy(file.ext, ext, GOLF_FILE_MAX_EXT);
    file.ext[GOLF_FILE_MAX_EXT - 1] = 0;

    return file;
}

uint64_t golf_file_get_time(const char *path) {
    return _get_file_time(path);
}

bool golf_file_load_data(const char *path, char **data, int *data_len) {
    FILE *f = fopen(path, "rb");
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

    *data = bytes;
    *data_len = num_bytes;

    return true;
}

static void _directory_add_file(const char *file_path, void *data) {
    golf_file_t file = golf_file(file_path);
    vec_golf_file_t *vec = (vec_golf_file_t*) data;
    vec_push(vec, file);
}

#if defined(_WIN32)

static void _directory_recurse(const char *dir_name, void (*fn)(const char *file_path, void*), void *data, bool recurse) {
    WIN32_FIND_DATA find_data;
    HANDLE handle = INVALID_HANDLE_VALUE;

    // Have to append \* to dir_name
    char dname[GOLF_FILE_MAX_PATH];
    int dir_name_len = (int)strlen(dir_name);
    for (int i = 0; i < dir_name_len; i++) {
        if (i >= GOLF_FILE_MAX_PATH) {
            dname[GOLF_FILE_MAX_PATH - 1] = 0;
            break;
        }
        dname[i] = dir_name[i];
    }
    if (dir_name_len + 3 >= GOLF_FILE_MAX_PATH) {
        dname[GOLF_FILE_MAX_PATH - 1] = 0;
    }
    else {
        dname[dir_name_len] = '\\';
        dname[dir_name_len + 1] = '*';
        dname[dir_name_len + 2] = 0;
    }

    handle = FindFirstFile(dname, &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        //assert(false);
        return;
    }
    while (true) {
        char full_path[GOLF_FILE_MAX_PATH];
        char *fname = find_data.cFileName;
        int fname_len = (int)strlen(fname);

        for (int i = 0; i < dir_name_len; i++) {
            if (i >= GOLF_FILE_MAX_PATH) {
                full_path[GOLF_FILE_MAX_PATH - 1] = 0;
                break;
            }
            full_path[i] = dir_name[i];
        }
        if (dir_name_len + 1 + fname_len + 1 >= GOLF_FILE_MAX_PATH) {
            full_path[GOLF_FILE_MAX_PATH - 1] = 0;
        }
        else {
            full_path[dir_name_len] = '/';
            for (int j = 0; j < fname_len; j++) {
                full_path[dir_name_len + 1 + j] = fname[j];
            }
            full_path[dir_name_len + 1 + fname_len] = 0;
        }

        if (recurse && (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if ((strcmp(fname, ".") != 0) && (strcmp(fname, "..") != 0)) {
                _directory_recurse(full_path, fn, data, recurse);
            }
        }
        else if (!!(find_data.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) ||
                !(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            _directory_add_file(full_path, data);
        }

        if (FindNextFile(handle, &find_data) == 0) {
            break;
        }
    }
}

#else

static void _create_file_path(char *file_path, const char *dir_name, const char *file_name) {
    int dir_name_len = (int)strlen(dir_name);
    int file_name_len = (int)strlen(file_name);

    for (int i = 0; i < dir_name_len; i++) {
        if (i < GOLF_FILE_MAX_PATH) {
            file_path[i] = dir_name[i];
        }
        else {
            file_path[GOLF_FILE_MAX_PATH - 1] = 0;
            break;
        }
    }
    if (dir_name_len + 1 + file_name_len < GOLF_FILE_MAX_PATH) {
        file_path[dir_name_len] = '/';
        for (int j = 0; j < file_name_len; j++) {
            file_path[dir_name_len + 1 + j] = file_name[j];
        }
        file_path[dir_name_len + 1 + file_name_len] = 0;
    }
    else {
        file_path[GOLF_FILE_MAX_PATH - 1] = 0;
    }
}

static void _directory_recurse(const char *dir_name, void (*fn)(const char *file_path, void*), void *data, bool recurse) {
    DIR *dir_ptr = opendir(dir_name);
    if (dir_ptr == NULL) {
        return;
    }

    struct dirent *entry = NULL;
    while ((entry = readdir(dir_ptr))) {
        char file_path[GOLF_FILE_MAX_PATH];
        _create_file_path(file_path, dir_name, entry->d_name);

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

#endif

void golf_dir_init(golf_dir_t *dir, const char *dir_name, bool recurse) {
    vec_golf_file_t files;
    vec_init(&files);

    _directory_recurse(dir_name, _directory_add_file, &files, recurse);
    dir->files = malloc(sizeof(golf_file_t) * files.length);
    dir->num_files = files.length;
    memcpy(dir->files, files.data, sizeof(golf_file_t) * files.length);
    vec_deinit(&files);
}

void golf_dir_deinit(golf_dir_t *dir) {
    free(dir->files);
}
