#define _CRT_SECURE_NO_WARNINGS
#include "golf/data.h"

#include <inttypes.h>

#include "fast_obj/fast_obj.h"
#include "map/map.h"
#include "mattiasgustavsson_libs/assetsys.h"
#include "parson/parson.h"
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#include "stb/stb_truetype.h"
#include "golf/base64.h"
#include "golf/file.h"
#include "golf/level.h"
#include "golf/log.h"
#include "golf/maths.h"
#include "golf/parson_helper.h"
#include "golf/string.h"

#include "golf/shaders/diffuse_color_material.glsl.h"
#include "golf/shaders/environment_material.glsl.h"
#include "golf/shaders/pass_through.glsl.h"
#include "golf/shaders/solid_color_material.glsl.h"
#include "golf/shaders/texture_material.glsl.h"
#include "golf/shaders/ui_sprite.glsl.h"

static map_golf_data_t _loaded_data;
static assetsys_t *_assetsys;

typedef bool (*golf_data_importer_t)(const char *path, char *data, int data_len);
typedef bool (*golf_data_loader_t)(golf_data_t *golf_data, const char *path, char *data, int data_len);
typedef bool (*golf_data_unloader_t)(golf_data_t *golf_data);

static assetsys_error_t _golf_assetsys_file_load(const char *path, char **data, int *data_len) {
    char assetsys_path[GOLF_FILE_MAX_PATH];
    snprintf(assetsys_path, GOLF_FILE_MAX_PATH, "/%s", path);

    assetsys_file_t asset_file;
    assetsys_file(_assetsys, assetsys_path, &asset_file);
    int size = assetsys_file_size(_assetsys, asset_file);
    *data = (char*) malloc(size + 1);
    *data_len = 0;
    assetsys_error_t error = assetsys_file_load(_assetsys, asset_file, data_len, *data, size);
    (*data)[size] = 0;
    return error;
}

//
// TEXTURES
//

static bool _golf_texture_import(const char *path, char *data, int data_len) {
    golf_string_t import_texture_file_path;
    golf_string_initf(&import_texture_file_path, "%s.golf_data", path);

    JSON_Value *existing_val = json_parse_file(import_texture_file_path.cstr);
    JSON_Object *existing_obj = json_value_get_object(existing_val);
    const char *existing_filter = json_object_get_string(existing_obj, "filter");

    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    if (existing_filter) {
        json_object_set_string(obj, "filter", existing_filter);
    }
    else {
        json_object_set_string(obj, "filter", "linear");
    }

    golf_json_object_set_data(obj, "img_data", (unsigned char*)data, data_len);

    json_serialize_to_file_pretty(val, import_texture_file_path.cstr);

    golf_string_deinit(&import_texture_file_path);
    json_value_free(val);
    json_value_free(existing_val);
    return true;
}

static bool _golf_texture_load(golf_data_t *golf_data, const char *path, char *data, int data_len) {
    golf_data->type = GOLF_DATA_TEXTURE;
    golf_data->texture = malloc(sizeof(golf_texture_t));

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
        golf_json_object_get_data(obj, "img_data", &img_data, &img_data_len);

        int x, y, n;
        int force_channels = 4;
        stbi_set_flip_vertically_on_load(0);
        unsigned char *stbi_data = stbi_load_from_memory((unsigned char*) img_data, img_data_len, &x, &y, &n, force_channels);
        if (!stbi_data) {
            golf_log_warning("STB Failed to load image");
        }

        sg_image_desc img_desc = {
            .width = x,
            .height = y,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .min_filter = filter,
            .mag_filter = filter,
            .wrap_u = SG_WRAP_REPEAT,
            .wrap_v = SG_WRAP_REPEAT,
            .data.subimage[0][0] = {
                .ptr = stbi_data,
                .size = x * y * n,
            },
        };

        width = x;
        height = y;
        image = sg_make_image(&img_desc);
        free(img_data);
        free(stbi_data);
    }

    json_value_free(val);

    golf_data->texture->sg_image = image;
    golf_data->texture->width = width;
    golf_data->texture->height = height;
    return true;
}

static bool _golf_texture_unload(golf_data_t *golf_data) {
    sg_destroy_image(golf_data->texture->sg_image);
    free(golf_data->texture);
    return true;
}

//
// SHADERS
//

static JSON_Value *_golf_shader_import_bare(const char *base_name, const char *name) {
    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    {
        golf_string_t fs_bare_name;
        golf_string_initf(&fs_bare_name, "%s_%s_fs.glsl", base_name, name);

        char *data;
        int data_len;
        if (!golf_file_load_data(fs_bare_name.cstr, &data, &data_len)) {
            golf_log_error("Failed to read file %s", fs_bare_name.cstr);
        }
        json_object_set_string(obj, "fs", data);
        free(data);

        golf_string_deinit(&fs_bare_name);
    }

    {
        golf_string_t vs_bare_name;
        golf_string_initf(&vs_bare_name, "%s_%s_vs.glsl", base_name, name);

        char *data;
        int data_len;
        if (!golf_file_load_data(vs_bare_name.cstr, &data, &data_len)) {
            golf_log_error("Failed to read file %s", vs_bare_name.cstr);
        }
        json_object_set_string(obj, "vs", data);
        free(data);

        golf_string_deinit(&vs_bare_name);
    }

    return val;
}

static bool _golf_shader_import(const char *path, char *data, int data_len) {
    golf_file_t file = golf_file(path);
    static const char *slangs = "glsl330:glsl300es";
    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    {
        golf_string_t cmd;
        golf_string_init(&cmd, "");
#if GOLF_PLATFORM_LINUX
        golf_string_appendf(&cmd, "tools/sokol-tools/linux/sokol-shdc --input %s --output src/golf/shaders/%s.h --slang %s", file.path, file.name, slangs);
#elif GOLF_PLATFORM_WINDOWS
        golf_string_appendf(&cmd, "tools\\sokol-tools\\win32\\sokol-shdc --input %s --output src/golf/shaders/%s.h --slang %s", file.path, file.name, slangs);
#elif GOLF_PLATFORM_MACOS
#endif
        system(cmd.cstr);
        golf_string_deinit(&cmd);
    }

    {
        golf_string_t cmd;
        golf_string_init(&cmd, "");
#if GOLF_PLATFORM_LINUX
        golf_string_appendf(&cmd, "tools/sokol-tools/linux/sokol-shdc --input %s --output out/temp/bare --slang %s --format bare", file.path, slangs);
#elif GOLF_PLATFORM_WINDOWS
        golf_string_appendf(&cmd, "tools\\sokol-tools\\win32\\sokol-shdc --input %s --output out/temp/bare --slang %s --format bare", file.path, slangs);
#elif GOLF_PLATFORM_MACOS
#endif
        int ret = system(cmd.cstr);
        if (ret == 0) {
            golf_string_t base_bare_name;
            golf_string_initf(&base_bare_name, "out/temp/bare_%s", file.name);
            golf_string_pop(&base_bare_name, 5);

            {
                JSON_Value *bare_val = _golf_shader_import_bare(base_bare_name.cstr, "glsl300es");
                json_object_set_value(obj, "glsl300es", bare_val);
            }

            {
                JSON_Value *bare_val = _golf_shader_import_bare(base_bare_name.cstr, "glsl330");
                json_object_set_value(obj, "glsl330", bare_val);
            }

            golf_string_deinit(&base_bare_name);
        }
        golf_string_deinit(&cmd);
    }

    golf_string_t import_shader_file_path;
    golf_string_initf(&import_shader_file_path, "%s.golf_data", file.path);
    json_serialize_to_file_pretty(val, import_shader_file_path.cstr);
    golf_string_deinit(&import_shader_file_path);

    json_value_free(val);
    return true;
}

