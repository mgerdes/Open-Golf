#define _CRT_SECURE_NO_WARNINGS

#include "file.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct file file_init(const char *path) {
    struct file file;
    strncpy(file.path, path, FILES_MAX_PATH);
    file.path[FILES_MAX_PATH - 1] = 0;

    int i = (int) strlen(path);
    while (i >= 0 && path[i--] != '.');
    if (i < 0) {
        i = (int) strlen(path);
        file.ext[0] = 0;
    }
    else {
        strncpy(file.ext, path + i + 1, FILES_MAX_EXT);
        file.ext[FILES_MAX_EXT - 1] = 0;
    }

    while (i >= 0 && path[i--] != '/');
    if (i < 0) {
        strncpy(file.name, path + i + 1, FILES_MAX_FILENAME);
        file.name[FILES_MAX_FILENAME - 1] = 0;
    }
    else {
        strncpy(file.name, path + i + 2, FILES_MAX_FILENAME);
        file.name[FILES_MAX_FILENAME - 1] = 0;
    }

    file.data = NULL;
    file.data_len = 0;

    return file;
}

bool file_load_data(struct file *file) {
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

    file->data = bytes;
    file->data_len = num_bytes;
    file->data_copy_pos = bytes;
    file->data_copy_len = num_bytes;
    return true;
}

void file_delete_data(struct file *file) {
    free(file->data);
    file->data = NULL;
}

bool file_set_data(struct file *file, char *data, int data_len) {
    if (file->data) {
        file_delete_data(file);
    }

    FILE *f = fopen(file->path, "wb");
    if (f) {
        fwrite(data, sizeof(char), data_len, f);
        fclose(f);
        return true;
    }
    else {
        return false;
    }
}

void file_copy_data(struct file *file, void *buffer, int num_bytes) {
    assert(file->data_copy_len >= num_bytes);
    memcpy(buffer, file->data_copy_pos, num_bytes);
    file->data_copy_len -= num_bytes;
    file->data_copy_pos += num_bytes;
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

bool file_copy_line(struct file *file, char **line_buffer, int *line_buffer_len) {
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

static void create_file_path(char *file_path, const char *dir_name, const char *file_name) {
    int dir_name_len = (int)strlen(dir_name);
    int file_name_len = (int)strlen(file_name);

    for (int i = 0; i < dir_name_len; i++) {
        if (i < FILES_MAX_PATH) {
            file_path[i] = dir_name[i];
        }
        else {
            file_path[FILES_MAX_PATH - 1] = 0;
            break;
        }
    }
    if (dir_name_len + 1 + file_name_len < FILES_MAX_PATH) {
        file_path[dir_name_len] = '/';
        for (int j = 0; j < file_name_len; j++) {
            file_path[dir_name_len + 1 + j] = file_name[j];
        }
        file_path[dir_name_len + 1 + file_name_len] = 0;
    }
    else {
        file_path[FILES_MAX_PATH - 1] = 0;
    }
}

#if defined(_WIN32) 

void file_get_time(struct file *file, struct file_time *time) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExA(file->path, GetFileExInfoStandard, &data)) {
        time->time = data.ftLastWriteTime;
    }
    else {
        memset(&time->time, 0, sizeof(time->time));
    }
}

int file_time_cmp(struct file_time *time0, struct file_time *time1) {
    return CompareFileTime(&time0->time, &time1->time);
}

