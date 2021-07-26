#include "mdata.h"

#include "hotloader.h"
#include "mstring.h"

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

static bool _mdata_file_handler(mfile_t file, bool first_time, void *udata) {
    if (strcmp(file.ext, ".mdata") == 0) {
        return true;
    }

    mfiletime_t filetime;
    if (mfile_get_time(&file, &filetime)) {
        mdata_file_t mdata_file;
        vec_init(&mdata_file.vals);

        mdata_file_add_val_uint64(&mdata_file, "timestamp", filetime.unix_time);

        //mfile_t data_file = mfile_append_extension(file.path, ".mdata");
        //mfile_set_data(&data_file, str.cstr, str.len);

        //mstring_deinit(&str);
    }

    if (strcmp(file.ext, ".png") == 0) {
      
    }

    return true;
}

void mdata_init(void) {
    hotloader_watch_files("data/", NULL, _mdata_file_handler, true);
}

void mdata_add_extension(const char *ext, bool (*mdata_file_creator)(mfile_t file, mdata_file_t *mdata_file)) {
}

void mdata_file_add_val_int(mdata_file_t *mdata_file, const char *field, int int_val) {
    _mdata_file_val_t val;
    val.type = _MDATA_FILE_VAL_TYPE_INT;
    val.int_val = int_val;
    vec_push(&mdata_file->vals, val);
}

void mdata_file_add_val_uint64(mdata_file_t *mdata_file, const char *field, uint64_t uint64_val) {
    _mdata_file_val_t val;
    val.type = _MDATA_FILE_VAL_TYPE_UINT64;
    val.uint64_val = uint64_val;
    vec_push(&mdata_file->vals, val);
}

void mdata_file_add_val_binary_data(mdata_file_t *mdata_file, const char *field, char *data, int data_len) {
    _mdata_file_val_t val;
    val.type = _MDATA_FILE_VAL_TYPE_BIN_DATA;
    val.data_val.data = malloc(data_len);
    memcpy(val.data_val.data, data, data_len);
    val.data_val.len = data_len;
    vec_push(&mdata_file->vals, val);
}

//void mdata_add_handler(const char *extension, bool (*handler)(mfile_t file, bool first_time, void *udata)) {

//}