static bool _golf_shader_load(golf_data_t *golf_data, const char *path, char *data, int data_len) {
    golf_data->type = GOLF_DATA_SHADER;
    golf_data->shader = malloc(sizeof(golf_shader_t));

    const sg_shader_desc *const_shader_desc;
    if (strcmp(path, "data/shaders/diffuse_color_material.glsl") == 0) {
        const_shader_desc = diffuse_color_material_shader_desc(sg_query_backend());
    }
    else if (strcmp(path, "data/shaders/environment_material.glsl") == 0) {
        const_shader_desc = environment_material_shader_desc(sg_query_backend());
    }
    else if (strcmp(path, "data/shaders/pass_through.glsl") == 0) {
        const_shader_desc = pass_through_shader_desc(sg_query_backend());
    }
    else if (strcmp(path, "data/shaders/solid_color_material.glsl") == 0) {
        const_shader_desc = solid_color_material_shader_desc(sg_query_backend());
    }
    else if (strcmp(path, "data/shaders/texture_material.glsl") == 0) {
        const_shader_desc = texture_material_shader_desc(sg_query_backend());
    }
    else if (strcmp(path, "data/shaders/ui_sprite.glsl") == 0) {
        const_shader_desc = ui_sprite_shader_desc(sg_query_backend());
    }
    else {
        golf_log_warning("No importer for shader %s", path);
        return false;
    }

    JSON_Value *val = json_parse_string(data);
    JSON_Object *obj = json_value_get_object(val);
    const char *fs = NULL, *vs = NULL;
#if SOKOL_GLCORE33
    fs = json_object_dotget_string(obj, "glsl330.fs");
    vs = json_object_dotget_string(obj, "glsl330.vs");
#elif SOKOL_GLES3
    fs = json_object_dotget_string(obj, "glsl300es.fs");
    vs = json_object_dotget_string(obj, "glsl300es.vs");
#endif

    sg_shader_desc shader_desc = *const_shader_desc;
    shader_desc.fs.source = fs;
    shader_desc.vs.source = vs;

    golf_data->shader->sg_shader = sg_make_shader(&shader_desc);

    json_value_free(val);

    return true;
}

static bool _golf_shader_unload(golf_data_t *golf_data) {
    sg_destroy_shader(golf_data->shader->sg_shader);
    free(golf_data->shader);
    return true;
}

//
// FONT
//

static void _stbi_write_func(void *context, void *data, int size) {
    vec_char_t *bmp = (vec_char_t*)context;
    vec_pusharr(bmp, (char*)data, size);
}

static JSON_Value *_golf_font_atlas_import(const char *file_data, int font_size, int img_size) {
    unsigned char *bitmap = malloc(img_size * img_size);
    stbtt_bakedchar cdata[96];
    memset(cdata, 0, sizeof(cdata));
    stbtt_BakeFontBitmap((const unsigned char*)file_data, 0, (float)-font_size, bitmap, img_size, img_size, 32, 95, cdata);

    float ascent, descent, linegap;
    stbtt_GetScaledFontVMetrics((const unsigned char*)file_data, 0, (float)-font_size, &ascent, &descent, &linegap);

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

    json_object_set_number(obj, "img_size", img_size);
    {
        vec_char_t img;
        vec_init(&img);
        stbi_write_png_to_func(_stbi_write_func, &img, img_size, img_size, 1, bitmap, img_size);
        golf_json_object_set_data(obj, "img_data", (unsigned char*)img.data, img.length);
        vec_deinit(&img);
    }

    free(bitmap);
    return val;
}

static bool _golf_font_import(const char *path, char *data, int data_len) {
    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    JSON_Value *atlases_val = json_value_init_array();
    JSON_Array *atlases_array = json_value_get_array(atlases_val);

    json_array_append_value(atlases_array, _golf_font_atlas_import(data, 16, 256));
    json_array_append_value(atlases_array, _golf_font_atlas_import(data, 24, 256));
    json_array_append_value(atlases_array, _golf_font_atlas_import(data, 32, 256));
    json_array_append_value(atlases_array, _golf_font_atlas_import(data, 40, 512));
    json_array_append_value(atlases_array, _golf_font_atlas_import(data, 48, 512));
    json_array_append_value(atlases_array, _golf_font_atlas_import(data, 56, 512));
    json_array_append_value(atlases_array, _golf_font_atlas_import(data, 64, 512));

    json_object_set_value(obj, "atlases", atlases_val);

    golf_string_t import_font_file_path;
    golf_string_initf(&import_font_file_path, "%s.golf_data", path);
    json_serialize_to_file(val, import_font_file_path.cstr);
    golf_string_deinit(&import_font_file_path);

    json_value_free(val);
    return true;
}

