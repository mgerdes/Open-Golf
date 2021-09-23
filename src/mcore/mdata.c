#define _CRT_SECURE_NO_WARNINGS

#include <assert.h>

#include "3rd_party/map/map.h"
#include "3rd_party/vec/vec.h"
#include "mcore/mdata.h"
#include "mcore/mcommon.h"
#include "mcore/mlog.h"
#include "mcore/mstring.h"
#include "golf/data_stream.h"
#include "golf/hotloader.h"
#include "golf/log.h"

#define _VAL_MAX_NAME_LEN 32

enum _val_type {
    _VAL_TYPE_INT,
    _VAL_TYPE_STRING,
    _VAL_TYPE_DATA,
};

typedef struct _val {
    enum _val_type type;
    union {
        int int_val;
        struct {
            unsigned char *data_val;
            int data_val_len;
        };
        char *string_val;
    };
    char name[_VAL_MAX_NAME_LEN];
    bool user_set;
} _val_t;

typedef vec_t(_val_t) _vec_val_t;

typedef struct mdatafile {
    char path[1024];
    _vec_val_t vals_vec;
    bool has_cached_vals;
    _vec_val_t cached_vals_vec;
} mdatafile_t;

static _val_t _create_int_val(const char *name, int int_val) {
    _val_t val;
    val.type = _VAL_TYPE_INT;
    mstrncpy(val.name, name, _VAL_MAX_NAME_LEN);
    val.int_val = int_val;
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

static _val_t _create_data_val(const char *name, const unsigned char *data, int data_len) {
    _val_t val;
    val.type = _VAL_TYPE_DATA;
    mstrncpy(val.name, name, _VAL_MAX_NAME_LEN);
    val.data_val = malloc(data_len);
    memcpy(val.data_val, data, data_len);
    val.data_val_len = data_len;
    return val;
}

mdatafile_t *mdatafile_load(const char *path) {
    mdatafile_t *mdatafile = malloc(sizeof(mdatafile_t));
    vec_init(&mdatafile->vals_vec);
    mdatafile->has_cached_vals = false;

    snprintf(mdatafile->path, 1024, "%s.mdata", path);
    unsigned char *data;
    int data_len;
    if (!mread_file(mdatafile->path, &data, &data_len)) {
        return mdatafile;
    }

    int i = 0;
    while (i < data_len) {
        int type_len = 0;
        char type[32]; 

        int name_len = 0;
        char name[_VAL_MAX_NAME_LEN + 1];

        while (data[i] != ' ') {
            if (i >= data_len) assert(false);
            type[type_len++] = data[i++];
            if (type_len >= 32) {
                assert(false);
            }
        }
        type[type_len] = 0;

        if (i >= data_len) assert(false);
        i++;

        while (data[i] != ' ') {
            if (i >= data_len) assert(false);
            name[name_len++] = data[i++];
            if (name_len >= _VAL_MAX_NAME_LEN + 1) {
                assert(false);
            }
        }
        name[name_len] = 0;

        if (i >= data_len) assert(false);
        i++;

        _val_t val;
        memset(&val, 0, sizeof(val));
        if (strcmp(type, "int") == 0) {
            int int_val = 0;
            while (data[i] != '\n') {
                if (data[i] < '0' || data[i] > '9') {
                    assert(false);
                }

                int_val *= 10;
                int_val += data[i++] - '0';
            }
            i++;

            mdatafile_add_int(mdatafile, name, int_val, false);
        }
        else if (strcmp(type, "string") == 0) {
            mstring_t str;
            mstring_init(&str, "");
            while (data[i] != '\n') {
                mstring_append_char(&str, data[i++]);
            }
            i++;

            mdatafile_add_string(mdatafile, name, str.cstr, false);
            mstring_deinit(&str);
        }
        else if (strcmp(type, "data") == 0) {
            mstring_t str;
            mstring_init(&str, "");
            while (data[i] != '\n') {
                mstring_append_char(&str, data[i++]);
            }
            i++;

            int data_len;
            unsigned char *data = mbase64_decode(str.cstr, str.len, &data_len); 
            mdatafile_add_data(mdatafile, name, data, data_len);
            free(data);
            mstring_deinit(&str);
        }
        else {
            mlog_error("Failed loading mdatafile %s. Unknown type %s.", mdatafile->path, type);
        }
    }

    free(data);
    return mdatafile;
}

static void _delete_vals_vec(_vec_val_t *vec) {
    for (int i = 0; i < vec->length; i++) {
        switch (vec->data[i].type) {
            case _VAL_TYPE_INT:
                break;
            case _VAL_TYPE_STRING:
                free(vec->data[i].string_val);
                break;
            case _VAL_TYPE_DATA:
                free(vec->data[i].data_val);
                break;
        }
    }
    vec_deinit(vec);
}

void mdatafile_delete(mdatafile_t *file) {
    _delete_vals_vec(&file->vals_vec);
    if (file->has_cached_vals) {
        _delete_vals_vec(&file->cached_vals_vec);
    }
    free(file);
}

void mdatafile_save(mdatafile_t *file) {
    mstring_t str;
    mstring_init(&str, "");
    for (int i = 0; i < file->vals_vec.length; i++) {
        _val_t val = file->vals_vec.data[i];
        switch (val.type) {
            case _VAL_TYPE_INT:
                mstring_appendf(&str, "int %s %d\n", val.name, val.int_val);
                break;
            case _VAL_TYPE_STRING:
                mstring_appendf(&str, "string %s %s\n", val.name, val.string_val);
                break;
            case _VAL_TYPE_DATA:
                {
                    char *enc = mbase64_encode(val.data_val, val.data_val_len);
                    mstring_appendf(&str, "data %s %s\n", val.name, enc);
                    free(enc);
                }
                break;
        }
    }
    mwrite_file(file->path, str.cstr, str.len);
    mstring_deinit(&str);
}

void mdatafile_cache_old_vals(mdatafile_t *file) {
    if (file->has_cached_vals) {
        _delete_vals_vec(&file->cached_vals_vec);
    }

    file->has_cached_vals = true;
    file->cached_vals_vec = file->vals_vec; 
    vec_init(&file->vals_vec);
}

static bool _find_cached_val(mdatafile_t *file, const char *name, enum _val_type type, _val_t *val) {
    if (!file->has_cached_vals) {
        return false;
    }

    for (int i = 0; i < file->cached_vals_vec.length; i++) {
        if (type == file->cached_vals_vec.data[i].type 
                && strcmp(file->cached_vals_vec.data[i].name, name) == 0) {
            *val = file->cached_vals_vec.data[i];
            return true;
        }
    }
    return false;
}

void mdatafile_add_int(mdatafile_t *file, const char *name, int int_val, bool user_set) {
    _val_t val, cached_val;
    if (user_set && _find_cached_val(file, name, _VAL_TYPE_INT, &cached_val)) {
        val = _create_int_val(name, cached_val.int_val);
    }
    else {
        val = _create_int_val(name, int_val);
    }
    vec_push(&file->vals_vec, val);
}

void mdatafile_add_string(mdatafile_t *file, const char *name, const char *string, bool user_set) {
    _val_t val, cached_val;
    if (user_set && _find_cached_val(file, name, _VAL_TYPE_STRING, &cached_val)) {
        val = _create_string_val(name, cached_val.string_val);
    }
    else {
        val = _create_string_val(name, string);
    }
    vec_push(&file->vals_vec, val);
}

void mdatafile_add_data(mdatafile_t *file, const char *name, unsigned char *data, int data_len) {
    _val_t val = _create_data_val(name, data, data_len);
    vec_push(&file->vals_vec, val);
}
