#include "golf/data.h"

#include "3rd_party/fast_obj/fast_obj.h"
#include "3rd_party/map/map.h"
#include "3rd_party/parson/parson.h"
#include "3rd_party/stb/stb_image_write.h"
#include "3rd_party/stb/stb_truetype.h"
#include "golf/base64.h"
#include "golf/file.h"
#include "golf/log.h"
#include "golf/maths.h"
#include "golf/string.h"

typedef struct golf_data_allocator_section {
    int bytes_allocated;
} golf_data_allocator_section_t;

typedef map_t(golf_data_allocator_section_t) map_golf_data_allocator_section_t;

typedef struct golf_data_allocator {
    const char *cur_section;
    map_golf_data_allocator_section_t sections; 
} golf_data_allocator_t;

typedef enum golf_data_file_type {
    GOLF_DATA_SCRIPT,
    GOLF_DATA_TEXTURE,
} golf_data_file_type_t;

typedef struct golf_data_file {
    int load_count;
    golf_file_t file;
    golf_file_t file_to_load;
    golf_filetime_t last_load_time;

    golf_data_file_type_t type; 
    union {
        golf_data_script_t *script;
        golf_data_texture_t *texture;
    };
} golf_data_file_t;

typedef map_t(golf_data_file_t) map_golf_data_file_t;

typedef struct golf_data {
    golf_data_allocator_t allocator;
    map_golf_data_file_t loaded_files;
} golf_data_t;

typedef bool (*golf_data_importer_t)(const char *path, char *data, int data_len);
typedef bool (*golf_data_loader_t)(const char *path, char *data, int data_len, golf_data_file_t *data_file);
typedef bool (*golf_data_unloader)(golf_data_file_t *data_file);

static golf_data_t _golf_data;

//
// ALLOCATOR
//

static void _golf_data_allocator_set_section(const char *cur_section) {
    if (!cur_section) {
        golf_log_error("Can't set cur_section to NULL");
    }
    if (_golf_data.allocator.cur_section[0]) {
        golf_log_error("Can't set cur_section, there's already one set");
    }
    _golf_data.allocator.cur_section = cur_section;
}

static golf_data_allocator_section_t *_golf_data_allocator_get_section(void) {
    const char *cur_section = _golf_data.allocator.cur_section;
    golf_data_allocator_section_t *section = map_get(&_golf_data.allocator.sections, cur_section);
    if (!section) {
        golf_data_allocator_section_t new_section;
        new_section.bytes_allocated = 0;
        map_set(&_golf_data.allocator.sections, cur_section, new_section);
        section = map_get(&_golf_data.allocator.sections, cur_section);
    }
    return section;
} 

static void *_golf_data_alloc(size_t n) {
    golf_data_allocator_section_t *section = _golf_data_allocator_get_section();
    char *mem = malloc(sizeof(int) + n);
    ((int*)mem)[0] = (int)n;
    section->bytes_allocated += (int)n;
    return mem + sizeof(int);
}

static void _golf_data_free(void *ptr) {
    if (!ptr) {
        return;
    }

    golf_data_allocator_section_t *section = _golf_data_allocator_get_section();
    char *mem = ((char*)ptr) - sizeof(int);
    int n = ((int*)mem)[0];
    section->bytes_allocated -= n;
    free(mem);
}

static void *_golf_data_realloc(void *ptr, size_t new_sz) {
    if (new_sz == 0) {
        return NULL;
    }

    golf_data_allocator_section_t *section = _golf_data_allocator_get_section();
    section->bytes_allocated += new_sz;

    char *new_mem = malloc(sizeof(int) + new_sz);
    ((int*)new_mem)[0] = (int)new_sz;

    if (ptr) {
        char *mem = ((char*)ptr) - sizeof(int);
        int n = ((int*)mem)[0];
        section->bytes_allocated -= n;
        if (n > new_sz) {
            n = new_sz;
        }
        memcpy(new_mem + sizeof(int), mem + sizeof(int), n);
        free(mem);
    }

    return new_mem + sizeof(int);
}

//
// PARSON HELPERS
//

void json_object_get_data(JSON_Object *obj, const char *name, unsigned char **data, int *data_len) {
    const char *enc_data = json_object_get_string(obj, name); 
    int enc_len = strlen(enc_data);
    *data_len = golf_base64_decode_out_len(enc_data, enc_len);
    *data = _golf_data_alloc(*data_len);
    if (!golf_base64_decode(enc_data, enc_len, *data)) {
        golf_log_warning("Failed to decode data in field %s", name);
    }
}