static void _golf_font_load_atlas(JSON_Object *atlas_obj, golf_font_atlas_t *atlas) {
    atlas->font_size = (float)json_object_get_number(atlas_obj, "font_size");
    atlas->ascent = (float)json_object_get_number(atlas_obj, "ascent");
    atlas->descent = (float)json_object_get_number(atlas_obj, "descent");
    atlas->linegap = (float)json_object_get_number(atlas_obj, "linegap");
    atlas->size = (int)json_object_get_number(atlas_obj, "img_size");

    JSON_Array *char_datas_array = json_object_get_array(atlas_obj, "char_datas");
    for (int i = 0; i < (int)json_array_get_count(char_datas_array); i++) {
        JSON_Object *char_data_obj = json_array_get_object(char_datas_array, i);
        int c = (int)json_object_get_number(char_data_obj, "c");
        if (c >= 0 && c < 256) {
            atlas->char_data[c].x0 = (float)json_object_get_number(char_data_obj, "x0");
            atlas->char_data[c].x1 = (float)json_object_get_number(char_data_obj, "x1");
            atlas->char_data[c].y0 = (float)json_object_get_number(char_data_obj, "y0");
            atlas->char_data[c].y1 = (float)json_object_get_number(char_data_obj, "y1");
            atlas->char_data[c].xoff = (float)json_object_get_number(char_data_obj, "xoff");
            atlas->char_data[c].yoff = (float)json_object_get_number(char_data_obj, "yoff");
            atlas->char_data[c].xadvance = (float)json_object_get_number(char_data_obj, "xadvance");
        }
    }

    unsigned char *img_data = NULL;
    int img_data_len;
    golf_json_object_get_data(atlas_obj, "img_data", &img_data, &img_data_len);

    int x, y, n;
    int force_channels = 4;
    stbi_set_flip_vertically_on_load(0);
    unsigned char *stb_data = stbi_load_from_memory(img_data, img_data_len, &x, &y, &n, force_channels);
    if (!stb_data) {
        golf_log_error("STB Failed to load image");
    }

    sg_image_desc img_desc = {
        .width = atlas->size,
        .height = atlas->size,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .data.subimage[0][0] = {
            .ptr = stb_data,
            .size = 4*sizeof(char)*x*y,
        }
    };
    atlas->sg_image = sg_make_image(&img_desc); 

    free(img_data);
    free(stb_data);
}

static bool _golf_font_load(golf_data_t *golf_data, const char *path, char *data, int data_len) {
    golf_data->type = GOLF_DATA_FONT;
    golf_data->font = malloc(sizeof(golf_font_t));

    JSON_Value *val = json_parse_string(data);
    JSON_Object *obj = json_value_get_object(val);
    if (!val) {
        golf_log_warning("Unable to parse json for font file %s", path);
        return false;
    }

    JSON_Array *atlases_array = json_object_get_array(obj, "atlases");
    vec_init(&golf_data->font->atlases);
    for (int i = 0; i < (int)json_array_get_count(atlases_array); i++) {
        JSON_Object *atlas_obj = json_array_get_object(atlases_array, i);
        golf_font_atlas_t atlas;
        _golf_font_load_atlas(atlas_obj, &atlas);
        vec_push(&golf_data->font->atlases, atlas);
    }

    json_value_free(val);

    return true;
}

static bool _golf_font_unload(golf_data_t *golf_data) {
    for (int i = 0; i < golf_data->font->atlases.length; i++) {
        golf_font_atlas_t atlas = golf_data->font->atlases.data[i];
        sg_destroy_image(atlas.sg_image);
    }
    vec_deinit(&golf_data->font->atlases);
    free(golf_data->font);
    return true;
}

//
// MODEL
//

typedef struct _model_material_data {
    const char *name;
    vec_float_t vertices;
} _model_material_data_t;
typedef vec_t(_model_material_data_t) _vec_model_material_data;

static bool _golf_model_import(const char *path, char *data, int data_len) {
    _vec_model_material_data model_materials;
    vec_init(&model_materials);

    fastObjMesh *m = fast_obj_read(path);
    for (int i = 0; i < (int)m->group_count; i++) {
        const fastObjGroup grp = m->groups[i];

        int idx = 0;
        for (int j = 0; j < (int)grp.face_count; j++) {
            int fv = m->face_vertices[grp.face_offset + j];
            int fm = m->face_materials[grp.face_offset + j];
            const char *material_name = "default";
            if (fm < (int)m->material_count) {
                fastObjMaterial mat = m->materials[fm];
                material_name = mat.name;
            }

            _model_material_data_t *model_material = NULL;
            for (int i = 0; i < model_materials.length; i++) {
                if (strcmp(model_materials.data[i].name, material_name) == 0) {
                    model_material = &model_materials.data[i];
                }
            }
            if (!model_material) {
                _model_material_data_t mat;
                mat.name = material_name;
                vec_init(&mat.vertices);
                vec_push(&model_materials, mat);
                model_material = &vec_last(&model_materials);
            }

            fastObjIndex m0 = m->indices[grp.index_offset + idx];
            vec3 p0 = vec3_create_from_array(&m->positions[3 * m0.p]);
            vec2 t0 = vec2_create_from_array(&m->texcoords[2 * m0.t]);
            vec3 n0 = vec3_create_from_array(&m->normals[3 * m0.n]);

            for (int k = 0; k < fv - 2; k++) {
                fastObjIndex m1 = m->indices[grp.index_offset + idx + k + 1];
                vec3 p1 = vec3_create_from_array(&m->positions[3 * m1.p]);
                vec2 t1 = vec2_create_from_array(&m->texcoords[2 * m1.t]);
                vec3 n1 = vec3_create_from_array(&m->normals[3 * m1.n]);

                fastObjIndex m2 = m->indices[grp.index_offset + idx + k + 2];
                vec3 p2 = vec3_create_from_array(&m->positions[3 * m2.p]);
                vec2 t2 = vec2_create_from_array(&m->texcoords[2 * m2.t]);
                vec3 n2 = vec3_create_from_array(&m->normals[3 * m2.n]);

                vec_push(&model_material->vertices, p0.x);
                vec_push(&model_material->vertices, p0.y);
                vec_push(&model_material->vertices, p0.z);
                vec_push(&model_material->vertices, n0.x);
                vec_push(&model_material->vertices, n0.y);
                vec_push(&model_material->vertices, n0.z);
                vec_push(&model_material->vertices, t0.x);
                vec_push(&model_material->vertices, t0.y);

                vec_push(&model_material->vertices, p1.x);
                vec_push(&model_material->vertices, p1.y);
                vec_push(&model_material->vertices, p1.z);
                vec_push(&model_material->vertices, n1.x);
                vec_push(&model_material->vertices, n1.y);
                vec_push(&model_material->vertices, n1.z);
                vec_push(&model_material->vertices, t1.x);
                vec_push(&model_material->vertices, t1.y);

                vec_push(&model_material->vertices, p2.x);
                vec_push(&model_material->vertices, p2.y);
                vec_push(&model_material->vertices, p2.z);
                vec_push(&model_material->vertices, n2.x);
                vec_push(&model_material->vertices, n2.y);
                vec_push(&model_material->vertices, n2.z);
                vec_push(&model_material->vertices, t2.x);
                vec_push(&model_material->vertices, t2.y);
            }

            idx += fv;
        }
    }

    JSON_Value *json_groups_val = json_value_init_array();
    JSON_Array *json_groups_arr = json_value_get_array(json_groups_val);
    for (int i = 0; i < model_materials.length; i++) {
        JSON_Value *json_group_val = json_value_init_object();
        JSON_Object *json_group_obj = json_value_get_object(json_group_val);
        json_object_set_string(json_group_obj, "material_name", model_materials.data[i].name);

        JSON_Value *json_vertices_val = json_value_init_array();
        JSON_Array *json_vertices_arr = json_value_get_array(json_vertices_val);
        for (int j = 0; j < model_materials.data[i].vertices.length; j++) {
            json_array_append_number(json_vertices_arr, model_materials.data[i].vertices.data[j]);
        }
        json_object_set_value(json_group_obj, "vertices", json_vertices_val);

        json_array_append_value(json_groups_arr, json_group_val);
    }

    JSON_Value *json_val = json_value_init_object();
    JSON_Object *json_obj = json_value_get_object(json_val);
    json_object_set_value(json_obj, "groups", json_groups_val);

    golf_string_t import_model_file_path;
    golf_string_initf(&import_model_file_path, "%s.golf_data", path);
    json_serialize_to_file(json_val, import_model_file_path.cstr);

    golf_string_deinit(&import_model_file_path);
    fast_obj_destroy(m);
    json_value_free(json_val);
    for (int i = 0; i < model_materials.length; i++) {
        vec_deinit(&model_materials.data[i].vertices);
    }
    vec_deinit(&model_materials);
    return true;
}

