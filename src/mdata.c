#define _CRT_SECURE_NO_WARNINGS

#include "mdata.h"

#include "data_stream.h"
#include "hotloader.h"
#include "mstring.h"
#include "log.h"

#define _MDATA_VAL_MAX_NAME 32

enum _mdata_file_val_type {
    _MDATA_FILE_VAL_TYPE_INT,
    _MDATA_FILE_VAL_TYPE_UINT64,
    _MDATA_FILE_VAL_TYPE_BIN_DATA,
};

typedef struct _mdata_file_val {
    enum _mdata_file_val_type type;
    union {
        int int_val;
        uint64_t uint64_val;
        struct {
            char *data;
            int len;
        } data_val;
    };
    char name[_MDATA_VAL_MAX_NAME + 1];
} _mdata_file_val_t;

typedef vec_t(_mdata_file_val_t) _vec_mdata_file_val_t;

struct mdata_file {
    _vec_mdata_file_val_t vals; 
};

typedef vec_t(mdata_file_t*) _vec_mdata_file_ptr_t;

typedef struct _mdata_extension_handler {
    vec_mfile_t files;
    vec_mfiletime_t file_load_times;
    _vec_mdata_file_ptr_t mdata_files;

    void *udata;
    bool (*mdata_file_creator)(int num_files, mfile_t *files, bool *file_changed, mdata_file_t **mdata_files, void *udata);
} _mdata_extension_handler_t;

typedef vec_t(_mdata_extension_handler_t) _vec_mdata_extension_handler_t;
static _vec_mdata_extension_handler_t _extension_handlers;

/*
static bool _callback(mfile_t file, bool first_time, void *udata) {
    if (strcmp(file.ext, ".mdata") == 0) {
        return true;
    }

    _mdata_extension_handler_t *extension_handler = (_mdata_extension_handler_t*) udata;
    mfiletime_t filetime;
    mfile_get_time(&file, &filetime); 

    mfile_t data_file = mfile_append_extension(file.path, ".mdata");
    mfiletime_t data_filetime;
    mfile_get_time(&data_file, &data_filetime);

    if (data_filetime.unix_time < filetime.unix_time) {
        mdata_file_t mdata_file;
        vec_init(&mdata_file.vals);
        extension_handler->mdata_file_creator(file, &mdata_file, extension_handler->udata);

        mstring_t str; 
        mstring_init(&str, "");
        for (int i = 0; i < mdata_file.vals.length; i++) {
            _mdata_file_val_t file_val = mdata_file.vals.data[i];
            switch (file_val.type) {
                case _MDATA_FILE_VAL_TYPE_INT:
                    mstring_appendf(&str, "int %s %d\n", file_val.name, file_val.int_val);
                    break;
                case _MDATA_FILE_VAL_TYPE_UINT64:
                    mstring_appendf(&str, "uint64 %s %u\n", file_val.name, file_val.uint64_val);
                    break;
                case _MDATA_FILE_VAL_TYPE_BIN_DATA:
                    mstring_appendf(&str, "bin_data %s %d ", file_val.name, file_val.data_val.len);
                    mstring_append_cstr_len(&str, file_val.data_val.data, file_val.data_val.len);
                    mstring_appendf(&str, "\n");
                    break;
            }
        }
        mfile_set_data(&data_file, str.cstr, str.len);
        mstring_deinit(&str);
        vec_deinit(&mdata_file.vals);
    }

    return true;
}
*/

void mdata_init(void) {
    vec_init(&_extension_handlers);
}

