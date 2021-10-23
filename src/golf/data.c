#include "golf/data.h"

#include "3rd_party/fast_obj/fast_obj.h"
#include "3rd_party/map/map.h"
#include "3rd_party/parson/parson.h"
#include "3rd_party/stb/stb_image.h"
#include "3rd_party/stb/stb_image_write.h"
#include "3rd_party/stb/stb_truetype.h"
#include "golf/base64.h"
#include "golf/file.h"
#include "golf/log.h"
#include "golf/maths.h"
#include "golf/string.h"

#include "golf/shaders/ui_sprite.glsl.h"

typedef struct golf_data {
    map_golf_data_file_t loaded_files;
} golf_data_t;

typedef bool (*golf_data_importer_t)(const char *path, char *data, int data_len);
typedef bool (*golf_data_loader_t)(const char *path, char *data, int data_len, golf_data_file_t *data_file);
typedef bool (*golf_data_unloader_t)(golf_data_file_t *data_file);

static golf_data_t _golf_data;

//
// PARSON HELPERS
//

static void _golf_data_json_object_get_data(JSON_Object *obj, const char *name, unsigned char **data, int *data_len) {
    const char *enc_data = json_object_get_string(obj, name); 
    int enc_len = strlen(enc_data);
    *data_len = golf_base64_decode_out_len(enc_data, enc_len);
    *data = malloc(*data_len);
    if (!golf_base64_decode(enc_data, enc_len, *data)) {
        golf_log_warning("Failed to decode data in field %s", name);
    }
}

static void _golf_data_json_object_set_data(JSON_Object *obj, const char *name, unsigned char *data, int data_len) {
    int enc_len = golf_base64_encode_out_len(data, data_len);
    char *enc_data = malloc(enc_len);
    if (!golf_base64_encode(data, data_len, enc_data)) {
        golf_log_warning("Failed to encode data in field %s", name);
    }
    json_object_set_string(obj, name, enc_data);
    free(enc_data);
}

static vec2 _golf_data_json_object_get_vec2(JSON_Object *obj, const char *name) {
    JSON_Array *array = json_object_get_array(obj, name);
    vec2 v;
    v.x = (float)json_array_get_number(array, 0);
    v.y = (float)json_array_get_number(array, 1);
    return v;
}

static vec3 _golf_data_json_object_get_vec3(JSON_Object *obj, const char *name) {
    JSON_Array *array = json_object_get_array(obj, name);
    vec3 v;
    v.x = (float)json_array_get_number(array, 0);
    v.y = (float)json_array_get_number(array, 1);
    v.z = (float)json_array_get_number(array, 2);
    return v;
}

