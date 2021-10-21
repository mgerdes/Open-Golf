#include "mcore/mdata.h"

#include "3rd_party/fast_obj/fast_obj.h"
#include "3rd_party/parson/parson.h"
#include "3rd_party/stb/stb_image_write.h"
#include "3rd_party/stb/stb_truetype.h"
#include "mcore/mcommon.h"
#include "mcore/mfile.h"
#include "mcore/mlog.h"
#include "mcore/mstring.h"

static void _mdata_json_object_get_data(JSON_Object *obj, const char *name, unsigned char **data, int *data_len) {
    const char *data_base64 = json_object_get_string(obj, name); 
    *data = mbase64_decode(data_base64, strlen(data_base64), data_len);
}

static void _mdata_json_object_set_data(JSON_Object *obj, const char *name, unsigned char *data, int data_len) {
    char *enc = mbase64_encode(data, data_len); 
    json_object_set_string(obj, name, enc);
    free(enc);
}

static vec2 _mdata_json_object_get_vec2(JSON_Object *obj, const char *name) {
    JSON_Array *array = json_object_get_array(obj, name);
    vec2 v;
    v.x = json_array_get_number(array, 0);
    v.y = json_array_get_number(array, 1);
    return v;
}

static vec3 _mdata_json_object_get_vec3(JSON_Object *obj, const char *name) {
    JSON_Array *array = json_object_get_array(obj, name);
    vec3 v;
    v.x = json_array_get_number(array, 0);
    v.y = json_array_get_number(array, 1);
    v.z = json_array_get_number(array, 2);
    return v;
}

static vec4 _mdata_json_object_get_vec4(JSON_Object *obj, const char *name) {
    JSON_Array *array = json_object_get_array(obj, name);
    vec4 v;
    v.x = json_array_get_number(array, 0);
    v.y = json_array_get_number(array, 1);
    v.z = json_array_get_number(array, 2);
    v.w = json_array_get_number(array, 3);
    return v;
}

static mfile_t _get_mdata_file(const char *path) {
    mfile_t mdata_file;
    {
        mstring_t mdata_file_path;
        mstring_initf(&mdata_file_path, "%s.mdata", path);
        mdata_file = mfile(mdata_file_path.cstr);
        mstring_deinit(&mdata_file_path);
    }
    return mdata_file;
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
    mstring_initf(&mdata_texture_file_path, "%s.mdata", file->path);
    json_serialize_to_file_pretty(val, mdata_texture_file_path.cstr);
    mstring_deinit(&mdata_texture_file_path);
    json_value_free(val);
}

mdata_texture_t *mdata_texture_load(const char *path) {
    mfile_t mdata_file = _get_mdata_file(path);

    if (!mfile_load_data(&mdata_file)) {
        return NULL;
    }

    JSON_Value *val = json_parse_string(mdata_file.data);
    JSON_Object *obj = json_value_get_object(val);
    if (!val) {
        mlog_warning("Unable to parse json for mdatafile %s", path);
        return NULL;
    }

    mdata_texture_t *data = malloc(sizeof(mdata_texture_t));
    data->json_val = val;
    data->filter = json_object_get_string(obj, "filter");
    _mdata_json_object_get_data(obj, "data", &data->data, &data->data_len);
    return data;
}