static bool _golf_model_load(golf_data_t *golf_data, const char *path, char *data, int data_len) {
    golf_data->type = GOLF_DATA_MODEL;
    golf_data->model = malloc(sizeof(golf_model_t));

    JSON_Value *val = json_parse_string(data);
    JSON_Object *obj = json_value_get_object(val);
    if (!val) {
        golf_log_warning("Unable to parse json model file %s", path);
        return false;
    }

    vec_init(&golf_data->model->groups);
    vec_init(&golf_data->model->positions);
    vec_init(&golf_data->model->normals);
    vec_init(&golf_data->model->texcoords);

    JSON_Array *json_groups_arr = json_object_get_array(obj, "groups");
    for (int i = 0; i < (int)json_array_get_count(json_groups_arr); i++) {
        JSON_Object *json_group_obj = json_array_get_object(json_groups_arr, i);
        const char *material_name = json_object_get_string(json_group_obj, "material_name");
        JSON_Array *json_vertices_arr = json_object_get_array(json_group_obj, "vertices");

        golf_model_group_t model_group;
        snprintf(model_group.material_name, GOLF_MODEL_MATERIAL_NAME_MAX_LEN, "%s", material_name);
        model_group.start_vertex = golf_data->model->positions.length;
        model_group.vertex_count = (int)json_array_get_count(json_vertices_arr) / 8;
        for (int j = 0; j < (int)json_array_get_count(json_vertices_arr); j += 8) {
            vec3 p;
            vec2 t;
            vec3 n;

            p.x = (float)json_array_get_number(json_vertices_arr, j + 0);
            p.y = (float)json_array_get_number(json_vertices_arr, j + 1);
            p.z = (float)json_array_get_number(json_vertices_arr, j + 2);
            n.x = (float)json_array_get_number(json_vertices_arr, j + 3);
            n.y = (float)json_array_get_number(json_vertices_arr, j + 4);
            n.z = (float)json_array_get_number(json_vertices_arr, j + 5);
            t.x = (float)json_array_get_number(json_vertices_arr, j + 6);
            t.y = (float)json_array_get_number(json_vertices_arr, j + 7);

            vec_push(&golf_data->model->positions, p);
            vec_push(&golf_data->model->texcoords, t);
            vec_push(&golf_data->model->normals, n);
        }
        vec_push(&golf_data->model->groups, model_group);
    }

    {
        sg_buffer_desc desc = {
            .type = SG_BUFFERTYPE_VERTEXBUFFER,
        };

        desc.data.size = sizeof(vec3) * golf_data->model->positions.length;
        desc.data.ptr = golf_data->model->positions.data;
        golf_data->model->sg_positions_buf = sg_make_buffer(&desc);

        desc.data.size = sizeof(vec3) * golf_data->model->normals.length;
        desc.data.ptr = golf_data->model->normals.data;
        golf_data->model->sg_normals_buf = sg_make_buffer(&desc);

        desc.data.size = sizeof(vec2) * golf_data->model->texcoords.length;
        desc.data.ptr = golf_data->model->texcoords.data;
        golf_data->model->sg_texcoords_buf = sg_make_buffer(&desc);
    }

    json_value_free(val);

    return true;
}

static bool _golf_model_unload(golf_data_t *golf_data) {
    vec_deinit(&golf_data->model->positions);
    vec_deinit(&golf_data->model->normals);
    vec_deinit(&golf_data->model->texcoords);
    free(golf_data->model);
    return true;
}

//
// UI PIXEL PACK
//

static void _golf_pixel_pack_pos_to_uvs(golf_pixel_pack_t *pp, int tex_w, int tex_h, vec2 p, vec2 *uv0, vec2 *uv1) {
    uv0->x = (pp->tile_size + pp->tile_padding) * p.x;
    uv0->x /= tex_w;
    uv0->y = (pp->tile_size + pp->tile_padding) * p.y;
    uv0->y /= tex_h;

    uv1->x = (pp->tile_size + pp->tile_padding) * (p.x + 1) - pp->tile_padding;
    uv1->x /= tex_w;
    uv1->y = (pp->tile_size + pp->tile_padding) * (p.y + 1) - pp->tile_padding;
    uv1->y /= tex_h;
}

