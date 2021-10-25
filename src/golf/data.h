#ifndef _GOLF_DATA_H
#define _GOLF_DATA_H

#include <stdbool.h>
#include "3rd_party/map/map.h"
#include "3rd_party/sokol/sokol_gfx.h"
#include "golf/file.h"
#include "golf/maths.h"
#include "golf/string.h"

typedef struct golf_data_lua_script {
    golf_string_t src;
} golf_data_lua_script_t;

typedef struct golf_data_texture {
    int width, height;
    sg_image sg_image;
} golf_data_texture_t;

typedef struct golf_data_font_atlas_char_data {
    float x0, y0, x1, y1;
    float xoff, yoff, xadvance;
} golf_data_font_atlas_char_data_t;

typedef struct golf_data_font_atlas {
    int size;
    float ascent, descent, linegap, font_size;
    golf_data_font_atlas_char_data_t char_data[256];
    sg_image sg_image;
} golf_data_font_atlas_t;
typedef vec_t(golf_data_font_atlas_t) vec_golf_data_font_atlas_t;

typedef struct golf_data_font {
    vec_golf_data_font_atlas_t atlases;
} golf_data_font_t;

typedef struct golf_data_model {
    vec_vec3_t positions;
    vec_vec3_t normals;
    vec_vec2_t texcoords;
    sg_buffer sg_positions_buf, sg_normals_buf, sg_texcoords_buf;
} golf_data_model_t;

typedef struct golf_data_shader {
    sg_shader sg_shader;
} golf_data_shader_t;

typedef struct golf_data_pixel_pack_icon {
    vec2 uv0, uv1;
} golf_data_pixel_pack_icon_t;
typedef map_t(golf_data_pixel_pack_icon_t) map_golf_data_pixel_pack_icon_t;

typedef struct golf_data_pixel_pack_square {
    vec2 tl_uv0, tm_uv0, tr_uv0, tl_uv1, tm_uv1, tr_uv1;
    vec2 ml_uv0, mm_uv0, mr_uv0, ml_uv1, mm_uv1, mr_uv1;
    vec2 bl_uv0, bm_uv0, br_uv0, bl_uv1, bm_uv1, br_uv1;
} golf_data_pixel_pack_square_t;
typedef map_t(golf_data_pixel_pack_square_t) map_golf_data_pixel_pack_square_t;

typedef struct golf_data_pixel_pack {
    golf_data_texture_t *texture;
    float tile_size;
    float tile_padding;
    map_golf_data_pixel_pack_icon_t icons;
    map_golf_data_pixel_pack_square_t squares;
} golf_data_pixel_pack_t;

typedef enum golf_data_config_property_type {
    GOLF_DATA_CONFIG_PROPERTY_NUM,
    GOLF_DATA_CONFIG_PROPERTY_STRING,
    GOLF_DATA_CONFIG_PROPERTY_VEC2,
    GOLF_DATA_CONFIG_PROPERTY_VEC3,
    GOLF_DATA_CONFIG_PROPERTY_VEC4,
} golf_data_config_property_type_t;

typedef struct golf_data_config_property {
    golf_data_config_property_type_t type;
    union {
        float num_val;
        char *string_val;
        vec2 vec2_val;
        vec3 vec3_val;
        vec4 vec4_val;
    };
} golf_data_config_property_t;
typedef map_t(golf_data_config_property_t) map_golf_data_config_property_t;

typedef struct golf_data_config {
    map_golf_data_config_property_t properties;
} golf_data_config_t;

float golf_data_config_get_num(golf_data_config_t *cfg, const char *name);
const char *golf_data_config_get_string(golf_data_config_t *cfg, const char *name);
vec2 golf_data_config_get_vec2(golf_data_config_t *cfg, const char *name);
vec3 golf_data_config_get_vec3(golf_data_config_t *cfg, const char *name);
vec4 golf_data_config_get_vec4(golf_data_config_t *cfg, const char *name);

#define CFG_NUM(cfg, name) golf_data_config_get_num(cfg, name);
#define CFG_STRING(cfg, name) golf_data_config_get_string(cfg, name);
#define CFG_VEC2(cfg, name) golf_data_config_get_vec2(cfg, name);
#define CFG_VEC3(cfg, name) golf_data_config_get_vec3(cfg, name);
#define CFG_VEC4(cfg, name) golf_data_config_get_vec4(cfg, name);

typedef enum golf_data_file_type {
    GOLF_DATA_LUA_SCRIPT,
    GOLF_DATA_TEXTURE,
    GOLF_DATA_FONT,
    GOLF_DATA_MODEL,
    GOLF_DATA_SHADER,
    GOLF_DATA_PIXEL_PACK,
    GOLF_DATA_CONFIG,
} golf_data_file_type_t;

typedef struct golf_data_file {
    int load_count;
    golf_file_t file;
    golf_file_t file_to_load;
    golf_filetime_t last_load_time;

    golf_data_file_type_t type; 
    union {
        golf_data_lua_script_t *lua_script;
        golf_data_texture_t *texture;
        golf_data_font_t *font;
        golf_data_model_t *model;
        golf_data_shader_t *shader;
        golf_data_pixel_pack_t *pixel_pack;
        golf_data_config_t *config;
    };
} golf_data_file_t;

typedef map_t(golf_data_file_t) map_golf_data_file_t;

void golf_data_init(void);
void golf_data_run_import(bool force_import);
void golf_data_update(float dt);
void golf_data_load_file(const char *path);
void golf_data_unload_file(const char *path);
void golf_data_debug_console_tab(void);

golf_data_file_t *golf_data_get_file(const char *path);
golf_data_texture_t *golf_data_get_texture(const char *path);
golf_data_pixel_pack_t *golf_data_get_pixel_pack(const char *path);
golf_data_model_t *golf_data_get_model(const char *path);
golf_data_shader_t *golf_data_get_shader(const char *path);
golf_data_font_t *golf_data_get_font(const char *path);
golf_data_config_t *golf_data_get_config(const char *path);

#endif