void mdata_texture_free(mdata_texture_t *data) {
    json_value_free((JSON_Value*)data->json_val);
    free(data->data);
    free(data);
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

void mdata_shader_import(mfile_t *file) {
    static const char *slangs = "glsl330:glsl300es";
    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    {
        mstring_t cmd;
#if MARS_PLATFORM_LINUX
        mstring_initf(&cmd, "tools/sokol-tools/linux/sokol-shdc --input %s --output src/golf2/shaders/%s.h --slang %s", file->path, file->name, slangs);
#elif MARS_PLATFORM_WINDOWS
        mstring_initf(&cmd, "tools\\sokol-tools\\win32\\sokol-shdc --input %s --output src/golf2/shaders/%s.h --slang %s", file->path, file->name, slangs);
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
        mstring_initf(&cmd, "tools\\sokol-tools\\win32\\sokol-shdc --input %s --output out/temp/bare --slang %s --format bare", file->path, slangs);
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
    mstring_initf(&mdata_shader_file_path, "%s.mdata", file->path);
    json_serialize_to_file_pretty(val, mdata_shader_file_path.cstr);
    mstring_deinit(&mdata_shader_file_path);

    json_value_free(val);
}

mdata_shader_t *mdata_shader_load(const char *path) {
    mfile_t mdata_file = _get_mdata_file(path);

    if (!mfile_load_data(&mdata_file)) {
        return NULL;
    }

    JSON_Value *val = json_parse_string(mdata_file.data);
    JSON_Object *obj = json_value_get_object(val);
    if (!val) {
        mlog_warning("Unable to parse json for mdatafile %s", path);
        return NULL;
    }

    mdata_shader_t *data = malloc(sizeof(mdata_shader_t));
    data->json_val = val;
    data->glsl300es.fs = json_object_dotget_string(obj, "glsl300es.fs");
    data->glsl300es.vs = json_object_dotget_string(obj, "glsl300es.vs");
    data->glsl330.fs = json_object_dotget_string(obj, "glsl330.fs");
    data->glsl330.vs = json_object_dotget_string(obj, "glsl330.vs");
    return data;
}

void mdata_shader_free(mdata_shader_t *data) {
    json_value_free(data->json_val);
    free(data);
}

//
// CONFIG
//

void mdata_config_import(mfile_t *file) {
}

mdata_config_t *mdata_config_load(const char *path) {
    mfile_t mdata_file = _get_mdata_file(path);

    if (!mfile_load_data(&mdata_file)) {
        return NULL;
    }

    JSON_Value *val = json_parse_string(mdata_file.data);
    JSON_Object *obj = json_value_get_object(val);
    if (!val) {
        mlog_warning("Unable to parse json for mdatafile %s", path);
        return NULL;
    }

    mdata_config_t *data = malloc(sizeof(mdata_config_t));
    vec_init(&data->properties);

    data->json_val = val;

    JSON_Array *props_array = json_object_get_array(obj, "properties");
    for (int i = 0; i < json_array_get_count(props_array); i++) {
        JSON_Object *prop_obj = json_array_get_object(props_array, i);
        mdata_config_property_t prop;
        prop.name = json_object_get_string(prop_obj, "name");
        if (!prop.name) {
            mlog_warning("No name found in config property");
            continue;
        }

        bool valid_type = false;
        JSON_Value *val = json_object_get_value(prop_obj, "val");
        JSON_Value_Type val_type = json_value_get_type(val);
        if (val_type == JSONString) {
            valid_type = true;
            prop.type = MDATA_CONFIG_PROPERTY_STRING;
            prop.string_val = json_value_get_string(val);
        }
        else if (val_type == JSONNumber) {
            valid_type = true;
            prop.type = MDATA_CONFIG_PROPERTY_NUMBER;
            prop.number_val = json_value_get_number(val);
        }
        else if (val_type == JSONArray) {
            JSON_Array *array = json_value_get_array(val);
            if (json_array_get_count(array) == 2) {
                valid_type = true;
                prop.type = MDATA_CONFIG_PROPERTY_VEC2;
                prop.vec2_val = _mdata_json_object_get_vec2(prop_obj, "val");
            }
            else if (json_array_get_count(array) == 3) {
                valid_type = true;
                prop.type = MDATA_CONFIG_PROPERTY_VEC3;
                prop.vec3_val = _mdata_json_object_get_vec3(prop_obj, "val");
            }
            else if (json_array_get_count(array) == 4) {
                valid_type = true;
                prop.type = MDATA_CONFIG_PROPERTY_VEC4;
                prop.vec4_val = _mdata_json_object_get_vec4(prop_obj, "val");
            }
        }

        if (valid_type) {
            vec_push(&data->properties, prop);
        }
        else {
            mlog_warning("Invalid type found for property %s", prop.name);
        }
    }

    return data;
}

void mdata_config_free(mdata_config_t *data) {
}

//
// FONT
//

static void _stbi_write_func(void *context, void *data, int size) {
    vec_char_t *bmp = (vec_char_t*)context;
    vec_pusharr(bmp, (char*)data, size);
}

static JSON_Value *_mdata_font_atlas_import(mfile_t *file, int font_size, int bitmap_size) {
    unsigned char *bitmap = malloc(bitmap_size * bitmap_size);
    stbtt_bakedchar cdata[96];
    memset(cdata, 0, sizeof(cdata));
    stbtt_BakeFontBitmap(file->data, 0, -font_size, bitmap, bitmap_size, bitmap_size, 32, 95, cdata);

    float ascent, descent, linegap;
    stbtt_GetScaledFontVMetrics(file->data, 0, -font_size, &ascent, &descent, &linegap);

    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);
    json_object_set_number(obj, "font_size", font_size);
    json_object_set_number(obj, "ascent", ascent);
    json_object_set_number(obj, "descent", descent);
    json_object_set_number(obj, "linegap", linegap);

    JSON_Value *char_datas_val = json_value_init_array();
    JSON_Array *char_datas_array = json_value_get_array(char_datas_val);
    for (int i = 0; i < 96; i++) {
        JSON_Value *char_data_val = json_value_init_object();
        JSON_Object *char_data_obj = json_value_get_object(char_data_val);
        json_object_set_number(char_data_obj, "c", 32 + i);
        json_object_set_number(char_data_obj, "x0", cdata[i].x0);
        json_object_set_number(char_data_obj, "x1", cdata[i].x1);
        json_object_set_number(char_data_obj, "y0", cdata[i].y0);
        json_object_set_number(char_data_obj, "y1", cdata[i].y1);
        json_object_set_number(char_data_obj, "xoff", cdata[i].xoff);
        json_object_set_number(char_data_obj, "yoff", cdata[i].yoff);
        json_object_set_number(char_data_obj, "xadvance", cdata[i].xadvance);
        json_array_append_value(char_datas_array, char_data_val);
    }
    json_object_set_value(obj, "char_datas", char_datas_val);

    json_object_set_number(obj, "bitmap_size", bitmap_size);
    {
        vec_char_t bmp;
        vec_init(&bmp);
        stbi_write_bmp_to_func(_stbi_write_func, &bmp, bitmap_size, bitmap_size, 1, bitmap);
        _mdata_json_object_set_data(obj, "bitmap_data", bmp.data, bmp.length);
        vec_deinit(&bmp);
    }

    free(bitmap);
    return val;
}

void mdata_font_import(mfile_t *file) {
    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    JSON_Value *atlases_val = json_value_init_array();
    JSON_Array *atlases_array = json_value_get_array(atlases_val);

    JSON_Value *small_atlas_val = _mdata_font_atlas_import(file, 16, 256);
    json_array_append_value(atlases_array, small_atlas_val);

    JSON_Value *medium_atlas_val = _mdata_font_atlas_import(file, 32, 256);
    json_array_append_value(atlases_array, medium_atlas_val);

    JSON_Value *large_atlas_val = _mdata_font_atlas_import(file, 64, 512);
    json_array_append_value(atlases_array, large_atlas_val);

    json_object_set_value(obj, "atlases", atlases_val);

    mstring_t mdata_font_file_path;
    mstring_initf(&mdata_font_file_path, "%s.mdata", file->path);
    json_serialize_to_file(val, mdata_font_file_path.cstr);
    mstring_deinit(&mdata_font_file_path);

    json_value_free(val);
}

static void _mdata_json_object_get_font_atlas(JSON_Object *obj, mdata_font_atlas_t *atlas) {
    atlas->font_size = json_object_get_number(obj, "font_size");
    atlas->ascent = json_object_get_number(obj, "ascent");
    atlas->descent = json_object_get_number(obj, "descent");
    atlas->linegap = json_object_get_number(obj, "linegap");
    atlas->bmp_size = json_object_get_number(obj, "bitmap_size");
    _mdata_json_object_get_data(obj, "bitmap_data", &atlas->bmp_data, &atlas->bmp_data_len);

    JSON_Array *char_datas_array = json_object_get_array(obj, "char_datas");
    for (int i = 0; i < json_array_get_count(char_datas_array); i++) {
        JSON_Object *char_data_obj = json_array_get_object(char_datas_array, i);
        int c = json_object_get_number(char_data_obj, "c");
        if (c >= 0 && c < 256) {
            atlas->char_data[c].x0 = json_object_get_number(char_data_obj, "x0");
            atlas->char_data[c].x1 = json_object_get_number(char_data_obj, "x1");
            atlas->char_data[c].y0 = json_object_get_number(char_data_obj, "y0");
            atlas->char_data[c].y1 = json_object_get_number(char_data_obj, "y1");
            atlas->char_data[c].xoff = json_object_get_number(char_data_obj, "xoff");
            atlas->char_data[c].yoff = json_object_get_number(char_data_obj, "yoff");
            atlas->char_data[c].xadvance = json_object_get_number(char_data_obj, "xadvance");
        }
    }
}

mdata_font_t *mdata_font_load(const char *path) {
    mfile_t mdata_file = _get_mdata_file(path);

    if (!mfile_load_data(&mdata_file)) {
        return NULL;
    }

    JSON_Value *val = json_parse_string(mdata_file.data);
    JSON_Object *obj = json_value_get_object(val);
    if (!val) {
        mlog_warning("Unable to parse json for mdatafile %s", path);
        return NULL;
    }

    JSON_Array *atlases_array = json_object_get_array(obj, "atlases");
    mdata_font_t *data = malloc(sizeof(mdata_font_t));
    data->json_val = val;
    for (int i = 0; i < json_array_get_count(atlases_array); i++) {
        JSON_Object *atlas_obj = json_array_get_object(atlases_array, i);
        _mdata_json_object_get_font_atlas(atlas_obj, &data->atlases[i]);
    }
    return data;
}

void mdata_font_free(mdata_font_t *data) {
    json_value_free(data->json_val);
    for (int i = 0; i < 3; i++) {
        free(data->atlases[i].bmp_data);
    }
    free(data);
}

//
// MODEL
//

void mdata_model_import(mfile_t *file) {
    JSON_Value *vertices_val = json_value_init_array();
    JSON_Array *vertices = json_value_get_array(vertices_val);

    fastObjMesh *m = fast_obj_read(file->path);
    for (int i = 0; i < m->group_count; i++) {
        const fastObjGroup grp = m->groups[i];

        int idx = 0;
        for (int j = 0; j < grp.face_count; j++) {
            int fv = m->face_vertices[grp.face_offset + j];
            if (fv != 3) {
                mlog_warning("OBJ file isn't triangulated %s", file->path); 
            }

            for (int k = 0; k < fv; k++) {
                fastObjIndex mi = m->indices[grp.index_offset + idx];

                vec3 p;
                p.x = m->positions[3 * mi.p + 0];
                p.y = m->positions[3 * mi.p + 1];
                p.z = m->positions[3 * mi.p + 2];

                vec2 t;
                t.x = m->texcoords[2 * mi.t + 0];
                t.y = m->texcoords[2 * mi.t + 1];

                vec3 n;
                n.x = m->normals[3 * mi.n + 0];
                n.y = m->normals[3 * mi.n + 1];
                n.z = m->normals[3 * mi.n + 2];

                JSON_Value *position_array_val = json_value_init_array();
                JSON_Array *position_array = json_value_get_array(position_array_val);
                json_array_append_number(position_array, p.x);
                json_array_append_number(position_array, p.y);
                json_array_append_number(position_array, p.z);

                JSON_Value *texcoord_array_val = json_value_init_array();
                JSON_Array *texcoord_array = json_value_get_array(texcoord_array_val);
                json_array_append_number(texcoord_array, t.x);
                json_array_append_number(texcoord_array, t.y);

                JSON_Value *normal_array_val = json_value_init_array();
                JSON_Array *normal_array = json_value_get_array(normal_array_val);
                json_array_append_number(normal_array, n.x);
                json_array_append_number(normal_array, n.y);
                json_array_append_number(normal_array, n.z);

                JSON_Value *face_val = json_value_init_object();
                JSON_Object *face = json_value_get_object(face_val);
                json_object_set_value(face, "position", position_array_val);
                json_object_set_value(face, "texcoord", texcoord_array_val);
                json_object_set_value(face, "normal", normal_array_val);

                json_array_append_value(vertices, face_val);

                idx++;
            }
        }
    }
    fast_obj_destroy(m);

    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);
    json_object_set_value(obj, "vertices", vertices_val);

    mstring_t mdata_model_file_path;
    mstring_initf(&mdata_model_file_path, "%s.mdata", file->path);
    json_serialize_to_file(val, mdata_model_file_path.cstr);
    mstring_deinit(&mdata_model_file_path);
    json_value_free(val);
}