static bool _golf_pixel_pack_load(const char *path, char *data, int data_len, golf_pixel_pack_t *pixel_pack) {
    JSON_Value *val = json_parse_string(data);
    JSON_Object *obj = json_value_get_object(val);

    pixel_pack->texture = golf_data_get_texture(json_object_get_string(obj, "texture"));
    pixel_pack->tile_size = (float)json_object_get_number(obj, "tile_size");
    pixel_pack->tile_padding = (float)json_object_get_number(obj, "tile_padding");
    map_init(&pixel_pack->icons);
    map_init(&pixel_pack->squares);

    golf_texture_t *tex = pixel_pack->texture;

    JSON_Array *icons_array = json_object_get_array(obj, "icons");
    for (int i = 0; i < (int)json_array_get_count(icons_array); i++) {
        JSON_Object *icon_obj = json_array_get_object(icons_array, i);
        const char *name = json_object_get_string(icon_obj, "name");
        vec2 pos = golf_json_object_get_vec2(icon_obj, "pos");

        golf_pixel_pack_icon_t icon;
        _golf_pixel_pack_pos_to_uvs(pixel_pack, tex->width, tex->height, pos, &icon.uv0, &icon.uv1);
        map_set(&pixel_pack->icons, name, icon);
    }

    JSON_Array *squares_array = json_object_get_array(obj, "squares");
    for (int i = 0; i < (int)json_array_get_count(squares_array); i++) {
        JSON_Object *square_obj = json_array_get_object(squares_array, i);
        const char *name = json_object_get_string(square_obj, "name");
        vec2 tl = golf_json_object_get_vec2(square_obj, "top_left");
        vec2 tm = golf_json_object_get_vec2(square_obj, "top_mid");
        vec2 tr = golf_json_object_get_vec2(square_obj, "top_right");
        vec2 ml = golf_json_object_get_vec2(square_obj, "mid_left");
        vec2 mm = golf_json_object_get_vec2(square_obj, "mid_mid");
        vec2 mr = golf_json_object_get_vec2(square_obj, "mid_right");
        vec2 bl = golf_json_object_get_vec2(square_obj, "bot_left");
        vec2 bm = golf_json_object_get_vec2(square_obj, "bot_mid");
        vec2 br = golf_json_object_get_vec2(square_obj, "bot_right");

        golf_pixel_pack_square_t square;
        _golf_pixel_pack_pos_to_uvs(pixel_pack, tex->width, tex->height, tl, &square.tl_uv0, &square.tl_uv1);
        _golf_pixel_pack_pos_to_uvs(pixel_pack, tex->width, tex->height, tm, &square.tm_uv0, &square.tm_uv1);
        _golf_pixel_pack_pos_to_uvs(pixel_pack, tex->width, tex->height, tr, &square.tr_uv0, &square.tr_uv1);
        _golf_pixel_pack_pos_to_uvs(pixel_pack, tex->width, tex->height, ml, &square.ml_uv0, &square.ml_uv1);
        _golf_pixel_pack_pos_to_uvs(pixel_pack, tex->width, tex->height, mm, &square.mm_uv0, &square.mm_uv1);
        _golf_pixel_pack_pos_to_uvs(pixel_pack, tex->width, tex->height, mr, &square.mr_uv0, &square.mr_uv1);
        _golf_pixel_pack_pos_to_uvs(pixel_pack, tex->width, tex->height, bl, &square.bl_uv0, &square.bl_uv1);
        _golf_pixel_pack_pos_to_uvs(pixel_pack, tex->width, tex->height, bm, &square.bm_uv0, &square.bm_uv1);
        _golf_pixel_pack_pos_to_uvs(pixel_pack, tex->width, tex->height, br, &square.br_uv0, &square.br_uv1);
        map_set(&pixel_pack->squares, name, square);
    }

    json_value_free(val);
    return true;
}

static bool _golf_pixel_pack_unload(golf_pixel_pack_t *pixel_pack) {
    map_deinit(&pixel_pack->icons);
    map_deinit(&pixel_pack->squares);
    return true;
}

//
// CONFIG
//

static bool _golf_config_load(const char *path, char *data, int data_len, golf_config_t *config) {
    JSON_Value *val = json_parse_string(data);
    JSON_Object *obj = json_value_get_object(val);

    if (!val || !obj) {
        golf_log_warning("Can't parse json for config file %s", path);
        return false;
    }

    map_init(&config->properties);

    for (int i = 0; i < (int)json_object_get_count(obj); i++) {
        const char *name = json_object_get_name(obj, i);
        JSON_Value *prop_val = json_object_get_value_at(obj, i);
        JSON_Value_Type type = json_value_get_type(prop_val);

        bool valid_property = false;
        golf_config_property_t data_property;  
        if (type == JSONNumber) {
            double prop_num = json_value_get_number(prop_val);
            data_property.type = GOLF_CONFIG_PROPERTY_NUM;
            data_property.num_val = (float)prop_num;
            valid_property = true;
        }
        else if (type == JSONString) {
            const char *prop_string = json_value_get_string(prop_val);
            data_property.type = GOLF_CONFIG_PROPERTY_STRING;
            data_property.string_val = malloc(strlen(prop_string) + 1);
            strcpy(data_property.string_val, prop_string);
            valid_property = true;
        }
        else if (type == JSONArray) {
            JSON_Array *prop_array = json_value_get_array(prop_val);
            if (json_array_get_count(prop_array) == 2) {
                data_property.type = GOLF_CONFIG_PROPERTY_VEC2;
                data_property.vec2_val = golf_json_object_get_vec2(obj, name);
                valid_property = true;
            }
            else if (json_array_get_count(prop_array) == 3) {
                data_property.type = GOLF_CONFIG_PROPERTY_VEC3;
                data_property.vec3_val = golf_json_object_get_vec3(obj, name);
                valid_property = true;
            }
            else if (json_array_get_count(prop_array) == 4) {
                data_property.type = GOLF_CONFIG_PROPERTY_VEC4;
                data_property.vec4_val = golf_json_object_get_vec4(obj, name);
                valid_property = true;
            }
        }

        if (valid_property) {
            map_set(&config->properties, name, data_property);
        }
        else {
            golf_log_warning("Property %s is invalid", name);
        }
    }

    json_value_free(val);
    return true;
}

static bool _golf_config_unload(golf_config_t *config) {
    const char *key;
    map_iter_t iter = map_iter(&config->properties);

    while ((key = map_next(&config->properties, &iter))) {
        golf_config_property_t *prop = map_get(&config->properties, key);
        switch (prop->type) {
            case GOLF_CONFIG_PROPERTY_NUM:
            case GOLF_CONFIG_PROPERTY_VEC2:
            case GOLF_CONFIG_PROPERTY_VEC3:
            case GOLF_CONFIG_PROPERTY_VEC4:
                break;
            case GOLF_CONFIG_PROPERTY_STRING:
                free(prop->string_val);
                break;
        }
    }

    map_deinit(&config->properties);
    return true;
}

float golf_config_get_num(golf_config_t *cfg, const char *name) {
    golf_config_property_t *prop = map_get(&cfg->properties, name);
    if (prop && prop->type == GOLF_CONFIG_PROPERTY_NUM) {
        return prop->num_val;
    }
    else {
        golf_log_warning("Invalid config property %s", name);
        return 0.0f;
    }
}

const char *golf_config_get_string(golf_config_t *cfg, const char *name) {
    golf_config_property_t *prop = map_get(&cfg->properties, name);
    if (prop && prop->type == GOLF_CONFIG_PROPERTY_STRING) {
        return prop->string_val;
    }
    else {
        golf_log_warning("Invalid config property %s", name);
        return "";
    }
}

vec2 golf_config_get_vec2(golf_config_t *cfg, const char *name) {
    golf_config_property_t *prop = map_get(&cfg->properties, name);
    if (prop && prop->type == GOLF_CONFIG_PROPERTY_VEC2) {
        return prop->vec2_val;
    }
    else {
        golf_log_warning("Invalid config property %s", name);
        return V2(0, 0);
    }
}

vec3 golf_config_get_vec3(golf_config_t *cfg, const char *name) {
    golf_config_property_t *prop = map_get(&cfg->properties, name);
    if (prop && prop->type == GOLF_CONFIG_PROPERTY_VEC3) {
        return prop->vec3_val;
    }
    else {
        golf_log_warning("Invalid config property %s", name);
        return V3(0, 0, 0);
    }
}

