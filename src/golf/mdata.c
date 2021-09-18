#define _CRT_SECURE_NO_WARNINGS

#include <assert.h>

#include "3rd_party/map/map.h"
#include "3rd_party/vec/vec.h"
#include "golf/mdata.h"
#include "golf/data_stream.h"
#include "golf/hotloader.h"
#include "golf/mcommon.h"
#include "golf/mstring.h"
#include "golf/log.h"

#define _VAL_MAX_NAME_LEN 32

enum _val_type {
    _VAL_TYPE_INT,
    _VAL_TYPE_UINT64,
    _VAL_TYPE_STRING,
    _VAL_TYPE_DATA,
};

typedef struct _val {
    enum _val_type type;
    union {
        int int_val;
        uint64_t uint64_val;
        struct {
            char *data_val;
            int data_val_len;
        };
        char *string_val;
    };
    char name[_VAL_MAX_NAME_LEN];
    bool user_set;
} _val_t;

typedef vec_t(_val_t) _vec_val_t;
typedef map_t(_val_t) _map_val_t;

struct mdata_file {
    _map_val_t vals_map;
    _vec_val_t vals_vec; 
};

typedef vec_t(mdata_file_t*) _vec_mdata_file_ptr_t;

typedef struct _ext_handler {
    vec_mfile_t files;
    vec_mfiletime_t file_load_times;
    _vec_mdata_file_ptr_t mdata_files;

    void *udata;
    bool (*mdata_file_creator)(mfile_t file, mdata_file_t *mdata_file, void *udata);
    bool (*mdata_file_handler)(const char *file_path, mdata_file_t *mdata_file, void *udata);
} _ext_handler_t;

typedef vec_t(_ext_handler_t) _vec_mdata_extension_handler_t;
static _vec_mdata_extension_handler_t _extension_handlers;

static _val_t _create_int_val(const char *name, int int_val) {
    _val_t val;
    val.type = _VAL_TYPE_INT;
    mstrncpy(val.name, name, _VAL_MAX_NAME_LEN);
    val.int_val = int_val;
    return val;
}

static _val_t _create_uint64_val(const char *name, uint64_t uint64_val) {
    _val_t val;
    val.type = _VAL_TYPE_UINT64;
    mstrncpy(val.name, name, _VAL_MAX_NAME_LEN);
    val.uint64_val = uint64_val;
    return val;
}

static _val_t _create_string_val(const char *name, const char *string_val) {
    _val_t val;
    val.type = _VAL_TYPE_STRING;
    mstrncpy(val.name, name, _VAL_MAX_NAME_LEN);
    int n = strlen(string_val) + 1;
    val.string_val = malloc(sizeof(char) * n);
    mstrncpy(val.string_val, string_val, n);
    return val;
}

static _val_t _create_data_val(const char *name, const char *data, int len) {
    _val_t val;
    val.type = _VAL_TYPE_DATA;
    mstrncpy(val.name, name, _VAL_MAX_NAME_LEN);
    val.data_val = malloc(len);
    val.data_val_len = len;
    memcpy(val.data_val, data, len);
    return val;
}

void mdata_init(void) {
    vec_init(&_extension_handlers);
}