void json_object_set_data(JSON_Object *obj, const char *name, unsigned char *data, int data_len) {
    int enc_len = golf_base64_encode_out_len(data, data_len);
    char *enc_data = _golf_data_alloc(enc_len);
    if (!golf_base64_encode(data, data_len, enc_data)) {
        golf_log_warning("Failed to encode data in field %s", name);
    }
    json_object_set_string(obj, name, enc_data);
    _golf_data_free(enc_data);
}

vec2 json_object_get_vec2(JSON_Object *obj, const char *name) {
    JSON_Array *array = json_object_get_array(obj, name);
    vec2 v;
    v.x = (float)json_array_get_number(array, 0);
    v.y = (float)json_array_get_number(array, 1);
    return v;
}

vec3 json_object_get_vec3(JSON_Object *obj, const char *name) {
    JSON_Array *array = json_object_get_array(obj, name);
    vec3 v;
    v.x = (float)json_array_get_number(array, 0);
    v.y = (float)json_array_get_number(array, 1);
    v.z = (float)json_array_get_number(array, 2);
    return v;
}

vec4 json_object_get_vec4(JSON_Object *obj, const char *name) {
    JSON_Array *array = json_object_get_array(obj, name);
    vec4 v;
    v.x = (float)json_array_get_number(array, 0);
    v.y = (float)json_array_get_number(array, 1);
    v.z = (float)json_array_get_number(array, 2);
    v.w = (float)json_array_get_number(array, 3);
    return v;
}

//
// SCRIPTS
//

static bool _golf_data_script_load(const char *path, char *data, int data_len) {
    golf_data_script_t *script = _golf_data_alloc(sizeof(golf_data_script_t));
    golf_string_init(&script->src, data);
    return true;
}

static bool _golf_data_script_reload(const char *path, char *data, int data_len) {
    return true;
}

static bool _golf_data_script_unload(const char *path) {
    return true;
}

//
// TEXTURES
//

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC _golf_data_alloc
#define STBI_FREE _golf_data_free
#define STBI_REALLOC _golf_data_realloc
#include "3rd_party/stb/stb_image.h"

static bool _golf_data_texture_import(const char *path, char *data, int data_len) {
    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    json_object_set_string(obj, "filter", "linear");

    printf("%s %d\n", path, data_len);
    {
        int x, y, n;
        int force_channels = 4;
        stbi_set_flip_vertically_on_load(0);
        unsigned char *img_data = stbi_load_from_memory((unsigned char*) data, data_len, &x, &y, &n, force_channels);
        if (!img_data) {
            golf_log_warning("STB Failed to load image");
        }
        json_object_set_number(obj, "x", x);
        json_object_set_number(obj, "y", y);
        json_object_set_number(obj, "n", n);
        json_object_set_data(obj, "img_data", img_data, n * x * y);
        _golf_data_free(img_data);
    }

    golf_string_t import_texture_file_path;
    golf_string_initf(&import_texture_file_path, "%s.import", path);
    json_serialize_to_file_pretty(val, import_texture_file_path.cstr);
    golf_string_deinit(&import_texture_file_path);
    json_value_free(val);
    return true;
}

static bool _golf_data_texture_load(const char *path, char *data, int data_len, golf_data_file_t *datafile) {
    JSON_Value *val = json_parse_string(data);

    if (!val) {
        golf_log_warning("Unable to parse json for file %s", path);
        return false;
    }

    JSON_Object *obj = json_value_get_object(val);
    if (!obj) {
        golf_log_warning("Unable to get object for json file %s", path);
        return false;
    }

    sg_filter filter = SG_FILTER_LINEAR;
    {
        const char *filter_str = json_object_get_string(obj, "filter");
        if (filter_str && strcmp(filter_str, "linear") == 0) {
            filter = SG_FILTER_LINEAR;
        }
        else if (filter_str && strcmp(filter_str, "nearest") == 0) {
            filter = SG_FILTER_NEAREST;
        }
    }

    sg_image image;
    int width, height;
    {
        int img_data_len;
        unsigned char *img_data;
        json_object_get_data(obj, "img_data", &img_data, &img_data_len);
        int x = (int)json_object_get_number(obj, "x");
        int y = (int)json_object_get_number(obj, "y");
        sg_image_desc img_desc = {
            .width = x,
            .height = y,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .min_filter = filter,
            .mag_filter = filter,
            .wrap_u = SG_WRAP_REPEAT,
            .wrap_v = SG_WRAP_REPEAT,
            .data.subimage[0][0] = {
                .ptr = img_data,
                .size = img_data_len,
            },
        };
        width = x;
        height = y;
        image = sg_make_image(&img_desc);
        _golf_data_free(img_data);
    }

    json_value_free(val);

    datafile->type = GOLF_DATA_TEXTURE;
    datafile->texture = _golf_data_alloc(sizeof(golf_data_texture_t));
    datafile->texture->image = image;
    datafile->texture->width = width;
    datafile->texture->height = height;
    return true;
}

