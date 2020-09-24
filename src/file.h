#ifndef _FILE_H
#define _FILE_H

#include <stdbool.h>

#define FILES_MAX_PATH 1024
#define FILES_MAX_FILENAME 256
#define FILES_MAX_EXT 32

#if defined(_WIN32)

#include <Windows.h>
struct file_time {
    FILETIME time;
};

#else

#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
struct file_time {
    time_t time;
};

#endif

struct file {
    char path[FILES_MAX_PATH];
    char name[FILES_MAX_FILENAME];
    char ext[FILES_MAX_EXT];

    int data_len;
    char *data;

    int data_copy_len;
    char *data_copy_pos;
};

struct directory {
    int num_files;
    struct file *files;
};

struct file file_init(const char *path);
bool file_load_data(struct file *file);
void file_delete_data(struct file *file);
bool file_set_data(struct file *file, char *data, int data_len);
void file_copy_data(struct file *file, void *buffer, int num_bytes);
bool file_copy_line(struct file *file, char **line_buffer, int *line_buffer_len);
void file_get_time(struct file *file, struct file_time *time);
int file_time_cmp(struct file_time *time0, struct file_time *time1);

void directory_init(struct directory* dir, const char* dir_name);
void directory_deinit(struct directory* dir);

void directory_init(struct directory *dir, const char *dir_name); 
void directory_deinit(struct directory *dir);
void directory_sort_files_alphabetically(struct directory *dir);

#endif