mdata_file_t *_load_mdata_file(mfile_t file) {
    if (!mfile_load_data(&file)) {
        return NULL;
    }

    mdata_file_t *mdata_file = malloc(sizeof(mdata_file_t));
    map_init(&mdata_file->vals_map);
    vec_init(&mdata_file->vals_vec);
    int i = 0;
    while (i < file.data_len) {
        int type_len = 0;
        char type[32]; 

        int name_len = 0;
        char name[_VAL_MAX_NAME_LEN + 1];

        while (file.data[i] != ' ') {
            if (i >= file.data_len) assert(false);
            type[type_len++] = file.data[i++];
            if (type_len >= 32) {
                assert(false);
            }
        }
        type[type_len] = 0;

        if (i >= file.data_len) assert(false);
        i++;

        while (file.data[i] != ' ') {
            if (i >= file.data_len) assert(false);
            name[name_len++] = file.data[i++];
            if (name_len >= _VAL_MAX_NAME_LEN + 1) {
                assert(false);
            }
        }
        name[name_len] = 0;

        if (i >= file.data_len) assert(false);
        i++;

        _val_t val;
        memset(&val, 0, sizeof(val));
        if (strcmp(type, "int") == 0) {
            int int_val = 0;
            while (file.data[i] != '\n') {
                if (file.data[i] < '0' || file.data[i] > '9') {
                    assert(false);
                }

                int_val *= 10;
                int_val += file.data[i++] - '0';
            }
            i++;

            val = _create_int_val(name, int_val);
        }
        else if (strcmp(type, "uint64") == 0) {
            while (file.data[i++] != '\n');
            assert(false);
        }
        else if (strcmp(type, "string") == 0) {
            mstring_t string;
            mstring_init(&string, "");
            while (file.data[i] != '\n') {
                mstring_append_char(&string, file.data[i++]);
            }
            i++;

            val = _create_string_val(name, string.cstr);
            mstring_deinit(&string);
        }
        else if (strcmp(type, "data") == 0) {
            mstring_t data;
            mstring_init(&data, "");
            while (file.data[i] != '\n') {
                mstring_append_char(&data, file.data[i++]);
            }
            i++;

            val = _create_data_val(name, data.cstr, data.len);
            mstring_deinit(&data);
        }
        else {
            assert(false);
        }

        map_set(&mdata_file->vals_map, val.name, val);
    }
    assert(i == file.data_len);

    mfile_free_data(&file);
    return mdata_file;
}

static void _delete_mdata_file(mdata_file_t *mdata_file) {
    free(mdata_file);
}

void mdata_add_extension_handler(const char *ext, bool (*mdata_file_creator)(mfile_t files, mdata_file_t *mdata_files, void *udata), bool (*mdata_file_handler)(const char *file_path, mdata_file_t *mdata_file, void *udata), void *udata) {
    _ext_handler_t handler;
    vec_init(&handler.files);
    vec_init(&handler.file_load_times);
    vec_init(&handler.mdata_files);
    handler.udata = udata;
    handler.mdata_file_creator = mdata_file_creator;

    mdir_t dir;
    mdir_init(&dir, "data", true);
    for (int i = 0; i < dir.num_files; i++) {
        mfile_t file = dir.files[i];
        if (strcmp(file.ext, ext) != 0) {
            continue;
        }

        mfile_t data_file = mfile_append_extension(file.path, ".mdata");
        mdata_file_t *mdata_file = _load_mdata_file(data_file);

        mfiletime_t file_load_time;
        mfile_get_time(&file, &file_load_time);

        vec_push(&handler.files, file);
        vec_push(&handler.file_load_times, file_load_time);
        vec_push(&handler.mdata_files, mdata_file);

        m_logf("mdata: Importing %s\n", file.path);
        mdata_file_creator(file, mdata_file, udata);
        mdata_file_handler(file.path, mdata_file, udata);
    }

    for (int i = 0; i < handler.files.length; i++) {
        mdata_file_t *mdata_file = handler.mdata_files.data[i];

        mstring_t str; 
        mstring_init(&str, "");
        for (int j = 0; j < mdata_file->vals_vec.length; j++) {
            _val_t file_val = mdata_file->vals_vec.data[j];
            switch (file_val.type) {
                case _VAL_TYPE_INT:
                    {
                        mstring_appendf(&str, "int %s %d\n", file_val.name, file_val.int_val);
                    }
                    break;
                case _VAL_TYPE_UINT64:
                    {
                        mstring_appendf(&str, "uint64 %s %u\n", file_val.name, file_val.uint64_val);
                    }
                    break;
                case _VAL_TYPE_STRING:
                    {
                        mstring_appendf(&str, "string %s %s\n", file_val.name, file_val.string_val);
                    }
                    break;
                case _VAL_TYPE_DATA:
                    {
                        char *encoding = mbase64_encode((const unsigned char*)file_val.data_val, file_val.data_val_len);
                        mstring_appendf(&str, "data %s %s\n", file_val.name, encoding);
                        free(encoding);
                    }
                    break;
            }
        }

        mfile_t data_file = mfile_append_extension(handler.files.data[i].path, ".mdata");
        mfile_set_data(&data_file, str.cstr, str.len);
        mstring_deinit(&str);
    }

    vec_push(&_extension_handlers, handler);
}

