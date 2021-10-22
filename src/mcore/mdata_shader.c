#include "mcore/mdata_shader.h"

#include "mcore/mdata.h"
#include "mcore/mfile.h"
#include "mcore/mlog.h"
#include "mcore/mparson.h"
#include "mcore/mstring.h"

static JSON_Value *_shader_import_bare(const char *base_name, const char *name) {
    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    {
        mstring_t fs_bare_name;
        mstring_initf(&fs_bare_name, "%s_%s_fs.glsl", base_name, name);
        mfile_t fs_file = mfile(fs_bare_name.cstr);
        if (!mfile_load_data(&fs_file)) {
            mlog_error("Failed to read file %s", fs_bare_name.cstr);
        }
        json_object_set_string(obj, "fs", fs_file.data);
        mfile_free_data(&fs_file);
        mstring_deinit(&fs_bare_name);
    }

    {
        mstring_t vs_bare_name;
        mstring_initf(&vs_bare_name, "%s_%s_vs.glsl", base_name, name);
        mfile_t vs_file = mfile(vs_bare_name.cstr);
        if (!mfile_load_data(&vs_file)) {
            mlog_error("Failed to read file %s", vs_bare_name.cstr);
        }
        json_object_set_string(obj, "vs", vs_file.data);
        mfile_free_data(&vs_file);
        mstring_deinit(&vs_bare_name);
    }

    return val;
}

bool mdata_shader_import(const char *path, char *data, int data_len) {
    mfile_t file = mfile(path);
    static const char *slangs = "glsl330:glsl300es";
    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    {
        mstring_t cmd;
#if MARS_PLATFORM_LINUX
        mstring_initf(&cmd, "tools/sokol-tools/linux/sokol-shdc --input %s --output src/golf2/shaders/%s.h --slang %s", file.path, file.name, slangs);
#elif MARS_PLATFORM_WINDOWS
        mstring_initf(&cmd, "tools\\sokol-tools\\win32\\sokol-shdc --input %s --output src/golf2/shaders/%s.h --slang %s", file.path, file.name, slangs);
#elif MARS_PLATFORM_MACOS
#endif
        int ret = system(cmd.cstr);
        mstring_deinit(&cmd);
    }

    {
        mstring_t cmd;
#if MARS_PLATFORM_LINUX
        mstring_initf(&cmd, "tools/sokol-tools/linux/sokol-shdc --input %s --output out/temp/bare --slang %s --format bare", file.path, slangs);
#elif MARS_PLATFORM_WINDOWS
        mstring_initf(&cmd, "tools\\sokol-tools\\win32\\sokol-shdc --input %s --output out/temp/bare --slang %s --format bare", file.path, slangs);
#elif MARS_PLATFORM_MACOS
#endif
        int ret = system(cmd.cstr);
        if (ret == 0) {
            mstring_t base_bare_name;
            mstring_initf(&base_bare_name, "out/temp/bare_%s", file.name);
            mstring_pop(&base_bare_name, 5);

            {
                JSON_Value *bare_val = _shader_import_bare(base_bare_name.cstr, "glsl300es");
                json_object_set_value(obj, "glsl300es", bare_val);
            }

            {
                JSON_Value *bare_val = _shader_import_bare(base_bare_name.cstr, "glsl330");
                json_object_set_value(obj, "glsl330", bare_val);
            }

            mstring_deinit(&base_bare_name);
        }
        mstring_deinit(&cmd);
    }

    mstring_t import_shader_file_path;
    mstring_initf(&import_shader_file_path, "%s.import", file.path);
    json_serialize_to_file_pretty(val, import_shader_file_path.cstr);
    mstring_deinit(&import_shader_file_path);

    json_value_free(val);
    return true;
}
