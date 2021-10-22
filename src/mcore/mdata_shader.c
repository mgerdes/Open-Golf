#include "mcore/mdata_shader.h"

#include "golf/file.h"
#include "golf/log.h"
#include "golf/parson_helper.h"
#include "golf/string.h"
#include "mcore/mdata.h"

static JSON_Value *_shader_import_bare(const char *base_name, const char *name) {
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

bool mdata_shader_import(const char *path, char *data, int data_len) {
    golf_file_t file = golf_file(path);
    static const char *slangs = "glsl330:glsl300es";
    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    {
        golf_string_t cmd;
#if MARS_PLATFORM_LINUX
        golf_string_initf(&cmd, "tools/sokol-tools/linux/sokol-shdc --input %s --output src/golf/shaders/%s.h --slang %s", file.path, file.name, slangs);
#elif MARS_PLATFORM_WINDOWS
        golf_string_initf(&cmd, "tools\\sokol-tools\\win32\\sokol-shdc --input %s --output src/golf/shaders/%s.h --slang %s", file.path, file.name, slangs);
#elif MARS_PLATFORM_MACOS
#endif
        int ret = system(cmd.cstr);
        golf_string_deinit(&cmd);
    }

    {
        golf_string_t cmd;
#if MARS_PLATFORM_LINUX
        golf_string_initf(&cmd, "tools/sokol-tools/linux/sokol-shdc --input %s --output out/temp/bare --slang %s --format bare", file.path, slangs);
#elif MARS_PLATFORM_WINDOWS
        golf_string_initf(&cmd, "tools\\sokol-tools\\win32\\sokol-shdc --input %s --output out/temp/bare --slang %s --format bare", file.path, slangs);
#elif MARS_PLATFORM_MACOS
#endif
        int ret = system(cmd.cstr);
        if (ret == 0) {
            golf_string_t base_bare_name;
            golf_string_initf(&base_bare_name, "out/temp/bare_%s", file.name);
            golf_string_pop(&base_bare_name, 5);

            {
                JSON_Value *bare_val = _shader_import_bare(base_bare_name.cstr, "glsl300es");
                json_object_set_value(obj, "glsl300es", bare_val);
            }

            {
                JSON_Value *bare_val = _shader_import_bare(base_bare_name.cstr, "glsl330");
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