mdata_model_t *mdata_model_load(const char *path) {
    mfile_t mdata_file = _get_mdata_file(path);

    if (!mfile_load_data(&mdata_file)) {
        return NULL;
    }

    JSON_Value *val = json_parse_string(mdata_file.data);
    JSON_Object *obj = json_value_get_object(val);
    if (!val) {
        mlog_warning("Unable to parse json for mdatafile %s", path);
        return NULL;
    }

    mdata_model_t *data = malloc(sizeof(mdata_model_t));
    data->json_val = val;
    vec_init(&data->positions);
    vec_init(&data->texcoords);
    vec_init(&data->normals);

    JSON_Value *vertices_val = json_object_get_value(obj, "vertices");
    JSON_Array *vertices = json_value_get_array(vertices_val);
    for (int i = 0; i < json_array_get_count(vertices); i++) {
        JSON_Object *vertex_obj = json_array_get_object(vertices, i);
        JSON_Array *position_array = json_object_get_array(vertex_obj, "position");
        JSON_Array *texturecoord_array = json_object_get_array(vertex_obj, "texcoord");
        JSON_Array *normal_array = json_object_get_array(vertex_obj, "normal");

        vec3 p = V3(json_array_get_number(position_array, 0), 
                json_array_get_number(position_array, 1),
                json_array_get_number(position_array, 2));
        vec2 tc = V2(json_array_get_number(texturecoord_array, 0), 
                json_array_get_number(texturecoord_array, 1));
        vec3 n = V3(json_array_get_number(normal_array, 0), 
                json_array_get_number(normal_array, 1),
                json_array_get_number(normal_array, 2));

        vec_push(&data->positions, p);
        vec_push(&data->texcoords, tc);
        vec_push(&data->normals, n);
    }

    return data;
}