static vec4 _golf_data_json_object_get_vec4(JSON_Object *obj, const char *name) {
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


//
// TEXTURES
//

static bool _golf_data_texture_import(const char *path, char *data, int data_len) {
    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    json_object_set_string(obj, "filter", "linear");
    _golf_data_json_object_set_data(obj, "img_data", data, data_len);

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
        _golf_data_json_object_get_data(obj, "img_data", &img_data, &img_data_len);

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

    datafile->type = GOLF_DATA_TEXTURE;
    datafile->texture = malloc(sizeof(golf_data_texture_t));
    datafile->texture->image = image;
    datafile->texture->width = width;
    datafile->texture->height = height;
    return true;
}

static bool _golf_data_texture_unload(golf_data_file_t *data) {
    sg_destroy_image(data->texture->image);
    free(data->texture);
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
        golf_string_init(&cmd, "");
#if GOLF_PLATFORM_LINUX
        golf_string_appendf(&cmd, "tools/sokol-tools/linux/sokol-shdc --input %s --output src/golf/shaders/%s.h --slang %s", file.path, file.name, slangs);
#elif GOLF_PLATFORM_WINDOWS
        golf_string_appendf(&cmd, "tools\\sokol-tools\\win32\\sokol-shdc --input %s --output src/golf/shaders/%s.h --slang %s", file.path, file.name, slangs);
#elif GOLF_PLATFORM_MACOS
#endif
        int ret = system(cmd.cstr);
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

static bool _golf_data_shader_load(const char *path, char *data, int data_len, golf_data_file_t *datafile) {
    const sg_shader_desc *const_shader_desc;
    if (strcmp(path, "data/shaders/ui_sprite.glsl") == 0) {
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

    golf_data_shader_t *shader = malloc(sizeof(golf_data_shader_t));
    shader->shader = sg_make_shader(&shader_desc);

    json_value_free(val);

    datafile->type = GOLF_DATA_SHADER;
    datafile->shader = shader; 
    return true;
}

static bool _golf_data_shader_unload(golf_data_file_t *data) {
    sg_destroy_shader(data->shader->shader);
    free(data->shader);
    return true;
}

//
// FONT
//

static void _stbi_write_func(void *context, void *data, int size) {
    vec_char_t *bmp = (vec_char_t*)context;
    vec_pusharr(bmp, (char*)data, size);
}

static JSON_Value *_golf_data_font_atlas_import(const char *file_data, int font_size, int img_size) {
    unsigned char *bitmap = malloc(img_size * img_size);
    stbtt_bakedchar cdata[96];
    memset(cdata, 0, sizeof(cdata));
    stbtt_BakeFontBitmap(file_data, 0, (float)-font_size, bitmap, img_size, img_size, 32, 95, cdata);

    float ascent, descent, linegap;
    stbtt_GetScaledFontVMetrics(file_data, 0, (float)-font_size, &ascent, &descent, &linegap);

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
        _golf_data_json_object_set_data(obj, "img_data", img.data, img.length);
        vec_deinit(&img);
    }

    free(bitmap);
    return val;
}

bool _golf_data_font_import(const char *path, char *data, int data_len) {
    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    JSON_Value *atlases_val = json_value_init_array();
    JSON_Array *atlases_array = json_value_get_array(atlases_val);

    json_array_append_value(atlases_array, _golf_data_font_atlas_import(data, 16, 256));
    json_array_append_value(atlases_array, _golf_data_font_atlas_import(data, 24, 256));
    json_array_append_value(atlases_array, _golf_data_font_atlas_import(data, 32, 256));
    json_array_append_value(atlases_array, _golf_data_font_atlas_import(data, 40, 512));
    json_array_append_value(atlases_array, _golf_data_font_atlas_import(data, 48, 512));
    json_array_append_value(atlases_array, _golf_data_font_atlas_import(data, 56, 512));
    json_array_append_value(atlases_array, _golf_data_font_atlas_import(data, 64, 512));

    json_object_set_value(obj, "atlases", atlases_val);

    golf_string_t import_font_file_path;
    golf_string_initf(&import_font_file_path, "%s.import", path);
    json_serialize_to_file(val, import_font_file_path.cstr);
    golf_string_deinit(&import_font_file_path);

    json_value_free(val);
    return true;
}

static void _golf_data_font_load_atlas(JSON_Object *atlas_obj, golf_data_font_atlas_t *atlas) {
    atlas->font_size = (float)json_object_get_number(atlas_obj, "font_size");
    atlas->ascent = (float)json_object_get_number(atlas_obj, "ascent");
    atlas->descent = (float)json_object_get_number(atlas_obj, "descent");
    atlas->linegap = (float)json_object_get_number(atlas_obj, "linegap");
    atlas->size = (int)json_object_get_number(atlas_obj, "img_size");

    JSON_Array *char_datas_array = json_object_get_array(atlas_obj, "char_datas");
    for (int i = 0; i < json_array_get_count(char_datas_array); i++) {
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
    _golf_data_json_object_get_data(atlas_obj, "img_data", &img_data, &img_data_len);

    int x, y, n;
    int force_channels = 4;
    stbi_set_flip_vertically_on_load(0);
    unsigned char *stb_data = stbi_load_from_memory(img_data, img_data_len, &x, &y, &n, force_channels);
    if (!stb_data) {
        golf_log_error("STB Failed to load image");
    }

    atlas->image = sg_make_image(&(sg_image_desc) {
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
            },
            });

    free(img_data);
    free(stb_data);
}

bool _golf_data_font_load(const char *path, char *data, int data_len, golf_data_file_t *data_file) {
    JSON_Value *val = json_parse_string(data);
    JSON_Object *obj = json_value_get_object(val);
    if (!val) {
        golf_log_warning("Unable to parse json for font file %s", path);
        return false;
    }

    JSON_Array *atlases_array = json_object_get_array(obj, "atlases");
    golf_data_font_t *font = malloc(sizeof(golf_data_font_t));
    vec_init(&font->atlases);
    for (int i = 0; i < json_array_get_count(atlases_array); i++) {
        JSON_Object *atlas_obj = json_array_get_object(atlases_array, i);
        golf_data_font_atlas_t atlas;
        _golf_data_font_load_atlas(atlas_obj, &atlas);
        vec_push(&font->atlases, atlas);
    }

    json_value_free(val);

    data_file->type = GOLF_DATA_FONT;
    data_file->font = font;
    return true;
}

bool _golf_data_font_unload(golf_data_file_t *data) {
    for (int i = 0; i < data->font->atlases.length; i++) {
        golf_data_font_atlas_t atlas = data->font->atlases.data[i];
        sg_destroy_image(atlas.image);
    }
    vec_deinit(&data->font->atlases);
    free(data->font);
    return true;
}

//
// MODEL
//

bool _golf_data_model_import(const char *path, char *data, int data_len) {
    JSON_Value *vertices_val = json_value_init_array();
    JSON_Array *vertices = json_value_get_array(vertices_val);

    fastObjMesh *m = fast_obj_read(path);
    for (int i = 0; i < (int)m->group_count; i++) {
        const fastObjGroup grp = m->groups[i];

        int idx = 0;
        for (int j = 0; j < (int)grp.face_count; j++) {
            int fv = m->face_vertices[grp.face_offset + j];
            if (fv != 3) {
                golf_log_warning("OBJ file isn't triangulated %s", path); 
                break;
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

    golf_string_t import_model_file_path;
    golf_string_initf(&import_model_file_path, "%s.import", path);
    json_serialize_to_file(val, import_model_file_path.cstr);
    golf_string_deinit(&import_model_file_path);
    json_value_free(val);
    return true;
}

bool _golf_data_model_load(const char *path, char *data, int data_len, golf_data_file_t *data_file) {
    JSON_Value *val = json_parse_string(data);
    JSON_Object *obj = json_value_get_object(val);
    if (!val) {
        golf_log_warning("Unable to parse json model file %s", path);
        return false;
    }

    golf_data_model_t *model = malloc(sizeof(golf_data_model_t));
    vec_init(&model->positions);
    vec_init(&model->normals);
    vec_init(&model->texcoords);

    JSON_Array *json_vertices = json_object_get_array(obj, "vertices");
    for (int i = 0; i < (int)json_array_get_count(json_vertices); i++) {
        JSON_Object *json_vertex = json_array_get_object(json_vertices, i);
        vec3 position = _golf_data_json_object_get_vec3(json_vertex, "position");
        vec3 normal = _golf_data_json_object_get_vec3(json_vertex, "normal");
        vec2 texcoord = _golf_data_json_object_get_vec2(json_vertex, "texcoord");
        vec_push(&model->positions, position);
        vec_push(&model->normals, normal);
        vec_push(&model->texcoords, texcoord);
    }

    json_value_free(val);

    data_file->type = GOLF_DATA_MODEL;
    data_file->model = model;
    return true;
}

bool _golf_data_model_unload(golf_data_file_t *data) {
    vec_deinit(&data->model->positions);
    vec_deinit(&data->model->normals);
    vec_deinit(&data->model->texcoords);
    free(data->model);
    return true;
}

//
// UI PIXEL PACK
//

static void _golf_date_ui_pixel_pack_pos_to_uvs(golf_data_ui_pixel_pack_t *pp, float tex_w, float tex_h, vec2 p, vec2 *uv0, vec2 *uv1) {
    uv0->x = (pp->tile_size + pp->tile_padding) * p.x;
    uv0->x /= tex_w;
    uv0->y = (pp->tile_size + pp->tile_padding) * p.y;
    uv0->y /= tex_h;

    uv1->x = (pp->tile_size + pp->tile_padding) * (p.x + 1) - pp->tile_padding;
    uv1->x /= tex_w;
    uv1->y = (pp->tile_size + pp->tile_padding) * (p.y + 1) - pp->tile_padding;
    uv1->y /= tex_h;
}

static bool _golf_data_ui_pixel_pack_load(const char *path, char *data, int data_len, golf_data_file_t *data_file) {
    JSON_Value *val = json_parse_string(data);
    JSON_Object *obj = json_value_get_object(val);

    golf_data_ui_pixel_pack_t *ui_pixel_pack = malloc(sizeof(golf_data_ui_pixel_pack_t));
    golf_string_init(&ui_pixel_pack->texture, json_object_get_string(obj, "texture"));
    ui_pixel_pack->tile_size = (float)json_object_get_number(obj, "tile_size");
    ui_pixel_pack->tile_padding = (float)json_object_get_number(obj, "tile_padding");
    map_init(&ui_pixel_pack->icons);
    map_init(&ui_pixel_pack->squares);

    golf_data_texture_t *tex = golf_data_get_texture(ui_pixel_pack->texture.cstr);

    JSON_Array *icons_array = json_object_get_array(obj, "icons");
    for (int i = 0; i < (int)json_array_get_count(icons_array); i++) {
        JSON_Object *icon_obj = json_array_get_object(icons_array, i);
        const char *name = json_object_get_string(icon_obj, "name");
        vec2 pos = _golf_data_json_object_get_vec2(icon_obj, "pos");

        golf_data_ui_pixel_pack_icon_t icon;
        _golf_date_ui_pixel_pack_pos_to_uvs(ui_pixel_pack, tex->width, tex->height, pos, &icon.uv0, &icon.uv1);
        map_set(&ui_pixel_pack->icons, name, icon);
    }

    JSON_Array *squares_array = json_object_get_array(obj, "squares");
    for (int i = 0; i < (int)json_array_get_count(squares_array); i++) {
        JSON_Object *square_obj = json_array_get_object(squares_array, i);
        const char *name = json_object_get_string(square_obj, "name");
        vec2 tl = _golf_data_json_object_get_vec2(square_obj, "top_left");
        vec2 tm = _golf_data_json_object_get_vec2(square_obj, "top_mid");
        vec2 tr = _golf_data_json_object_get_vec2(square_obj, "top_right");
        vec2 ml = _golf_data_json_object_get_vec2(square_obj, "mid_left");
        vec2 mm = _golf_data_json_object_get_vec2(square_obj, "mid_mid");
        vec2 mr = _golf_data_json_object_get_vec2(square_obj, "mid_right");
        vec2 bl = _golf_data_json_object_get_vec2(square_obj, "bot_left");
        vec2 bm = _golf_data_json_object_get_vec2(square_obj, "bot_mid");
        vec2 br = _golf_data_json_object_get_vec2(square_obj, "bot_right");

        golf_data_ui_pixel_pack_square_t square;
        _golf_date_ui_pixel_pack_pos_to_uvs(ui_pixel_pack, tex->width, tex->height, tl, &square.tl_uv0, &square.tl_uv1);
        _golf_date_ui_pixel_pack_pos_to_uvs(ui_pixel_pack, tex->width, tex->height, tm, &square.tm_uv0, &square.tm_uv1);
        _golf_date_ui_pixel_pack_pos_to_uvs(ui_pixel_pack, tex->width, tex->height, tr, &square.tr_uv0, &square.tr_uv1);
        _golf_date_ui_pixel_pack_pos_to_uvs(ui_pixel_pack, tex->width, tex->height, ml, &square.ml_uv0, &square.ml_uv1);
        _golf_date_ui_pixel_pack_pos_to_uvs(ui_pixel_pack, tex->width, tex->height, mm, &square.mm_uv0, &square.mm_uv1);
        _golf_date_ui_pixel_pack_pos_to_uvs(ui_pixel_pack, tex->width, tex->height, mr, &square.mr_uv0, &square.mr_uv1);
        _golf_date_ui_pixel_pack_pos_to_uvs(ui_pixel_pack, tex->width, tex->height, bl, &square.bl_uv0, &square.bl_uv1);
        _golf_date_ui_pixel_pack_pos_to_uvs(ui_pixel_pack, tex->width, tex->height, bm, &square.bm_uv0, &square.bm_uv1);
        _golf_date_ui_pixel_pack_pos_to_uvs(ui_pixel_pack, tex->width, tex->height, br, &square.br_uv0, &square.br_uv1);
        map_set(&ui_pixel_pack->squares, name, square);
    }

    json_value_free(val);
    data_file->type = GOLF_DATA_UI_PIXEL_PACK;
    data_file->ui_pixel_pack = ui_pixel_pack;
    return true;
}

static bool _golf_data_ui_pixel_pack_unload(golf_data_file_t *data_file) {
    map_deinit(&data_file->ui_pixel_pack->icons);
    map_deinit(&data_file->ui_pixel_pack->squares);
    free(data_file->ui_pixel_pack);
    return true;
}

//
// DATA
//

void golf_data_init(void) {
    map_init(&_golf_data.loaded_files);
}

golf_data_importer_t _golf_data_get_importer(const char *ext) { 
    if ((strcmp(ext, ".png") == 0) ||
            (strcmp(ext, ".jpg") == 0) ||
            (strcmp(ext, ".bmp") == 0)) {
        return &_golf_data_texture_import;
    }
    else if ((strcmp(ext, ".glsl") == 0)) {
        return &_golf_data_shader_import;
    }
    else if ((strcmp(ext, ".ttf") == 0)) {
        return &_golf_data_font_import;
    }
    else if ((strcmp(ext, ".obj") == 0)) {
        return &_golf_data_model_import;
    }
    else {
        return NULL;
    }
}

golf_data_loader_t _golf_data_get_loader(const char *ext) { 
    if ((strcmp(ext, ".png") == 0) ||
            (strcmp(ext, ".jpg") == 0) ||
            (strcmp(ext, ".bmp") == 0)) {
        return &_golf_data_texture_load;
    }
    else if ((strcmp(ext, ".glsl") == 0)) {
        return &_golf_data_shader_load;
    }
    else if ((strcmp(ext, ".ttf") == 0)) {
        return &_golf_data_font_load;
    }
    else if ((strcmp(ext, ".obj") == 0)) {
        return &_golf_data_model_load;
    }
    else if ((strcmp(ext, ".ui_pixel_pack") == 0)) {
        return &_golf_data_ui_pixel_pack_load;
    }
    else {
        return NULL;
    }
}

golf_data_unloader_t _golf_data_get_unloader(const char *ext) { 
    if ((strcmp(ext, ".png") == 0) ||
            (strcmp(ext, ".jpg") == 0) ||
            (strcmp(ext, ".bmp") == 0)) {
        return &_golf_data_texture_unload;
    }
    else if ((strcmp(ext, ".glsl") == 0)) {
        return &_golf_data_shader_unload;
    }
    else if ((strcmp(ext, ".ttf") == 0)) {
        return &_golf_data_font_unload;
    }
    else if ((strcmp(ext, ".obj") == 0)) {
        return &_golf_data_model_unload;
    }
    else if ((strcmp(ext, ".ui_pixel_pack") == 0)) {
        return &_golf_data_ui_pixel_pack_unload;
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
            golf_file_t import_file = golf_file_append_extension(file.path, ".import");
            if (!force_import && golf_file_cmp_time(&file, &import_file) < 0.0f) {
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

        golf_filetime_t file_time;
        golf_file_get_time(&loaded_file->file, &file_time);
        if (golf_filetime_cmp(loaded_file->last_load_time, file_time) < 0.0f) {
            golf_file_t file = golf_file(key);
            golf_file_t file_to_load = loaded_file->file;
            if (golf_file_load_data(&file_to_load)) {
                golf_log_note("Reloading file %s", key);


                golf_data_loader_t load = _golf_data_get_loader(file.ext);
                golf_data_unloader_t unload = _golf_data_get_unloader(file.ext);
                if (!load || !unload) {
                    golf_log_error("Missing loader or unloader for loaded file %s", file.path);
                }

                unload(loaded_file);
                load(file.path, file_to_load.data, file_to_load.data_len, loaded_file);

                loaded_file->last_load_time = file_time;
                golf_file_free_data(&file_to_load);
            }
            else {
                golf_log_warning("Unable to load file %s", key);
            }
        }
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

    golf_data_importer_t importer = _golf_data_get_importer(file.ext);
    if (importer) {
        file_to_load = golf_file_append_extension(path, ".import");
    }

    if (golf_file_load_data(&file_to_load)) {
        golf_data_loader_t loader = _golf_data_get_loader(file.ext);
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

golf_data_file_t *golf_data_get_file(const char *path) {
    return map_get(&_golf_data.loaded_files, path);
}

golf_data_texture_t *golf_data_get_texture(const char *path) {
    static const char *fallback_texture = "data/textures/fallback.png";

    golf_data_file_t *data_file = golf_data_get_file(path);
    if (!data_file || data_file->type != GOLF_DATA_TEXTURE) {
        golf_log_warning("Could not find texture %s. Using fallback", path);
        data_file = golf_data_get_file(fallback_texture);
        if (!data_file || data_file->type != GOLF_DATA_TEXTURE) {
            golf_log_error("Could not find fallback texture");
        }
    }
    return data_file->texture;
}

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "3rd_party/cimgui/cimgui.h"

void golf_data_debug_console_tab(void) {
    if (igCollapsingHeaderTreeNodeFlags("Textures", ImGuiTreeNodeFlags_None)) {
        const char *key;
        map_iter_t iter = map_iter(&_golf_data.loaded_files);

        while ((key = map_next(&_golf_data.loaded_files, &iter))) {
            golf_data_file_t *loaded_file = map_get(&_golf_data.loaded_files, key);
            if (loaded_file->type != GOLF_DATA_TEXTURE) {
                continue;
            }

            golf_data_texture_t *texture = loaded_file->texture;
            if (igTreeNodeStr(key)) {
                igText("Width: %d", texture->width);
                igText("Height: %d", texture->height);
                igImage((ImTextureID)(intptr_t)texture->image.id, (ImVec2){texture->width, texture->height}, (ImVec2){0, 0}, (ImVec2){1, 1}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 1});
                igTreePop();
            }
        }
    }

    if (igCollapsingHeaderTreeNodeFlags("Fonts", ImGuiTreeNodeFlags_None)) {
        const char *key;
        map_iter_t iter = map_iter(&_golf_data.loaded_files);

        while ((key = map_next(&_golf_data.loaded_files, &iter))) {
            golf_data_file_t *loaded_file = map_get(&_golf_data.loaded_files, key);
            if (loaded_file->type != GOLF_DATA_FONT) {
                continue;
            }

            golf_data_font_t *font = loaded_file->font;
            if (igTreeNodeStr(key)) {
                for (int i = 0; i < font->atlases.length; i++) {
                    golf_data_font_atlas_t *atlas = &font->atlases.data[i];
                    igText("Atlas Size: %d", atlas->size);
                    igImage((ImTextureID)(intptr_t)atlas->image.id, (ImVec2){atlas->size, atlas->size}, (ImVec2){0, 0}, (ImVec2){1, 1}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 1});
                }
                igTreePop();
            }
        }
    }

    if (igCollapsingHeaderTreeNodeFlags("Models", ImGuiTreeNodeFlags_None)) {
        const char *key;
        map_iter_t iter = map_iter(&_golf_data.loaded_files);

        while ((key = map_next(&_golf_data.loaded_files, &iter))) {
            golf_data_file_t *loaded_file = map_get(&_golf_data.loaded_files, key);
            if (loaded_file->type != GOLF_DATA_MODEL) {
                continue;
            }

            golf_data_model_t *model = loaded_file->model;
            if (igTreeNodeStr(key)) {
                if (igTreeNodeStr("Positions: ")) {
                    for (int i = 0; i < model->positions.length; i++) {
                        vec3 position = model->positions.data[i];
                        igText("<%.3f, %.3f, %.3f>", position.x, position.y, position.z);
                    }
                    igTreePop();
                }
                if (igTreeNodeStr("Normals: ")) {
                    for (int i = 0; i < model->normals.length; i++) {
                        vec3 normal = model->normals.data[i];
                        igText("<%.3f, %.3f, %.3f>", normal.x, normal.y, normal.z);
                    }
                    igTreePop();
                }
                if (igTreeNodeStr("Texcoords: ")) {
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

    if (igCollapsingHeaderTreeNodeFlags("Shaders", ImGuiTreeNodeFlags_None)) {
        const char *key;
        map_iter_t iter = map_iter(&_golf_data.loaded_files);

        while ((key = map_next(&_golf_data.loaded_files, &iter))) {
            golf_data_file_t *loaded_file = map_get(&_golf_data.loaded_files, key);
            if (loaded_file->type != GOLF_DATA_SHADER) {
                continue;
            }
            if (igTreeNodeStr(key)) {
                igTreePop();
            }
        }
    }

    if (igCollapsingHeaderTreeNodeFlags("UI Pixel Packs", ImGuiTreeNodeFlags_None)) {
        const char *key;
        map_iter_t iter = map_iter(&_golf_data.loaded_files);

        while ((key = map_next(&_golf_data.loaded_files, &iter))) {
            golf_data_file_t *loaded_file = map_get(&_golf_data.loaded_files, key);
            if (loaded_file->type != GOLF_DATA_UI_PIXEL_PACK) {
                continue;
            }

            golf_data_ui_pixel_pack_t *pp = loaded_file->ui_pixel_pack;
            golf_data_texture_t *t = golf_data_get_texture(pp->texture.cstr);

            if (igTreeNodeStr(key)) {
                if (igTreeNodeStr("Icons")) {
                    const char *icon_key;
                    map_iter_t icons_iter = map_iter(&pp->icons);
                    while ((icon_key = map_next(&pp->icons, &icons_iter))) {
                        if (igTreeNodeStr(icon_key)) {
                            golf_data_ui_pixel_pack_icon_t *i = map_get(&pp->icons, icon_key);
                            igImage((ImTextureID)(intptr_t)t->image.id, (ImVec2){40, 40}, (ImVec2){i->uv0.x, i->uv0.y}, (ImVec2){i->uv1.x, i->uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 1});
                            igTreePop();
                        }
                    }
                    igTreePop();
                }

                if (igTreeNodeStr("Squares")) {
                    const char *square_key;
                    map_iter_t squares_iter = map_iter(&pp->squares);
                    while ((square_key = map_next(&pp->squares, &squares_iter))) {
                        if (igTreeNodeStr(square_key)) {
                            golf_data_ui_pixel_pack_square_t *s = map_get(&pp->squares, square_key);

                            igPushStyleVarVec2(ImGuiStyleVar_ItemSpacing, (ImVec2){0, 0});

                            igImage((ImTextureID)(intptr_t)t->image.id, (ImVec2){40, 40}, (ImVec2){s->tl_uv0.x, s->tl_uv0.y}, (ImVec2){s->tl_uv1.x, s->tl_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
                            igSameLine(0, 0);
                            igImage((ImTextureID)(intptr_t)t->image.id, (ImVec2){40, 40}, (ImVec2){s->tm_uv0.x, s->tm_uv0.y}, (ImVec2){s->tm_uv1.x, s->tm_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
                            igSameLine(0, 0);
                            igImage((ImTextureID)(intptr_t)t->image.id, (ImVec2){40, 40}, (ImVec2){s->tr_uv0.x, s->tr_uv0.y}, (ImVec2){s->tr_uv1.x, s->tr_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});

                            igImage((ImTextureID)(intptr_t)t->image.id, (ImVec2){40, 40}, (ImVec2){s->ml_uv0.x, s->ml_uv0.y}, (ImVec2){s->ml_uv1.x, s->ml_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
                            igSameLine(0, 0);
                            igImage((ImTextureID)(intptr_t)t->image.id, (ImVec2){40, 40}, (ImVec2){s->mm_uv0.x, s->mm_uv0.y}, (ImVec2){s->mm_uv1.x, s->mm_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
                            igSameLine(0, 0);
                            igImage((ImTextureID)(intptr_t)t->image.id, (ImVec2){40, 40}, (ImVec2){s->mr_uv0.x, s->mr_uv0.y}, (ImVec2){s->mr_uv1.x, s->mr_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});

                            igImage((ImTextureID)(intptr_t)t->image.id, (ImVec2){40, 40}, (ImVec2){s->bl_uv0.x, s->bl_uv0.y}, (ImVec2){s->bl_uv1.x, s->bl_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
                            igSameLine(0, 0);
                            igImage((ImTextureID)(intptr_t)t->image.id, (ImVec2){40, 40}, (ImVec2){s->bm_uv0.x, s->bm_uv0.y}, (ImVec2){s->bm_uv1.x, s->bm_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
                            igSameLine(0, 0);
                            igImage((ImTextureID)(intptr_t)t->image.id, (ImVec2){40, 40}, (ImVec2){s->br_uv0.x, s->br_uv0.y}, (ImVec2){s->br_uv1.x, s->br_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});

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