static bool _golf_data_texture_unload(golf_data_file_t *data) {
    return true;
}

//
// SHADERS
//

static JSON_Value *_golf_data_shader_import_bare(const char *base_name, const char *name) {
    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    {
        golf_string_t fs_bare_name;
        golf_string_initf(&fs_bare_name, "%s_%s_fs.glsl", base_name, name);
        golf_file_t fs_file = golf_file(fs_bare_name.cstr);
        if (!golf_file_load_data(&fs_file)) {
            golf_log_error("Failed to read file %s", fs_bare_name.cstr);
        }
        json_object_set_string(obj, "fs", fs_file.data);
        golf_file_free_data(&fs_file);
        golf_string_deinit(&fs_bare_name);
    }

    {
        golf_string_t vs_bare_name;
        golf_string_initf(&vs_bare_name, "%s_%s_vs.glsl", base_name, name);
        golf_file_t vs_file = golf_file(vs_bare_name.cstr);
        if (!golf_file_load_data(&vs_file)) {
            golf_log_error("Failed to read file %s", vs_bare_name.cstr);
        }
        json_object_set_string(obj, "vs", vs_file.data);
        golf_file_free_data(&vs_file);
        golf_string_deinit(&vs_bare_name);
    }

    return val;
}

bool _golf_data_shader_import(const char *path, char *data, int data_len) {
    golf_file_t file = golf_file(path);
    static const char *slangs = "glsl330:glsl300es";
    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    {
        golf_string_t cmd;
#if GOLF_PLATFORM_LINUX
        golf_string_initf(&cmd, "tools/sokol-tools/linux/sokol-shdc --input %s --output src/golf/shaders/%s.h --slang %s", file.path, file.name, slangs);
#elif GOLF_PLATFORM_WINDOWS
        golf_string_initf(&cmd, "tools\\sokol-tools\\win32\\sokol-shdc --input %s --output src/golf/shaders/%s.h --slang %s", file.path, file.name, slangs);
#elif GOLF_PLATFORM_MACOS
#endif
        int ret = system(cmd.cstr);
        golf_string_deinit(&cmd);
    }

    {
        golf_string_t cmd;
#if GOLF_PLATFORM_LINUX
        golf_string_initf(&cmd, "tools/sokol-tools/linux/sokol-shdc --input %s --output out/temp/bare --slang %s --format bare", file.path, slangs);
#elif GOLF_PLATFORM_WINDOWS
        golf_string_initf(&cmd, "tools\\sokol-tools\\win32\\sokol-shdc --input %s --output out/temp/bare --slang %s --format bare", file.path, slangs);
#elif GOLF_PLATFORM_MACOS
#endif
        int ret = system(cmd.cstr);
        if (ret == 0) {
            golf_string_t base_bare_name;
            golf_string_initf(&base_bare_name, "out/temp/bare_%s", file.name);
            golf_string_pop(&base_bare_name, 5);

            {
                JSON_Value *bare_val = _golf_data_shader_import_bare(base_bare_name.cstr, "glsl300es");
                json_object_set_value(obj, "glsl300es", bare_val);
            }

            {
                JSON_Value *bare_val = _golf_data_shader_import_bare(base_bare_name.cstr, "glsl330");
                json_object_set_value(obj, "glsl330", bare_val);
            }

            golf_string_deinit(&base_bare_name);
        }
        golf_string_deinit(&cmd);
    }

    golf_string_t import_shader_file_path;
    golf_string_initf(&import_shader_file_path, "%s.import", file.path);
    json_serialize_to_file_pretty(val, import_shader_file_path.cstr);
    golf_string_deinit(&import_shader_file_path);

    json_value_free(val);
    return true;
}

//
// DATA
//