void mdata_add_extension_handler(const char *dir_name, const char *ext, bool (*mdata_file_creator)(int num_files, mfile_t *files, bool *file_changed,  mdata_file_t **mdata_files, void *udata), void *udata) {
    _mdata_extension_handler_t handler;
    vec_init(&handler.files);
    vec_init(&handler.file_load_times);
    vec_init(&handler.mdata_files);
    handler.udata = udata;
    handler.mdata_file_creator = mdata_file_creator;

    mdir_t dir;
    mdir_init(&dir, dir_name, true);
    for (int i = 0; i < dir.num_files; i++) {
        mfile_t file = dir.files[i];
        if (strcmp(file.ext, ext) != 0) {
            continue;
        }

        mdata_file_t *mdata_file = malloc(sizeof(mdata_file_t));
        vec_init(&mdata_file->vals);

        mfiletime_t file_load_time;
        mfile_get_time(&file, &file_load_time);

        vec_push(&handler.files, file);
        vec_push(&handler.file_load_times, file_load_time);
        vec_push(&handler.mdata_files, mdata_file);
    }

    mdata_file_creator(handler.files.length, handler.files.data, NULL, handler.mdata_files.data, udata);
    for (int i = 0; i < handler.files.length; i++) {
        mfile_t data_file = mfile_append_extension(handler.files.data[i].path, ".mdata");
        mdata_file_t *mdata_file = handler.mdata_files.data[i];

        mstring_t str; 
        mstring_init(&str, "");
        for (int i = 0; i < mdata_file->vals.length; i++) {
            _mdata_file_val_t file_val = mdata_file->vals.data[i];
            switch (file_val.type) {
                case _MDATA_FILE_VAL_TYPE_INT:
                    mstring_appendf(&str, "int %s %d\n", file_val.name, file_val.int_val);
                    break;
                case _MDATA_FILE_VAL_TYPE_UINT64:
                    mstring_appendf(&str, "uint64 %s %u\n", file_val.name, file_val.uint64_val);
                    break;
                case _MDATA_FILE_VAL_TYPE_BIN_DATA:
                    mstring_appendf(&str, "bin_data %s %d ", file_val.name, file_val.data_val.len);
                    mstring_append_cstr_len(&str, file_val.data_val.data, file_val.data_val.len);
                    mstring_appendf(&str, "\n");
                    break;
            }
        }

        mfile_set_data(&data_file, str.cstr, str.len);
        mstring_deinit(&str);
    }

    vec_push(&_extension_handlers, handler);
}

/*
void mdata_add_extension_handler(const char *ext, bool (*mdata_file_creator)(mfile_t file, mdata_file_t *mdata_file, void *udata), void *udata) {
    _mdata_extension_handler_t *extension_handler = malloc(sizeof(_mdata_extension_handler_t));
    extension_handler->udata = udata;
    extension_handler->mdata_file_creator = mdata_file_creator;
    hotloader_watch_files_with_ext("data", ext, extension_handler, _callback, true);
}
*/

void mdata_file_add_val_int(mdata_file_t *mdata_file, const char *field, int int_val) {
    _mdata_file_val_t val;
    val.type = _MDATA_FILE_VAL_TYPE_INT;
    val.int_val = int_val;
    strncpy(val.name, field, _MDATA_VAL_MAX_NAME);
    val.name[_MDATA_VAL_MAX_NAME] = 0;
    vec_push(&mdata_file->vals, val);
}

void mdata_file_add_val_uint64(mdata_file_t *mdata_file, const char *field, uint64_t uint64_val) {
    _mdata_file_val_t val;
    val.type = _MDATA_FILE_VAL_TYPE_UINT64;
    val.uint64_val = uint64_val;
    strncpy(val.name, field, _MDATA_VAL_MAX_NAME);
    val.name[_MDATA_VAL_MAX_NAME] = 0;
    vec_push(&mdata_file->vals, val);
}

void mdata_file_add_val_binary_data(mdata_file_t *mdata_file, const char *field, char *data, int data_len, bool compress) {
    if (compress) {
        struct data_stream stream;
        data_stream_init(&stream);
        data_stream_push(&stream, data, data_len);
        data_stream_compress(&stream);

        _mdata_file_val_t val;
        val.type = _MDATA_FILE_VAL_TYPE_BIN_DATA;
        val.data_val.data = stream.data;
        val.data_val.len = stream.len;
        strncpy(val.name, field, _MDATA_VAL_MAX_NAME);
        val.name[_MDATA_VAL_MAX_NAME] = 0;
        vec_push(&mdata_file->vals, val);
    }
    else {
        _mdata_file_val_t val;
        val.type = _MDATA_FILE_VAL_TYPE_BIN_DATA;
        val.data_val.data = malloc(data_len);
        memcpy(val.data_val.data, data, data_len);
        val.data_val.len = data_len;
        strncpy(val.name, field, _MDATA_VAL_MAX_NAME);
        val.name[_MDATA_VAL_MAX_NAME] = 0;
        vec_push(&mdata_file->vals, val);
    }
}