void mdata_model_free(mdata_model_t *data) {
    json_value_free(data->json_val);
    vec_deinit(&data->positions);
    vec_deinit(&data->texcoords);
    vec_deinit(&data->normals);
    free(data);
}

//
// UI PIXEL PACK
//

void mdata_ui_pixel_pack_import(mfile_t *file) {
}

mdata_ui_pixel_pack_t *mdata_ui_pixel_pack_load(const char *path) {
    mfile_t mdata_file = _get_mdata_file(path);

    if (!mfile_load_data(&mdata_file)) {
        return NULL;
    }

    JSON_Value *val = json_parse_string(mdata_file.data);
    JSON_Object *obj = json_value_get_object(val);
    if (!val) {
        mlog_warning("Unable to parse json for mdatafile %s", path);
        return NULL;
    }

    mdata_ui_pixel_pack_t *data = malloc(sizeof(mdata_ui_pixel_pack_t));
    vec_init(&data->squares);
    vec_init(&data->icons);

    data->json_val = val;
    data->texture = json_object_get_string(obj, "texture");
    data->tile_size = json_object_get_number(obj, "tile_size");
    data->tile_padding = json_object_get_number(obj, "tile_padding");

    JSON_Array *icons_array = json_object_get_array(obj, "icons");
    for (int i = 0; i < json_array_get_count(icons_array); i++) {
        JSON_Object *icon_obj = json_array_get_object(icons_array, i);
        mdata_ui_pixel_pack_icon_t icon;
        icon.name = json_object_get_string(icon_obj, "name");
        icon.x = json_object_get_number(icon_obj, "x");
        icon.y = json_object_get_number(icon_obj, "y");
        vec_push(&data->icons, icon);
    }

    JSON_Array *squares_array = json_object_get_array(obj, "squares");
    for (int i = 0; i < json_array_get_count(squares_array); i++) {
        JSON_Object *square_obj = json_array_get_object(squares_array, i);
        mdata_ui_pixel_pack_square_t square;
        square.name = json_object_get_string(square_obj, "name");
        square.tl = _mdata_json_object_get_vec2(square_obj, "top_left");
        square.tm = _mdata_json_object_get_vec2(square_obj, "top_mid");
        square.tr = _mdata_json_object_get_vec2(square_obj, "top_right");
        square.ml = _mdata_json_object_get_vec2(square_obj, "mid_left");
        square.mm = _mdata_json_object_get_vec2(square_obj, "mid_mid");
        square.mr = _mdata_json_object_get_vec2(square_obj, "mid_right");
        square.bl = _mdata_json_object_get_vec2(square_obj, "bot_left");
        square.bm = _mdata_json_object_get_vec2(square_obj, "bot_mid");
        square.br = _mdata_json_object_get_vec2(square_obj, "bot_right");
        vec_push(&data->squares, square);
    }

    return data;
}

