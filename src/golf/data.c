#include "golf/data.h"

#include "3rd_party/fast_obj/fast_obj.h"
#include "3rd_party/map/map.h"
#include "3rd_party/parson/parson.h"
#include "3rd_party/stb/stb_image_write.h"
#include "3rd_party/stb/stb_truetype.h"
#include "golf/base64.h"
#include "golf/file.h"
#include "golf/log.h"
#include "golf/string.h"

typedef enum golf_data_file_type {
    GOLF_DATA_FILE_SCRIPT,
} golf_data_file_type_t;

typedef struct golf_data_file {
    int load_count;
    golf_file_t file;
    golf_file_t file_to_load;
    golf_filetime_t last_load_time;

    golf_data_file_type_t type; 
    union {
        golf_data_script_t *script;
    };
} golf_data_file_t;

typedef map_t(golf_data_file_t) map_golf_data_file_t;

typedef struct golf_data {
    map_golf_data_file_t loaded_files;
} golf_data_t;

static golf_data_t _golf_data;

static bool _golf_data_script_load(const char *path, char *data, int data_len) {
    golf_data_script_t *script = malloc(sizeof(golf_data_script_t));
    golf_string_init(&script->src, data);
    return true;
}

static bool _golf_data_script_reload(const char *path, char *data, int data_len) {
    return true;
}

static bool _golf_data_script_unload(const char *path) {
    return true;
}

void golf_data_init(void) {
}

void golf_data_run_import(void) {
    golf_dir_t dir;
    golf_dir_init(&dir, "data", true);

    for (int i = 0; i < dir.num_files; i++) {
        golf_file_t file = dir.files[i];
        golf_file_t import_file = golf_file_append_extension(file.path, ".import");
        if (golf_file_cmp_time(&file, &import_file) < 0.0f) {
            continue;
        }

        if (!golf_file_load_data(&file)) {
            golf_log_warning("Unable to load file %s", file.path); 
            continue;
        }

        /*
        for (int i = 0; i <  _golf_data.importers.length; i++) {
            golf_data_importer_t importer = _golf_data.importers.data[i];
            if (strcmp(importer.ext, file.ext) == 0) {
                golf_log_note("Importing file %s", file.path);
                importer.import(file.path, file.data, file.data_len);
            }
        }
        */

        golf_file_free_data(&file);
    }

    golf_dir_deinit(&dir);
}

void golf_data_update(float dt) {
    const char *key;
    map_iter_t iter = map_iter(&_golf_data.loaded_files);

    while ((key = map_next(&_golf_data.loaded_files, &iter))) {
        golf_data_file_t *loaded_file = map_get(&_golf_data.loaded_files, key);

        /*
        golf_filetime_t file_time;
        golf_file_get_time(&loaded_file->file, &file_time);
        if (golf_filetime_cmp(loaded_file->last_load_time, file_time) < 0.0f) {
            golf_file_t file = golf_file(key);
            golf_file_t file_to_load = loaded_file->file;
            if (golf_file_load_data(&file_to_load)) {
                golf_log_note("Reloading file %s", key);

                for (int i = 0; i < _golf_data.loaders.length; i++) {
                    golf_data_loader_t loader = _golf_data.loaders.data[i];
                    if (strcmp(loader.ext, file.ext) == 0) {
                        loader.reload(file.path, file_to_load.data, file_to_load.data_len);
                    }
                }

                loaded_file->last_load_time = file_time;
                golf_file_free_data(&file_to_load);
            }
            else {
                golf_log_warning("Unable to load file %s", key);
            }
        }
        */
    }
}

void golf_data_load_file(const char *path) {
    golf_log_note("Loading file %s", path);

    /*
    {
        golf_data_file_t *loaded_file = map_get(&_golf_data.loaded_files, path);
        if (loaded_file) {
            loaded_file->load_count++;
            return;
        }
    }

    golf_file_t file = golf_file(path);
    golf_file_t file_to_load = file;    
    for (int i = 0; i < _golf_data.importers.length; i++) {
        if (strcmp(_golf_data.importers.data[i].ext, file.ext) == 0) {
            file_to_load = golf_file_append_extension(path, ".import");
            break;
        }
    }

    if (!golf_file_load_data(&file_to_load)) {
        golf_log_warning("Unable to load file %s", file_to_load.path);
        return;
    }

    for (int i = 0; i < _golf_data.loaders.length; i++) {
        golf_data_loader_t loader = _golf_data.loaders.data[i];
        if (strcmp(loader.ext, file.ext) == 0) {
            loader.load(path, file_to_load.data, file_to_load.data_len);
        }
    }

    golf_file_free_data(&file_to_load);

    {
        golf_data_file_t loaded_file;
        loaded_file.load_count = 1;
        loaded_file.file = file_to_load;
        golf_file_get_time(&file_to_load, &loaded_file.last_load_time);
        map_set(&_golf_data.loaded_files, path, loaded_file);
    }
    */
}

void golf_data_unload_file(const char *path) {
}