bool mdata_file_get_int(mdata_file_t *mdata_file, const char *field, int *val) {
    _val_t *mdata_val = map_get(&mdata_file->vals_map, field);
    if (mdata_val && mdata_val->type == _VAL_TYPE_INT) {
        *val = mdata_val->int_val;
        return true;
    }
    return false;
}

bool mdata_file_get_uint64(mdata_file_t *mdata_file, const char *field, uint64_t *val) {
    _val_t *mdata_val = map_get(&mdata_file->vals_map, field);
    if (mdata_val && mdata_val->type == _VAL_TYPE_UINT64) {
        *val = mdata_val->uint64_val;
        return true;
    }
    return false;
}

bool mdata_file_get_string(mdata_file_t *mdata_file, const char *field, const char **string) {
    _val_t *mdata_val = map_get(&mdata_file->vals_map, field);
    if (mdata_val && mdata_val->type == _VAL_TYPE_STRING) {
        *string = mdata_val->string_val;
        return true;
    }
    return false;
}

bool mdata_file_get_data(mdata_file_t *mdata_file, const char *field, char **data, int *data_len) {
    _val_t *mdata_val = map_get(&mdata_file->vals_map, field);
    if (mdata_val && mdata_val->type == _VAL_TYPE_DATA) {
        *data = mdata_val->data_val;
        *data_len = mdata_val->data_val_len;
        return true;
    }
    return false;
}

void mdata_file_add_int(mdata_file_t *mdata_file, const char *field, int int_val, bool user_set) {
    if (user_set) {
        _val_t *val = map_get(&mdata_file->vals_map, field);
        if (val && val->type == _VAL_TYPE_INT) {
            vec_push(&mdata_file->vals_vec, *val);
            return;
        }
    }

    _val_t val = _create_int_val(field, int_val);
    vec_push(&mdata_file->vals_vec, val);
    map_set(&mdata_file->vals_map, field, val);
}

void mdata_file_add_uint64(mdata_file_t *mdata_file, const char *field, uint64_t uint64_val, bool user_set) {
    if (user_set) {
        _val_t *val = map_get(&mdata_file->vals_map, field);
        if (val && val->type == _VAL_TYPE_UINT64) {
            vec_push(&mdata_file->vals_vec, *val);
            return;
        }
    }

    _val_t val = _create_uint64_val(field, uint64_val);
    vec_push(&mdata_file->vals_vec, val);
    map_set(&mdata_file->vals_map, field, val);
}

void mdata_file_add_string(mdata_file_t *mdata_file, const char *field, const char *string_val, bool user_set) {
    if (user_set) {
        _val_t *val = map_get(&mdata_file->vals_map, field);
        if (val && val->type == _VAL_TYPE_STRING) {
            vec_push(&mdata_file->vals_vec, *val);
            return;
        }
    }

    _val_t val = _create_string_val(field, string_val);
    vec_push(&mdata_file->vals_vec, val);
    map_set(&mdata_file->vals_map, field, val);
}

void mdata_file_add_data(mdata_file_t *mdata_file, const char *field, char *data, int data_len, bool compress) {
    _val_t val = _create_data_val(field, data, data_len);
    vec_push(&mdata_file->vals_vec, val);
    map_set(&mdata_file->vals_map, field, val);
}