void mdata_ui_pixel_pack_free(mdata_ui_pixel_pack_t *data) {
}

//
// UI
//

void mdata_ui_import(mfile_t *file) {
}

mdata_ui_t *mdata_ui_load(const char *path) {
    mfile_t mdata_file = _get_mdata_file(path);

    if (!mfile_load_data(&mdata_file)) {
        return NULL;
    }

    JSON_Value *val = json_parse_string(mdata_file.data);
    JSON_Object *obj = json_value_get_object(val); 
    if (!obj) {
        mlog_warning("Unable to parse json for mdatafile %s", path);
        return NULL;
    }

    mdata_ui_t *data = malloc(sizeof(mdata_ui_t));
    vec_init(&data->entities);

    data->json_val = val;

    JSON_Array *entities_array = json_object_get_array(obj, "entities");
    for (int i = 0; i < json_array_get_count(entities_array); i++) {
        JSON_Object *entity_obj = json_array_get_object(entities_array, i);

        const char *type = json_object_get_string(entity_obj, "type");
        if (!type) {
            mlog_warning("No type on entity in UI file");
            continue;
        }

        const char *name = json_object_get_string(entity_obj, "type");
        if (!name) {
            mlog_warning("No name on entity in UI file");
            continue;
        }

        if (strcmp(type, "pixel_pack_square") == 0) {
            const char *pixel_pack = json_object_get_string(entity_obj, "string");
            const char *square_name = json_object_get_string(entity_obj, "font");
            vec2 pos = _mdata_json_object_get_vec2(entity_obj, "pos");
            vec2 size = _mdata_json_object_get_vec2(entity_obj, "size");
            float tile_screen_size = json_object_get_number(entity_obj, "size");

            mdata_ui_entity_pixel_pack_square_t pixel_pack_square;
            pixel_pack_square.pixel_pack = pixel_pack;
            pixel_pack_square.square_name = square_name;
            pixel_pack_square.pos = pos;
            pixel_pack_square.size = size;
            pixel_pack_square.tile_screen_size = tile_screen_size;

            mdata_ui_entity_t entity;
            entity.name = name;
            entity.type = MDATA_UI_ENTITY_PIXEL_PACK_SQUARE;
            entity.pixel_pack_square = pixel_pack_square;
            vec_push(&data->entities, entity);
        }
        else if (strcmp(type, "text") == 0) {
            const char *string = json_object_get_string(entity_obj, "string");
            const char *font = json_object_get_string(entity_obj, "font");
            vec2 pos = _mdata_json_object_get_vec2(entity_obj, "pos");
            float size = json_object_get_number(entity_obj, "size");
            vec4 color = _mdata_json_object_get_vec4(entity_obj, "color");
            const char *horiz_align = json_object_get_string(entity_obj, "horiz-align");
            const char *vert_align = json_object_get_string(entity_obj, "vert-align");

            mdata_ui_entity_text_t text;
            text.string = string;
            text.font = font;
            text.pos = pos;
            text.size = size;
            text.color = color;
            text.horiz_align = horiz_align;
            text.vert_align = vert_align;

            mdata_ui_entity_t entity;
            entity.name = name;
            entity.type = MDATA_UI_ENTITY_TEXT;
            entity.text = text;
            vec_push(&data->entities, entity);
        }
        else {
            mlog_warning("Unknown type in UI file %s", type);
            continue;
        }
    }

    return data;
}

