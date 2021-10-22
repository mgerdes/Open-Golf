#include "mcore/mdata.h"

#include "3rd_party/fast_obj/fast_obj.h"
#include "3rd_party/parson/parson.h"
#include "3rd_party/stb/stb_image_write.h"
#include "3rd_party/stb/stb_truetype.h"
#include "mcore/mcommon.h"
#include "mcore/mdata_font.c"
#include "mcore/mdata_model.c"
#include "mcore/mdata_script.c"
#include "mcore/mdata_shader.c"
#include "mcore/mdata_texture.c"
#include "mcore/mfile.h"
#include "mcore/mlog.h"
#include "mcore/mstring.h"

typedef struct mdata_loader {
    const char *ext;
    bool(*load)(const char *path, char *data, int data_len);
    bool(*unload)(const char *path);
    bool(*reload)(const char *path, char *data, int data_len);
} mdata_loader_t;

typedef vec_t(mdata_loader_t) vec_mdata_loader_t;

typedef struct mdata_importer {
    const char *ext;
    bool(*import)(const char *path, char *data, int data_len);
} mdata_importer_t;

typedef vec_t(mdata_importer_t) vec_mdata_importer_t;

typedef struct mdata_file {
    int load_count;
    mfile_t file;
    mfile_t file_to_load;
    mfiletime_t last_load_time;
} mdata_file_t;

typedef map_t(mdata_file_t) map_mdata_file_t;

typedef struct mdata {
    map_mdata_file_t loaded_files;
    vec_mdata_loader_t loaders; 
    vec_mdata_importer_t importers; 
} mdata_t;

static mdata_t _mdata;

void mdata_init(void) {
    map_init(&_mdata.loaders);
    vec_init(&_mdata.loaders);
    vec_init(&_mdata.importers);

    mdata_add_importer(".ttf", mdata_font_import);
    mdata_add_importer(".png", mdata_texture_import);
    mdata_add_importer(".jpg", mdata_texture_import);
    mdata_add_importer(".bmp", mdata_texture_import);
    mdata_add_importer(".glsl", mdata_shader_import);
    mdata_add_importer(".obj", mdata_model_import);

    mdata_add_loader(".ttf", mdata_font_load, mdata_font_unload, mdata_font_reload);
}

void mdata_run_import(void) {
    mdir_t dir;
    mdir_init(&dir, "data", true);

    for (int i = 0; i < dir.num_files; i++) {
        mfile_t file = dir.files[i];
        mfile_t import_file = mfile_append_extension(file.path, ".import");
        if (mfile_cmp_time(&file, &import_file) < 0.0f) {
            continue;
        }

        if (!mfile_load_data(&file)) {
            mlog_warning("mdata_run_import: Unable to load file %s", file.path); 
            continue;
        }

        for (int i = 0; i <  _mdata.importers.length; i++) {
            mdata_importer_t importer = _mdata.importers.data[i];
            if (strcmp(importer.ext, file.ext) == 0) {
                mlog_note("Importing file %s", file.path);
                importer.import(file.path, file.data, file.data_len);
            }
        }

        mfile_free_data(&file);
    }

    mdir_deinit(&dir);
}

void mdata_update(float dt) {
    const char *key;
    map_iter_t iter = map_iter(&_mdata.loaded_files);

    while ((key = map_next(&_mdata.loaded_files, &iter))) {
        mdata_file_t *loaded_file = map_get(&_mdata.loaded_files, key);

        mfiletime_t file_time;
        mfile_get_time(&loaded_file->file, &file_time);
        if (mfiletime_cmp(loaded_file->last_load_time, file_time) < 0.0f) {
            mfile_t file = mfile(key);
            mfile_t file_to_load = loaded_file->file;
            if (mfile_load_data(&file_to_load)) {
                mlog_note("Reloading file %s", key);

                for (int i = 0; i < _mdata.loaders.length; i++) {
                    mdata_loader_t loader = _mdata.loaders.data[i];
                    if (strcmp(loader.ext, file.ext) == 0) {
                        loader.reload(file.path, file_to_load.data, file_to_load.data_len);
                    }
                }

                loaded_file->last_load_time = file_time;
                mfile_free_data(&file_to_load);
            }
            else {
                mlog_warning("Unable to load file %s", key);
            }
        }
    }
}

void mdata_add_loader(const char *ext, bool(*load)(const char *path, char *data, int data_len), bool(*unload)(const char *path), bool(*reload)(const char *path, char *data, int data_len)) {
    mdata_loader_t loader;
    loader.ext = ext;
    loader.load = load;
    loader.unload = unload;
    loader.reload = reload;
    vec_push(&_mdata.loaders, loader);
}

void mdata_add_importer(const char *ext, bool(*import)(const char *path, char *data, int data_len)) {
    mdata_importer_t importer;
    importer.ext = ext;
    importer.import = import;
    vec_push(&_mdata.importers, importer);
}

void mdata_load_file(const char *path) {
    mlog_note("Loading file %s", path);

    {
        mdata_file_t *loaded_file = map_get(&_mdata.loaded_files, path);
        if (loaded_file) {
            loaded_file->load_count++;
            return;
        }
    }

    mfile_t file = mfile(path);
    mfile_t file_to_load = file;    
    for (int i = 0; i < _mdata.importers.length; i++) {
        if (strcmp(_mdata.importers.data[i].ext, file.ext) == 0) {
            file_to_load = mfile_append_extension(path, ".import");
            break;
        }
    }

    if (!mfile_load_data(&file_to_load)) {
        mlog_warning("Unable to load file %s", file_to_load.path);
        return;
    }

    for (int i = 0; i < _mdata.loaders.length; i++) {
        mdata_loader_t loader = _mdata.loaders.data[i];
        if (strcmp(loader.ext, file.ext) == 0) {
            loader.load(path, file_to_load.data, file_to_load.data_len);
        }
    }

    mfile_free_data(&file_to_load);

    {
        mdata_file_t loaded_file;
        loaded_file.load_count = 1;
        loaded_file.file = file_to_load;
        mfile_get_time(&file_to_load, &loaded_file.last_load_time);
        map_set(&_mdata.loaded_files, path, loaded_file);
    }
}

void mdata_unload_file(const char *path) {
}