vec4 golf_config_get_vec4(golf_config_t *cfg, const char *name) {
    golf_config_property_t *prop = map_get(&cfg->properties, name);
    if (prop && prop->type == GOLF_CONFIG_PROPERTY_VEC4) {
        return prop->vec4_val;
    }
    else {
        golf_log_warning("Invalid config property %s", name);
        return V4(0, 0, 0, 0);
    }
}

static bool _golf_level_load(golf_data_t *golf_data, const char *path, char *data, int data_len) {
    golf_data->type = GOLF_DATA_LEVEL;
    golf_data->level = malloc(sizeof(golf_level_t));
    return golf_level_load(golf_data->level, path, data, data_len);
}

static bool _golf_level_unload(golf_data_t *golf_data) {
    bool result = golf_level_unload(golf_data->level);
    free(golf_data->level);
    return result;
}

static bool _golf_static_data_load(golf_data_t *golf_data, const char *path, char *data, int data_len) {
    golf_data->type = GOLF_DATA_STATIC_DATA;
    golf_data->static_data = malloc(sizeof(golf_static_data_t));

    JSON_Value *val = json_parse_string(data);
    JSON_Array *arr = json_value_get_array(val);

    vec_init(&golf_data->static_data->data_paths);
    for (int i = 0; i < (int)json_array_get_count(arr); i++) {
        const char *data_path = json_array_get_string(arr, i);
        if (data_path) {
            char *data_path_copy = malloc(strlen(data_path) + 1);
            strcpy(data_path_copy, data_path);
            vec_push(&golf_data->static_data->data_paths, data_path_copy);
            golf_data_load(data_path_copy);
        }
    }

    json_value_free(val);

    return true;
}

static bool _golf_static_data_unload(golf_data_t *golf_data) {
    for (int i = 0; i < golf_data->static_data->data_paths.length; i++) {
        char *data_path = golf_data->static_data->data_paths.data[i];
        golf_data_unload(data_path);
        free(data_path);
    }
    vec_deinit(&golf_data->static_data->data_paths);
    free(golf_data->static_data);
    return true;
}

//
// DATA
//

void golf_data_init(void) {
    _assetsys = assetsys_create(NULL);
    assetsys_error_t error = assetsys_mount(_assetsys, "data", "/data");
    if (error != ASSETSYS_SUCCESS) {
        golf_log_error("Unable to mount data");
    }
    map_init(&_loaded_data);

    golf_data_run_import(false);
    golf_data_load("data/static_data.static_data");
}

static golf_data_unloader_t _golf_data_get_unloader(const char *ext) {
    if ((strcmp(ext, ".png") == 0) ||
            (strcmp(ext, ".jpg") == 0) ||
            (strcmp(ext, ".bmp") == 0)) {
        return &_golf_texture_unload;
    }
    else if ((strcmp(ext, ".glsl") == 0)) {
        return &_golf_shader_unload;
    }
    else if ((strcmp(ext, ".ttf") == 0)) {
        return &_golf_font_unload;
    }
    else if ((strcmp(ext, ".obj") == 0)) {
        return &_golf_model_unload;
    }
    else if ((strcmp(ext, ".level") == 0)) {
        return &_golf_level_unload;
    }
    else if ((strcmp(ext, ".static_data") == 0)) {
        return &_golf_static_data_unload;
    }
    else {
        return NULL;
    }
}

static golf_data_loader_t _golf_data_get_loader(const char *ext) {
    if ((strcmp(ext, ".png") == 0) ||
            (strcmp(ext, ".jpg") == 0) ||
            (strcmp(ext, ".bmp") == 0)) {
        return &_golf_texture_load;
    }
    else if ((strcmp(ext, ".glsl") == 0)) {
        return &_golf_shader_load;
    }
    else if ((strcmp(ext, ".ttf") == 0)) {
        return &_golf_font_load;
    }
    else if ((strcmp(ext, ".obj") == 0)) {
        return &_golf_model_load;
    }
    else if ((strcmp(ext, ".level") == 0)) {
        return &_golf_level_load;
    }
    else if ((strcmp(ext, ".static_data") == 0)) {
        return &_golf_static_data_load;
    }
    else {
        return NULL;
    }
}

static golf_data_importer_t _golf_data_get_importer(const char *ext) { 
    if ((strcmp(ext, ".png") == 0) ||
            (strcmp(ext, ".jpg") == 0) ||
            (strcmp(ext, ".bmp") == 0)) {
        return &_golf_texture_import;
    }
    else if ((strcmp(ext, ".glsl") == 0)) {
        return &_golf_shader_import;
    }
    else if ((strcmp(ext, ".ttf") == 0)) {
        return &_golf_font_import;
    }
    else if ((strcmp(ext, ".obj") == 0)) {
        return &_golf_model_import;
    }
    else {
        return NULL;
    }
}

void golf_data_run_import(bool force_import) {
    golf_dir_t dir;
    golf_dir_init(&dir, "data", true);

    for (int i = 0; i < dir.num_files; i++) {
        golf_file_t file = dir.files[i];
        golf_data_importer_t importer = _golf_data_get_importer(file.ext);

        if (importer) {
            golf_file_t import_file = golf_file_append_extension(file.path, ".golf_data");
            uint64_t file_time = golf_file_get_time(file.path);
            uint64_t import_file_time = golf_file_get_time(import_file.path);
            if (!force_import && (import_file_time >= file_time)) {
                continue;
            }

            golf_log_note("Importing file %s", file.path);
            char *data;
            int data_len;
            if (!golf_file_load_data(file.path, &data, &data_len)) {
                golf_log_warning("Unable to load file %s", file.path); 
                continue;
            }
            importer(file.path, data, data_len); 
            free(data);
        }
    }

    golf_dir_deinit(&dir);
}

void golf_data_update(float dt) {
    const char *key;
    map_iter_t iter = map_iter(&_loaded_data);

    while ((key = map_next(&_loaded_data, &iter))) {
        golf_data_t *golf_data = map_get(&_loaded_data, key);

        uint64_t file_time = golf_file_get_time(golf_data->file.path);
        if (golf_data->last_load_time < file_time) {
            golf_data->last_load_time = file_time;
            golf_file_t file = golf_file(key);
            golf_file_t file_to_load = golf_data->file;

            char *data = NULL;
            int data_len = 0;
            assetsys_error_t error = _golf_assetsys_file_load(file_to_load.path, &data, &data_len);
            if (error == ASSETSYS_SUCCESS) {
                golf_log_note("Reloading file %s", key);

                golf_data_unloader_t unloader = _golf_data_get_unloader(file.ext);
                golf_data_loader_t loader = _golf_data_get_loader(file.ext);
                if (unloader && loader) {
                    unloader(golf_data);
                    loader(golf_data, file.path, data, data_len);
                }
                else {
                    golf_log_warning("Unable to load file %s", key);
                }
            }
            else {
                golf_log_warning("Unable to load file %s", key);
            }
            free(data);
        }
    }
}