void mdata_ui_free(mdata_ui_t *data) {
}

//
// MDATA
//

static vec_mdata_loader_t _mdata_loaders;
static map_mdata_t _loaded_mdata_files;
static mdir_t _data_dir;

void mdata_init(void) {
    vec_init(&_mdata_loaders);
    map_init(&_loaded_mdata_files);	
    mdir_init(&_data_dir, "data", true);
}

void mdata_run_import(void) {
    for (int i = 0; i < _data_dir.num_files; i++) {
        mfile_t file = _data_dir.files[i];
        if (strcmp(file.ext, ".mdata") == 0) {
            mstring_t base_file_path;
            mstring_initf(&base_file_path, "%s", file.path);
            mstring_pop(&base_file_path, 6);

            mdata_t *loaded_data = map_get(&_loaded_mdata_files, base_file_path.cstr);
            if (loaded_data) {
                mfiletime_t mdata_filetime;
                mfile_get_time(&file, &mdata_filetime);

                int cmp = mfiletime_cmp(loaded_data->last_load_time, mdata_filetime);
                if (mfiletime_cmp(loaded_data->last_load_time, mdata_filetime) < 0) {
                    loaded_data->last_load_time = mdata_filetime;

                    bool load_successful = false;
                    switch (loaded_data->type) {
                        case MDATA_SHADER:
                            mdata_shader_free(loaded_data->shader);
                            loaded_data->shader = mdata_shader_load(base_file_path.cstr);
                            load_successful = loaded_data->shader != NULL;
                            break;
                        case MDATA_TEXTURE:
                            mdata_texture_free(loaded_data->texture);
                            loaded_data->texture = mdata_texture_load(base_file_path.cstr);
                            load_successful = loaded_data->texture != NULL;
                            break;
                        case MDATA_FONT:
                            mdata_font_free(loaded_data->font);
                            loaded_data->font = mdata_font_load(base_file_path.cstr);
                            load_successful = loaded_data->font != NULL;
                            break;
                        case MDATA_MODEL:
                            mdata_model_free(loaded_data->model);
                            loaded_data->model = mdata_model_load(base_file_path.cstr);
                            load_successful = loaded_data->model != NULL;
                            break;
                        case MDATA_CONFIG:
                            mdata_config_free(loaded_data->config);
                            loaded_data->config = mdata_config_load(base_file_path.cstr);
                            load_successful = loaded_data->config != NULL;
                            break;
                        case MDATA_UI_PIXEL_PACK:
                            mdata_ui_pixel_pack_free(loaded_data->ui_pixel_pack);
                            loaded_data->ui_pixel_pack = mdata_ui_pixel_pack_load(base_file_path.cstr);
                            load_successful = loaded_data->ui_pixel_pack != NULL;
                            break;
                        case MDATA_UI:
                            mdata_ui_free(loaded_data->ui);
                            loaded_data->ui = mdata_ui_load(base_file_path.cstr);
                            load_successful = loaded_data->ui != NULL;
                            break;
                    }

                    if (load_successful) {
                        for (int i = 0; i < _mdata_loaders.length; i++) {
                            mdata_loader_t loader = _mdata_loaders.data[i];
                            if (loader.type == loaded_data->type) {
                                loader.reload(base_file_path.cstr, *loaded_data);
                            }
                        }
                    }
                }
            }

            mstring_deinit(&base_file_path);
        }
        else { 
            if (!mfile_load_data(&file)) {
                mlog_warning("Failed to load data for file %s", file.path);
                continue;
            }

            mfiletime_t file_time;
            mfile_get_time(&file, &file_time);

            mfile_t mdata_file = _get_mdata_file(file.path);
            mfiletime_t mdata_file_time;
            mfile_get_time(&mdata_file, &mdata_file_time);

            if (mfiletime_cmp(file_time, mdata_file_time) < 0) {
                continue;
            }

            void (*importer)(mfile_t*) = NULL;

            if ((strcmp(file.ext, ".glsl") == 0)) {
                importer = mdata_shader_import;
            }
            else if ((strcmp(file.ext, ".png") == 0) ||
                    (strcmp(file.ext, ".bmp") == 0) ||
                    (strcmp(file.ext, ".jpg") == 0)) {
                importer = mdata_texture_import;
            }
            else if ((strcmp(file.ext, ".ttf") == 0)) {
                importer = mdata_font_import;
            }
            else if ((strcmp(file.ext, ".obj") == 0)) {
                importer = mdata_model_import;
            }

            if (importer) {
                mlog_note("Importing %s", file.path);
                importer(&file);
            }
        }
    }
}

