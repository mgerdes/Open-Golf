#include "hotloader.h"

#include <assert.h>

#include "array.h"
#include "file.h"

struct hotloader_file {
    struct file_time load_time; 
    struct file file;
    void *udata;
    bool (*callback)(struct file file, bool first_time, void *udata);
};
array_t(struct hotloader_file, array_hotloader_file);

struct hotloader_state {
    struct array_hotloader_file files;
};

static struct hotloader_state _state;

void hotloader_init(void) {
    array_init(&_state.files);
}

void hotloader_update(void) {
#if HOTLOADER_ACTIVE
    for (int i = 0; i < _state.files.length; i++) {
        struct hotloader_file *hl_file = &_state.files.data[i];
        struct file_time cur_time;
        file_get_time(&hl_file->file, &cur_time);
        if (file_time_cmp(&hl_file->load_time, &cur_time) < 0) {
            if (hl_file->callback(hl_file->file, false, hl_file->udata)) {
                hl_file->load_time = cur_time;
            }
        }
    }
#endif
}

void hotloader_watch_file(const char *path, void *udata,
        bool (*callback)(struct file file, bool first_time, void *udata)) {
    struct file file = file_init(path);

    bool ret = callback(file, true,udata);
    assert(ret);
    struct hotloader_file hotloader_file;
    hotloader_file.file = file;
    file_get_time(&hotloader_file.file, &hotloader_file.load_time);
    hotloader_file.callback = callback;
    hotloader_file.udata = udata;
    array_push(&_state.files, hotloader_file);
}

void hotloader_watch_files(const char *path, const char *ext, void *udata,
        bool (*callback)(struct file file, bool first_time, void *udata)) {
    struct directory dir;
    directory_init(&dir, path);
    for (int i = 0; i < dir.num_files; i++) {
        if (strcmp(ext, dir.files[i].ext) != 0) {
            continue;
        }

        hotloader_watch_file(dir.files[i].path, udata, callback);
    }
    directory_deinit(&dir);
}