void golf_data_load(const char *path) {
    golf_log_note("Loading file %s", path);

    golf_data_t *loaded_data = map_get(&_loaded_data, path);
    if (loaded_data) {
        loaded_data->load_count++;
        return;
    }

    golf_file_t file = golf_file(path);
    golf_file_t file_to_load = golf_file(path);
    golf_data_importer_t importer = _golf_data_get_importer(file_to_load.ext);
    if (importer) {
        file_to_load = golf_file_append_extension(path, ".golf_data");
    }

    char *data = NULL;
    int data_len = 0;
    assetsys_error_t error = _golf_assetsys_file_load(file_to_load.path, &data, &data_len);
    if (error == ASSETSYS_SUCCESS) {
        golf_data_t golf_data;
        golf_data.load_count = 1;
        golf_data.file = file_to_load;
        golf_data.last_load_time = golf_file_get_time(file_to_load.path);

        golf_data_loader_t data_loader = _golf_data_get_loader(file.ext);
        if (data_loader) {
            data_loader(&golf_data, path, data, data_len);
            map_set(&_loaded_data, file.path, golf_data);
        }
        else {
            golf_log_warning("Unable to load file %s", file_to_load.path);
        }
    }
    free(data);
}

void golf_data_unload(const char *path) {
    golf_log_note("Unloading file %s", path);

    golf_data_t *golf_data = map_get(&_loaded_data, path); 
    if (!golf_data) {
        golf_log_warning("Unloading file %s that is not loaded", path);
        return;
    }

    golf_data->load_count--;
    if (golf_data->load_count == 0) {
        golf_file_t file = golf_file(path);
        golf_data_unloader_t unloader = _golf_data_get_unloader(file.ext);
        if (unloader) {
            unloader(golf_data);
        }
        else {
            golf_log_warning("Unable to unload file %s", path);
        }

        map_remove(&_loaded_data, path);
    }
}

golf_data_t *golf_data_get_file(const char *path) {
    return map_get(&_loaded_data, path);
}

golf_texture_t *golf_data_get_texture(const char *path) {
    static const char *fallback_texture = "data/textures/fallback.png";

    golf_data_t *data_file = golf_data_get_file(path);
    if (!data_file || data_file->type != GOLF_DATA_TEXTURE) {
        golf_log_warning("Could not find texture %s. Using fallback", path);
        data_file = golf_data_get_file(fallback_texture);
        if (!data_file || data_file->type != GOLF_DATA_TEXTURE) {
            golf_log_error("Could not find fallback texture");
        }
    }
    return data_file->texture;
}

golf_pixel_pack_t *golf_data_get_pixel_pack(const char *path) {
    static const char *fallback_pixel_pack = "data/textures/pixel_pack.pixel_pack";

    golf_data_t *data_file = golf_data_get_file(path);
    if (!data_file || data_file->type != GOLF_DATA_PIXEL_PACK) {
        golf_log_warning("Could not find pixel pack %s. Using fallback", path);
        data_file = golf_data_get_file(fallback_pixel_pack);
        if (!data_file || data_file->type != GOLF_DATA_PIXEL_PACK) {
            golf_log_error("Could not find fallback pixel pack");
        }
    }
    return data_file->pixel_pack;
}

golf_model_t *golf_data_get_model(const char *path) {
    static const char *fallback = "data/models/cube.obj";

    golf_data_t *data_file = golf_data_get_file(path);
    if (!data_file || data_file->type != GOLF_DATA_MODEL) {
        golf_log_warning("Could not find model %s. Using fallback", path);
        data_file = golf_data_get_file(fallback);
        if (!data_file || data_file->type != GOLF_DATA_MODEL) {
            golf_log_error("Could not find fallback model");
        }
    }
    return data_file->model;
}

golf_shader_t *golf_data_get_shader(const char *path) {
    golf_data_t *data_file = golf_data_get_file(path);
    if (!data_file || data_file->type != GOLF_DATA_SHADER) {
        golf_log_error("Could not find shader %s.", path);
        return NULL;
    }
    return data_file->shader;
}

golf_font_t *golf_data_get_font(const char *path) {
    static const char *fallback = "data/font/DroidSerif-Bold.ttf";

    golf_data_t *data_file = golf_data_get_file(path);
    if (!data_file || data_file->type != GOLF_DATA_FONT) {
        golf_log_warning("Could not find font %s. Using fallback", path);
        data_file = golf_data_get_file(fallback);
        if (!data_file || data_file->type != GOLF_DATA_FONT) {
            golf_log_error("Could not find fallback font");
        }
    }
    return data_file->font;
}

golf_config_t *golf_data_get_config(const char *path) {
    golf_data_t *data_file = golf_data_get_file(path);
    if (!data_file || data_file->type != GOLF_DATA_CONFIG) {
        golf_log_error("Could not find config file %s", path);
    }
    return data_file->config;
}

golf_level_t *golf_data_get_level(const char *path) {
    golf_data_t *data_file = golf_data_get_file(path);
    if (!data_file || data_file->type != GOLF_DATA_LEVEL) {
        golf_log_error("Could not find level file %s", path);
    }
    return data_file->level;
}

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"