void mdata_add_loader(mdata_type_t type, bool(*load)(const char *path, mdata_t data), bool(*unload)(const char *path, mdata_t data), bool(*reload)(const char *path, mdata_t data)) {
    mdata_loader_t loader;
    loader.type = type;
    loader.load = load;
    loader.unload = unload;
    loader.reload = reload;
    vec_push(&_mdata_loaders, loader);
}

void mdata_load_file(const char *path) {
    mdata_t *loaded_data = map_get(&_loaded_mdata_files, path);
    if (loaded_data) {
        loaded_data->load_count++;
    }
    else {
        mdata_t data;
        data.load_count = 1;
        mfile_t file = mfile(path);
        bool load_successful = false;
        if ((strcmp(file.ext, ".glsl") == 0)) {
            data.type = MDATA_SHADER;
            data.shader = mdata_shader_load(path);
            load_successful = data.shader != NULL;
            if (!data.shader) {
                mlog_warning("Unable to load shader file %s", file.path);
            }
        }
        else if ((strcmp(file.ext, ".png") == 0) ||
                (strcmp(file.ext, ".bmp") == 0) ||
                (strcmp(file.ext, ".jpg") == 0)) {
            data.type = MDATA_TEXTURE;
            data.texture = mdata_texture_load(path);
            load_successful = data.texture != NULL;
            if (!data.texture) {
                mlog_warning("Unable to load texture file %s", file.path);
            }
        }
        else if ((strcmp(file.ext, ".ttf") == 0)) {
            data.type = MDATA_FONT;
            data.font = mdata_font_load(path);
            load_successful = data.font != NULL;
            if (!data.font) {
                mlog_warning("Unable to load font file %s", file.path);
            }
        }
        else if ((strcmp(file.ext, ".obj") == 0)) {
            data.type = MDATA_MODEL;
            data.model = mdata_model_load(path);
            load_successful = data.model != NULL;
            if (!data.model) {
                mlog_warning("Unable to load model file %s", file.path);
            }
        }
        else if ((strcmp(file.ext, ".ui_pixel_pack") == 0)) {
            data.type = MDATA_UI_PIXEL_PACK;
            data.ui_pixel_pack = mdata_ui_pixel_pack_load(path);
            load_successful = data.ui_pixel_pack != NULL;
            if (!data.ui_pixel_pack) {
                mlog_warning("Unable to load ui pixel pack file %s", file.path);
            }
        }
        else if ((strcmp(file.ext, ".cfg") == 0)) {
            data.type = MDATA_CONFIG;
            data.config = mdata_config_load(path);
            load_successful = data.config != NULL;
            if (!data.config) {
                mlog_warning("Unable to load config file %s", file.path);
            }
        }
        else if ((strcmp(file.ext, ".ui") == 0)) {
            data.type = MDATA_UI;
            data.ui = mdata_ui_load(path);
            load_successful = data.ui != NULL;
            if (!data.ui) {
                mlog_warning("Unable to load ui file %s", file.path);
            }
        }
        else {
            mlog_warning("Unknown file ext %s", file.ext);
            return;
        }

        mfile_t mdata_file = _get_mdata_file(path);
        mfile_get_time(&mdata_file, &data.last_load_time);
        if (load_successful) {
            for (int i = 0; i < _mdata_loaders.length; i++) {
                mdata_loader_t loader = _mdata_loaders.data[i];
                if (loader.type == data.type) {
                    loader.load(file.path, data);
                }
            }
        }
        map_set(&_loaded_mdata_files, path, data);
    }
}