void golf_data_init(void) {
    _golf_data.allocator.cur_section = "";
    json_set_allocation_functions(_golf_data_alloc, _golf_data_free);
    map_init(&_golf_data.allocator.sections);
    map_init(&_golf_data.loaded_files);
}

golf_data_importer_t _golf_data_get_ext_importer(const char *ext) { 
    if ((strcmp(ext, ".png") == 0) ||
            (strcmp(ext, ".jpg") == 0) ||
            (strcmp(ext, ".bmp") == 0)) {
        return &_golf_data_texture_import;
    }
    else if ((strcmp(ext, ".glsl") == 0)) {
        return &_golf_data_shader_import;
    }
    else {
        return NULL;
    }
}

golf_data_loader_t _golf_data_get_ext_loader(const char *ext) { 
    if ((strcmp(ext, ".png") == 0) ||
            (strcmp(ext, ".jpg") == 0) ||
            (strcmp(ext, ".bmp") == 0)) {
        return &_golf_data_texture_load;
    }
    else {
        return NULL;
    }
}

golf_data_unloader _golf_data_get_ext_unloader(const char *ext) { 
    if ((strcmp(ext, ".png") == 0) ||
            (strcmp(ext, ".jpg") == 0) ||
            (strcmp(ext, ".bmp") == 0)) {
        return &_golf_data_texture_unload;
    }
    else {
        return NULL;
    }
}

void golf_data_run_import(void) {
    golf_dir_t dir;
    golf_dir_init(&dir, "data", true);

    for (int i = 0; i < dir.num_files; i++) {
        golf_file_t file = dir.files[i];
        golf_data_importer_t importer = _golf_data_get_ext_importer(file.ext);

        if (importer) {
            golf_file_t import_file = golf_file_append_extension(file.path, ".import");
            if (golf_file_cmp_time(&file, &import_file) < 0.0f) {
                continue;
            }

            golf_log_note("Importing file %s", file.path);

            if (!golf_file_load_data(&file)) {
                golf_log_warning("Unable to load file %s", file.path); 
                continue;
            }

            importer(file.path, file.data, file.data_len); 

            golf_file_free_data(&file);
        }
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

    {
        golf_data_file_t *loaded_file = map_get(&_golf_data.loaded_files, path);
        if (loaded_file) {
            loaded_file->load_count++;
            return;
        }
    }

    golf_file_t file = golf_file(path);
    golf_file_t file_to_load = file;    

    golf_data_importer_t importer = _golf_data_get_ext_importer(file.ext);
    if (importer) {
        file_to_load = golf_file_append_extension(path, ".import");
    }

    if (golf_file_load_data(&file_to_load)) {
        golf_data_loader_t loader = _golf_data_get_ext_loader(file.ext);
        if (loader) {
            golf_data_file_t loaded_file;
            loaded_file.load_count = 1;
            loaded_file.file = file_to_load;
            golf_file_get_time(&file_to_load, &loaded_file.last_load_time);
            loader(path, file_to_load.data, file_to_load.data_len, &loaded_file);
            map_set(&_golf_data.loaded_files, path, loaded_file);
        }
        else {
            golf_log_warning("Trying to load file without a loader %s", path);
        }
        golf_file_free_data(&file_to_load);
    }
    else {
        golf_log_warning("Unable to load file %s", file_to_load.path);
    }
}

void golf_data_unload_file(const char *path) {
}

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "3rd_party/cimgui/cimgui.h"

void golf_data_debug_console_tab(void) {
    {
        golf_data_allocator_section_t *section = _golf_data_allocator_get_section();
        igText("Bytes Allocated: %d", section->bytes_allocated);
    }

    if (igCollapsingHeaderTreeNodeFlags("Textures", ImGuiTreeNodeFlags_None)) {
        const char *key;
        map_iter_t iter = map_iter(&_golf_data.loaded_files);

        while ((key = map_next(&_golf_data.loaded_files, &iter))) {
            golf_data_file_t *loaded_file = map_get(&_golf_data.loaded_files, key);
            golf_data_texture_t *texture = loaded_file->texture;
            if (loaded_file->type == GOLF_DATA_TEXTURE) {
                if (igTreeNodeStr(key)) {
                    igText("Width: %d", texture->width);
                    igText("Height: %d", texture->height);
                    igImage((ImTextureID)(intptr_t)texture->image.id, (ImVec2){texture->width, texture->height}, (ImVec2){0, 0}, (ImVec2){1, 1}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 1});
                    igTreePop();
                }
            }
        }
    }
}
