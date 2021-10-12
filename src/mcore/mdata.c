#include "mcore/mdata.h"

#include "3rd_party/parson/parson.h"
#include "3rd_party/stb/stb_image_write.h"
#include "3rd_party/stb/stb_truetype.h"
#include "mcore/mcommon.h"
#include "mcore/mfile.h"
#include "mcore/mlog.h"
#include "mcore/mstring.h"

static void _mdata_json_object_set_data(JSON_Object *obj, const char *name, unsigned char *data, int data_len) {
    char *enc = mbase64_encode(data, data_len); 
    json_object_set_string(obj, name, enc);
    free(enc);
}

//
// TEXTURE
//

void mdata_texture_import(mfile_t *file) {
    mdata_texture_t *existing_data = mdata_texture_load(file->path);

    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    json_object_set_string(obj, "filter", "linear");
    _mdata_json_object_set_data(obj, "data", file->data, file->data_len);

    if (existing_data) {
        json_object_set_string(obj, "filter", existing_data->filter);
        mdata_texture_free(existing_data);
    }

    mstring_t mdata_texture_file_path;
    mstring_initf(&mdata_texture_file_path, "%s.mdata_texture", file->path);
    json_serialize_to_file_pretty(val, mdata_texture_file_path.cstr);
    mstring_deinit(&mdata_texture_file_path);

    json_value_free(val);
}

mdata_texture_t *mdata_texture_load(const char *path) {
    return NULL;
}

void mdata_texture_free(mdata_texture_t *data) {
}

//
// SHADER
//

static JSON_Value *_mdata_shader_import_bare(const char *base_name, const char *name) {
    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    {
        mstring_t fs_bare_name;
        mstring_initf(&fs_bare_name, "%s_%s_fs.glsl", base_name, name);
        unsigned char *fs_data;
        int fs_data_len;
        if (!mread_file(fs_bare_name.cstr, &fs_data, &fs_data_len)) {
            mlog_error("Failed to read file %s", fs_bare_name.cstr);
        }
        _mdata_json_object_set_data(obj, "fs", fs_data, fs_data_len);
        free(fs_data);
        mstring_deinit(&fs_bare_name);
    }

    {
        mstring_t vs_bare_name;
        mstring_initf(&vs_bare_name, "%s_%s_vs.glsl", base_name, name);
        unsigned char *vs_data;
        int vs_data_len;
        if (!mread_file(vs_bare_name.cstr, &vs_data, &vs_data_len)) {
            mlog_error("Failed to read file %s", vs_bare_name.cstr);
        }
        _mdata_json_object_set_data(obj, "vs", vs_data, vs_data_len);
        free(vs_data);
        mstring_deinit(&vs_bare_name);
    }

    return val;
}

void mdata_shader_import(mfile_t *file) {
    static const char *slangs = "glsl330:glsl300es";
    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    {
        mstring_t cmd;
#if MARS_PLATFORM_LINUX
        mstring_initf(&cmd, "tools/sokol-tools/linux/sokol-shdc --input %s --output src/golf2/shaders/%s.h --slang %s", file->path, file->name, slangs);
#elif MARS_PLATFORM_WINDOWS
#elif MARS_PLATFORM_MACOS
#endif
        int ret = system(cmd.cstr);
        mstring_deinit(&cmd);
    }

    {
        mstring_t cmd;
#if MARS_PLATFORM_LINUX
        mstring_initf(&cmd, "tools/sokol-tools/linux/sokol-shdc --input %s --output out/temp/bare --slang %s --format bare", file->path, slangs);
#elif MARS_PLATFORM_WINDOWS
#elif MARS_PLATFORM_MACOS
#endif
        int ret = system(cmd.cstr);
        if (ret == 0) {
            mstring_t base_bare_name;
            mstring_initf(&base_bare_name, "out/temp/bare_%s", file->name);
            mstring_pop(&base_bare_name, 5);

            {
                JSON_Value *bare_val = _mdata_shader_import_bare(base_bare_name.cstr, "glsl300es");
                json_object_set_value(obj, "glsl300es", bare_val);
            }

            {
                JSON_Value *bare_val = _mdata_shader_import_bare(base_bare_name.cstr, "glsl330");
                json_object_set_value(obj, "glsl330", bare_val);
            }

            mstring_deinit(&base_bare_name);
        }
        mstring_deinit(&cmd);
    }

    mstring_t mdata_shader_file_path;
    mstring_initf(&mdata_shader_file_path, "%s.mdata_shader", file->path);
    json_serialize_to_file_pretty(val, mdata_shader_file_path.cstr);
    mstring_deinit(&mdata_shader_file_path);

    json_value_free(val);
}

mdata_shader_t *mdata_shader_load(const char *path) {

}

void mdata_shader_free(mdata_shader_t *data) {

}

//
// CONFIG
//

void mdata_config_import(mfile_t *file) {
}

mdata_config_t *mdata_config_load(const char *path) {
    return NULL;
}

void mdata_config_free(mdata_config_t *data) {
}

//
// FONT
//

static JSON_Value *_mdata_font_atlas_import(mfile_t *file, int font_size, int bitmap_size) {
    unsigned char *bitmap = malloc(bitmap_size * bitmap_size);
    stbtt_bakedchar cdata[96];
    stbtt_BakeFontBitmap(file->data, 0, -font_size, bitmap, bitmap_size, bitmap_size, 32, 95, cdata);

    float ascent, descent, linegap;
    stbtt_GetScaledFontVMetrics(file->data, 0, -font_size, &ascent, &descent, &linegap);

    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);
    json_object_set_number(obj, "font_size", font_size);
    json_object_set_number(obj, "ascent", ascent);
    json_object_set_number(obj, "descent", descent);
    json_object_set_number(obj, "linegap", linegap);
    json_object_set_number(obj, "bitmap_size", bitmap_size);
    _mdata_json_object_set_data(obj, "bitmap_data", bitmap, bitmap_size * bitmap_size);
    free(bitmap);
    return val;
}

void mdata_font_import(mfile_t *file) {
    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    JSON_Value *atlases_val = json_value_init_array();
    JSON_Array *atlases_array = json_value_get_array(atlases_val);

    JSON_Value *small_atlas_val = _mdata_font_atlas_import(file, 16, 128);
    json_array_append_value(atlases_array, small_atlas_val);

    JSON_Value *medium_atlas_val = _mdata_font_atlas_import(file, 32, 256);
    json_array_append_value(atlases_array, medium_atlas_val);

    JSON_Value *large_atlas_val = _mdata_font_atlas_import(file, 64, 512);
    json_array_append_value(atlases_array, large_atlas_val);

    json_object_set_value(obj, "atlases", atlases_val);

    mstring_t mdata_font_file_path;
    mstring_initf(&mdata_font_file_path, "%s.mdata_font", file->path);
    json_serialize_to_file_pretty(val, mdata_font_file_path.cstr);
    mstring_deinit(&mdata_font_file_path);

    json_value_free(val);
}

mdata_texture_t *mdata_font_load(const char *path) {
    return NULL;
}

void mdata_font_free(mdata_texture_t *data) {
}