void mdata_unload_file(const char *path) {
    mdata_t *loaded_data = map_get(&_loaded_mdata_files, path);
    if (loaded_data) {
        loaded_data->load_count--;
        if (loaded_data->load_count == 0) {
            switch (loaded_data->type) {
                case MDATA_SHADER:
                    mdata_shader_free(loaded_data->shader);
                    break;
                case MDATA_TEXTURE:
                    mdata_texture_free(loaded_data->texture);
                    break;
                case MDATA_FONT:
                    mdata_font_free(loaded_data->font);
                    break;
                case MDATA_MODEL:
                    mdata_model_free(loaded_data->model);
                    break;
                case MDATA_CONFIG:
                    mdata_config_free(loaded_data->config);
                    break;
                case MDATA_UI_PIXEL_PACK:
                    mdata_ui_pixel_pack_free(loaded_data->ui_pixel_pack);
                    break;
                case MDATA_UI:
                    mdata_ui_free(loaded_data->ui);
                    break;
            }

            for (int i = 0; i < _mdata_loaders.length; i++) {
                mdata_loader_t loader = _mdata_loaders.data[i];
                if (loader.type == loaded_data->type) {
                    loader.unload(path, *loaded_data);
                }
            }
            map_remove(&_loaded_mdata_files, path);
        }
    }
    else {
        mlog_warning("Attempting to unload file that isn't loaded %s", path);
    }
}