void directory_init(struct directory *dir, const char *dir_name) {
    WIN32_FIND_DATA find_data;
    HANDLE handle = INVALID_HANDLE_VALUE;

    // Have to append \* to dir_name
    char dname[FILES_MAX_PATH];
    int dir_name_len = (int)strlen(dir_name);
    for (int i = 0; i < dir_name_len; i++) {
        if (i >= FILES_MAX_PATH) {
            dname[FILES_MAX_PATH - 1] = 0;
            break;
        }
        dname[i] = dir_name[i];
    }
    if (dir_name_len + 3 >= FILES_MAX_PATH) {
        dname[FILES_MAX_PATH - 1] = 0;
    }
    else {
        dname[dir_name_len] = '\\';
        dname[dir_name_len + 1] = '*';
        dname[dir_name_len + 2] = 0;
    }

    dir->num_files = 0;
    dir->files = NULL;

    // Loop through files first to count the num of files
    handle = FindFirstFile(dname, &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        assert(false);
        return;
    }
    while (true) {
        if (!!(find_data.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) ||
                !(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            dir->num_files++;
        }
        if (FindNextFile(handle, &find_data) == 0) {
            break;
        }
    }

    dir->files = malloc(sizeof(struct file)*dir->num_files);
    int file_idx = 0;

    // Loop through files a second time to actually get the data
    handle = FindFirstFile(dname, &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        assert(false);
        return;
    }
    while (true) {
        if (!!(find_data.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) ||
                !(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char full_path[FILES_MAX_PATH];
            char *fname = find_data.cFileName;
            int fname_len = (int)strlen(fname);

            for (int i = 0; i < dir_name_len; i++) {
                if (i >= FILES_MAX_PATH) {
                    full_path[FILES_MAX_PATH - 1] = 0;
                    break;
                }
                full_path[i] = dir_name[i];
            }
            if (dir_name_len + 1 + fname_len + 1 >= FILES_MAX_PATH) {
                full_path[FILES_MAX_PATH - 1] = 0;
            }
            else {
                full_path[dir_name_len] = '/';
                for (int j = 0; j < fname_len; j++) {
                    full_path[dir_name_len + 1 + j] = fname[j];
                }
                full_path[dir_name_len + 1 + fname_len] = 0;
            }

            if (file_idx < dir->num_files) {
                dir->files[file_idx++] = file_init(full_path);
            }
        }
        if (FindNextFile(handle, &find_data) == 0) {
            break;
        }
    }
}

void directory_deinit(struct directory *dir) {
    free(dir->files);
}

#else

void file_get_time(struct file *file, struct file_time *time) {
    struct stat info;
    if (stat(file->path, &info)) {
        return;
    }
    time->time = info.st_mtime;
}

int file_time_cmp(struct file_time *time0, struct file_time *time1) {
    return (int)difftime(time0->time, time1->time);
}

void directory_init(struct directory *dir, const char *dir_name) {
    dir->num_files = 0;
    dir->files = NULL;

    struct dirent *entry = NULL;
    DIR *dir_ptr = opendir(dir_name);
    if (dir_ptr != NULL) {
        while ((entry = readdir(dir_ptr))) {
            char file_path[FILES_MAX_PATH];
            create_file_path(file_path, dir_name, entry->d_name);

            struct stat info;
            stat(file_path, &info); 
            if (S_ISREG(info.st_mode)) {
                dir->num_files++;
            }
        }
    }

    dir->files = malloc(sizeof(struct file)*dir->num_files);
    int file_idx = 0;

    dir_ptr = opendir(dir_name);
    if (dir_ptr != NULL) {
        while ((entry = readdir(dir_ptr))) {
            char file_path[FILES_MAX_PATH];
            create_file_path(file_path, dir_name, entry->d_name);

            struct stat info;
            stat(file_path, &info); 
            if (S_ISREG(info.st_mode)) {
                dir->files[file_idx++] = file_init(file_path);
            }
        }
    }
    closedir(dir_ptr);
}

void directory_deinit(struct directory *dir) {
}

#endif

static int compare_files(const void *a, const void *b) {
    struct file *file_a = (struct file *)a;
    struct file *file_b = (struct file *)b;
    return strcmp(file_a->name, file_b->name);
}

void directory_sort_files_alphabetically(struct directory *dir) {
    qsort(dir->files, dir->num_files, sizeof(struct file), compare_files);
}
