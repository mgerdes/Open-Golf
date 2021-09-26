#include <assert.h>
#include <stdbool.h>

#include "3rd_party/cute_headers/cute_files.h"
#include "3rd_party/map/map.h"
#include "3rd_party/vec/vec.h"
#include "mcore/mcommon.h"
#include "mcore/mimport.h"
#include "mcore/mlog.h"
#include "mcore/mstring.h"
#include "golf/data_stream.h"
#include "golf/log.h"

#ifdef MEMBED_FILES
#include "membedder/membedded_files.h"
#endif

typedef struct _mimporter {
    const char *ext;
    void (*callback)(mdatafile_t *file, void *udata);
    void *udata;
} _mimporter_t;

typedef map_t(_mimporter_t) _map_mimporter_t;
static _map_mimporter_t _importers_map;

static void _create_texture_mdatafile(mdatafile_t *file, unsigned char *data, int data_len) {
    mdatafile_add_string(file, "filter", "linear", true);
    mdatafile_add_data(file, "data", data, data_len);
}

static void _create_default_mdatafile(mdatafile_t *file, unsigned char *data, int data_len) {
    mdatafile_add_data(file, "data", data, data_len);
}

static void _run_import(cf_file_t *file, void *udata) {
    _mimporter_t *importer = (_mimporter_t*)udata;
    if (strcmp(file->ext, importer->ext) != 0) {
        return ;
    }

    mdatafile_t *mdatafile = mdatafile_load(file->path);
    importer->callback(mdatafile, importer->udata);
    mdatafile_delete(mdatafile);
}

static void _visit_file(cf_file_t *file, void *udata) {
    if (strcmp(file->ext, ".mdata") == 0) {
        return;
    }

    mlog_note("Importing file %s", file->path);

    unsigned char *data;
    int data_len;
    if (!mread_file(file->path, &data, &data_len)) {
        mlog_error("Could not load file %s.", file->path);
    }

    mdatafile_t *mdatafile = mdatafile_load(file->path);
    mdatafile_cache_old_vals(mdatafile);

    if ((strcmp(file->ext, ".png") == 0) ||
            (strcmp(file->ext, ".bmp") == 0) ||
            (strcmp(file->ext, ".jpg") == 0)) {
        _create_texture_mdatafile(mdatafile, data, data_len);
    }
    else if ((strcmp(file->ext, ".cfg") == 0) ||
            (strcmp(file->ext, ".mscript") == 0) ||
            (strcmp(file->ext, ".ogg") == 0) ||
            (strcmp(file->ext, ".fnt") == 0) ||
            (strcmp(file->ext, ".terrain_model") == 0) ||
            (strcmp(file->ext, ".hole") == 0) ||
            (strcmp(file->ext, ".model") == 0)) {
        _create_default_mdatafile(mdatafile, data, data_len);
    }
    else {
        mlog_warning("Don't know how to create mdatafile for %s", file->path);
    }

    _mimporter_t *importer = map_get(&_importers_map, file->ext);
    if (importer) {
        (*importer->callback)(mdatafile, importer->udata);
    }

    free(data);
    mdatafile_save(mdatafile);
    mdatafile_delete(mdatafile);
}

void mimport_init(void) {
    map_init(&_importers_map);
    //mimport_run();
}

void mimport_add_importer(const char *ext, void (*callback)(mdatafile_t *file, void *udata), void *udata) {
    _mimporter_t importer;
    importer.ext = ext;
    importer.callback = callback;
    importer.udata = udata;
    map_set(&_importers_map, ext, importer);
#ifdef MEMBED_FILES
    for (int i = 0; i < _num_embedded_files; i++) {
        if (strcmp(_embedded_files[i].ext, ext) == 0) {
            mdatafile_t *mdatafile = mdatafile_load(_embedded_files[i].path);
            callback(mdatafile, udata);
            mdatafile_delete(mdatafile);
        }
    }
#else
    cf_traverse("data", _run_import, &importer);
#endif
}

void mimport_run(bool always_run) {
    mlog_note("mimport_run");
#ifdef MEMBED_FILES
    if (always_run) {
        cf_traverse("data", _visit_file, NULL);
    }
#else
    cf_traverse("data", _visit_file, NULL);
#endif
}

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
    char name[1024];
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
    val.data_val = malloc(data_len + 1);
    memcpy(val.data_val, data, data_len);
    val.data_val[data_len] = 0;
    val.data_val_len = data_len;
    return val;
}

mdatafile_t *mdatafile_load(const char *path) {
    mdatafile_t *mdatafile = malloc(sizeof(mdatafile_t));
    vec_init(&mdatafile->vals_vec);
    mdatafile->has_cached_vals = false;

    snprintf(mdatafile->name, 1024, "%s", path);
    snprintf(mdatafile->path, 1024, "%s.mdata", path);
    unsigned char *data = NULL;
    int data_len = 0;

#if MEMBED_FILES
    for (int i = 0; i < _num_embedded_files; i++) {
        if (strcmp(_embedded_files[i].path, mdatafile->name) == 0) {
            data = _embedded_files[i].data;
            data_len = _embedded_files[i].data_len;
            break;
        }
    }
#else
    if (!mread_file(mdatafile->path, &data, &data_len)) {
        return mdatafile;
    }
#endif

    if (!data) {
        mlog_error("Unable to load data for mdatafile %s", mdatafile->path);
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

#if MEMBED_FILES
#else
    free(data);
#endif
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

static bool _find_val(_vec_val_t *vals_vec, const char *name, enum _val_type type, _val_t *val) {
    for (int i = 0; i < vals_vec->length; i++) {
        if (type == vals_vec->data[i].type 
                && strcmp(vals_vec->data[i].name, name) == 0) {
            *val = vals_vec->data[i];
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

const char *mdatafile_get_name(mdatafile_t *file) {
    return file->name;
}

bool mdatafile_get_string(mdatafile_t *file, const char *name, const char **string) {
    _val_t val;
    if (_find_val(&file->vals_vec, name, _VAL_TYPE_STRING, &val)) {
        *string = val.string_val;
        return true;
    }
    else {
        *string = NULL;
        return false;
    }
}

bool mdatafile_get_data(mdatafile_t *file, const char *name, unsigned char **data, int *data_len) {
    _val_t val;
    if (_find_val(&file->vals_vec, name, _VAL_TYPE_DATA, &val)) {
        *data = val.data_val;
        *data_len = val.data_val_len;
        return true;
    }
    else {
        *data = NULL;
        *data_len = 0;
        return false;
    }
}