void golf_data_debug_console_tab(void) {
    if (igCollapsingHeader_TreeNodeFlags("Textures", ImGuiTreeNodeFlags_None)) {
        const char *key;
        map_iter_t iter = map_iter(&_loaded_data);

        while ((key = map_next(&_loaded_data, &iter))) {
            golf_data_t *loaded_file = map_get(&_loaded_data, key);
            if (loaded_file->type != GOLF_DATA_TEXTURE) {
                continue;
            }

            golf_texture_t *texture = loaded_file->texture;
            if (igTreeNode_Str(key)) {
                igText("Width: %d", texture->width);
                igText("Height: %d", texture->height);
                igImage((ImTextureID)(intptr_t)texture->sg_image.id, (ImVec2){(float)texture->width, (float)texture->height}, (ImVec2){0, 0}, (ImVec2){1, 1}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 1});
                igTreePop();
            }
        }
    }

    if (igCollapsingHeader_TreeNodeFlags("Fonts", ImGuiTreeNodeFlags_None)) {
        const char *key;
        map_iter_t iter = map_iter(&_loaded_data);

        while ((key = map_next(&_loaded_data, &iter))) {
            golf_data_t *loaded_file = map_get(&_loaded_data, key);
            if (loaded_file->type != GOLF_DATA_FONT) {
                continue;
            }

            golf_font_t *font = loaded_file->font;
            if (igTreeNode_Str(key)) {
                for (int i = 0; i < font->atlases.length; i++) {
                    golf_font_atlas_t *atlas = &font->atlases.data[i];
                    igText("Font Size: %f", atlas->font_size);
                    igImage((ImTextureID)(intptr_t)atlas->sg_image.id, (ImVec2){(float)atlas->size, (float)atlas->size}, (ImVec2){0, 0}, (ImVec2){1, 1}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 1});
                }
                igTreePop();
            }
        }
    }

    if (igCollapsingHeader_TreeNodeFlags("Models", ImGuiTreeNodeFlags_None)) {
        const char *key;
        map_iter_t iter = map_iter(&_loaded_data);

        while ((key = map_next(&_loaded_data, &iter))) {
            golf_data_t *loaded_file = map_get(&_loaded_data, key);
            if (loaded_file->type != GOLF_DATA_MODEL) {
                continue;
            }

            golf_model_t *model = loaded_file->model;
            if (igTreeNode_Str(key)) {
                if (igTreeNode_Str("Positions: ")) {
                    for (int i = 0; i < model->positions.length; i++) {
                        vec3 position = model->positions.data[i];
                        igText("<%.3f, %.3f, %.3f>", position.x, position.y, position.z);
                    }
                    igTreePop();
                }
                if (igTreeNode_Str("Normals: ")) {
                    for (int i = 0; i < model->normals.length; i++) {
                        vec3 normal = model->normals.data[i];
                        igText("<%.3f, %.3f, %.3f>", normal.x, normal.y, normal.z);
                    }
                    igTreePop();
                }
                if (igTreeNode_Str("Texcoords: ")) {
                    for (int i = 0; i < model->texcoords.length; i++) {
                        vec2 texcoord = model->texcoords.data[i];
                        igText("<%.3f, %.3f>", texcoord.x, texcoord.y);
                    }
                    igTreePop();
                }
                igTreePop();
            }
        }
    }

    if (igCollapsingHeader_TreeNodeFlags("Shaders", ImGuiTreeNodeFlags_None)) {
        const char *key;
        map_iter_t iter = map_iter(&_loaded_data);

        while ((key = map_next(&_loaded_data, &iter))) {
            golf_data_t *loaded_file = map_get(&_loaded_data, key);
            if (loaded_file->type != GOLF_DATA_SHADER) {
                continue;
            }
            if (igTreeNode_Str(key)) {
                igTreePop();
            }
        }
    }

    if (igCollapsingHeader_TreeNodeFlags("Pixel Packs", ImGuiTreeNodeFlags_None)) {
        const char *key;
        map_iter_t iter = map_iter(&_loaded_data);

        while ((key = map_next(&_loaded_data, &iter))) {
            golf_data_t *loaded_file = map_get(&_loaded_data, key);
            if (loaded_file->type != GOLF_DATA_PIXEL_PACK) {
                continue;
            }

            golf_pixel_pack_t *pp = loaded_file->pixel_pack;
            golf_texture_t *t = pp->texture;

            if (igTreeNode_Str(key)) {
                if (igTreeNode_Str("Icons")) {
                    const char *icon_key;
                    map_iter_t icons_iter = map_iter(&pp->icons);
                    while ((icon_key = map_next(&pp->icons, &icons_iter))) {
                        if (igTreeNode_Str(icon_key)) {
                            golf_pixel_pack_icon_t *i = map_get(&pp->icons, icon_key);
                            igImage((ImTextureID)(intptr_t)t->sg_image.id, (ImVec2){40, 40}, (ImVec2){i->uv0.x, i->uv0.y}, (ImVec2){i->uv1.x, i->uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 1});
                            igTreePop();
                        }
                    }
                    igTreePop();
                }

                if (igTreeNode_Str("Squares")) {
                    const char *square_key;
                    map_iter_t squares_iter = map_iter(&pp->squares);
                    while ((square_key = map_next(&pp->squares, &squares_iter))) {
                        if (igTreeNode_Str(square_key)) {
                            golf_pixel_pack_square_t *s = map_get(&pp->squares, square_key);

                            igPushStyleVar_Vec2(ImGuiStyleVar_ItemSpacing, (ImVec2){0, 0});

                            igImage((ImTextureID)(intptr_t)t->sg_image.id, (ImVec2){40, 40}, (ImVec2){s->tl_uv0.x, s->tl_uv0.y}, (ImVec2){s->tl_uv1.x, s->tl_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
                            igSameLine(0, 0);
                            igImage((ImTextureID)(intptr_t)t->sg_image.id, (ImVec2){40, 40}, (ImVec2){s->tm_uv0.x, s->tm_uv0.y}, (ImVec2){s->tm_uv1.x, s->tm_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
                            igSameLine(0, 0);
                            igImage((ImTextureID)(intptr_t)t->sg_image.id, (ImVec2){40, 40}, (ImVec2){s->tr_uv0.x, s->tr_uv0.y}, (ImVec2){s->tr_uv1.x, s->tr_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});

                            igImage((ImTextureID)(intptr_t)t->sg_image.id, (ImVec2){40, 40}, (ImVec2){s->ml_uv0.x, s->ml_uv0.y}, (ImVec2){s->ml_uv1.x, s->ml_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
                            igSameLine(0, 0);
                            igImage((ImTextureID)(intptr_t)t->sg_image.id, (ImVec2){40, 40}, (ImVec2){s->mm_uv0.x, s->mm_uv0.y}, (ImVec2){s->mm_uv1.x, s->mm_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
                            igSameLine(0, 0);
                            igImage((ImTextureID)(intptr_t)t->sg_image.id, (ImVec2){40, 40}, (ImVec2){s->mr_uv0.x, s->mr_uv0.y}, (ImVec2){s->mr_uv1.x, s->mr_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});

                            igImage((ImTextureID)(intptr_t)t->sg_image.id, (ImVec2){40, 40}, (ImVec2){s->bl_uv0.x, s->bl_uv0.y}, (ImVec2){s->bl_uv1.x, s->bl_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
                            igSameLine(0, 0);
                            igImage((ImTextureID)(intptr_t)t->sg_image.id, (ImVec2){40, 40}, (ImVec2){s->bm_uv0.x, s->bm_uv0.y}, (ImVec2){s->bm_uv1.x, s->bm_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
                            igSameLine(0, 0);
                            igImage((ImTextureID)(intptr_t)t->sg_image.id, (ImVec2){40, 40}, (ImVec2){s->br_uv0.x, s->br_uv0.y}, (ImVec2){s->br_uv1.x, s->br_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});

                            igPopStyleVar(1);
                            igTreePop();
                        }
                    }
                    igTreePop();
                }

                igTreePop();
            }
        }
    }
}
