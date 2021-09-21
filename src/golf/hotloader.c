#include "hotloader.h"

#include <assert.h>

#include "3rd_party/vec/vec.h"
#include "mcore/mlog.h"
#include "mcore/mfile.h"
#include "golf/log.h"

struct hotloader_file {
    mfiletime_t load_time; 
    mfile_t file;
    void *udata;
    bool (*callback)(mfile_t file, bool first_time, void *udata);
};
typedef vec_t(struct hotloader_file) vec_hotloader_file_t;

struct hotloader_state {
    vec_hotloader_file_t files;
};

static struct hotloader_state _state;

void hotloader_init(void) {
    vec_init(&_state.files);
}

void hotloader_update(void) {
#if HOTLOADER_ACTIVE
    for (int i = 0; i < _state.files.length; i++) {
        struct hotloader_file *hl_file = &_state.files.data[i];
        mfiletime_t cur_time;
        mfile_get_time(&hl_file->file, &cur_time);
        if (mfiletime_cmp(hl_file->load_time, cur_time) < 0) {
            if (hl_file->callback(hl_file->file, false, hl_file->udata)) {
                hl_file->load_time = cur_time;
            }
        }
    }
#endif
}

void hotloader_watch_file(const char *path, void *udata,
        bool (*callback)(mfile_t file, bool first_time, void *udata)) {
    mfile_t file = mfile(path);

    bool ret = callback(file, true, udata);
    if (!ret) {
        mlog_warning("Could not load file: %s", path);
    }
    assert(ret);
    struct hotloader_file hotloader_file;
    hotloader_file.file = file;
    mfile_get_time(&hotloader_file.file, &hotloader_file.load_time);
    hotloader_file.callback = callback;
    hotloader_file.udata = udata;
    vec_push(&_state.files, hotloader_file);
}

void hotloader_watch_files(const char *path, void *udata,
        bool (*callback)(mfile_t file, bool first_time, void *udata), bool recurse) {
    mdir_t dir;
    mdir_init(&dir, path, recurse);
    for (int i = 0; i < dir.num_files; i++) {
        hotloader_watch_file(dir.files[i].path, udata, callback);
    }
    mdir_deinit(&dir);
}

void hotloader_watch_files_with_ext(const char *path, const char *ext, void *udata,
        bool (*callback)(mfile_t file, bool first_time, void *udata), bool recurse) {
    mdir_t dir;
    mdir_init(&dir, path, recurse);
    for (int i = 0; i < dir.num_files; i++) {
        if (strcmp(ext, dir.files[i].ext) != 0) {
            continue;
        }


        hotloader_watch_file(dir.files[i].path, udata, callback);
    }
    mdir_deinit(&dir);
}
