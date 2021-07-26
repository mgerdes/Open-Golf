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

typedef struct _mdata_extension_handler {
    bool (*mdata_file_creator)(mfile_t file, mdata_file_t *mdata_file);
} _mdata_extension_handler_t;

static bool _callback(mfile_t file, bool first_time, void *udata) {
    if (strcmp(file.ext, ".mdata") == 0) {
        return true;
    }

    _mdata_extension_handler_t *extension_handler = (_mdata_extension_handler_t*) udata;
    mfiletime_t filetime;
    if (mfile_get_time(&file, &filetime)) {
        mdata_file_t mdata_file;
        vec_init(&mdata_file.vals);

        mdata_file_add_val_uint64(&mdata_file, "timestamp", filetime.unix_time);
        extension_handler->mdata_file_creator(file, &mdata_file);

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

        mfile_t data_file = mfile_append_extension(file.path, ".mdata");
        mfile_set_data(&data_file, str.cstr, str.len);
        mstring_deinit(&str);
    }

    if (strcmp(file.ext, ".png") == 0) {
      
    }

    return true;
}

void mdata_init(void) {
    //hotloader_watch_files("data/", NULL, _mdata_file_handler, true);
}

void mdata_add_extension_handler(const char *ext, bool (*mdata_file_creator)(mfile_t file, mdata_file_t *mdata_file)) {
    _mdata_extension_handler_t *extension_handler = malloc(sizeof(_mdata_extension_handler_t));
    extension_handler->mdata_file_creator = mdata_file_creator;
    hotloader_watch_files_with_ext("data", ext, extension_handler, _callback, true);
}

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

void mdata_file_add_val_binary_data(mdata_file_t *mdata_file, const char *field, char *data, int data_len) {
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
