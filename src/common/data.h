#ifndef _GOLF_DATA_H
#define _GOLF_DATA_H

#include <stdbool.h>
#include "sokol/sokol_gfx.h"
#include "common/file.h"
#include "common/map.h"
#include "common/maths.h"
#include "common/script.h"
#include "common/string.h"

#define GOLF_MAX_NAME_LEN 64

typedef struct golf_gif_texture {
    sg_filter filter;
    int width, height;
    unsigned char *image_data;

    int num_frames;
    int *delays;
    sg_image *sg_images;
} golf_gif_texture_t;

typedef struct golf_texture {
    sg_filter filter;
    int width, height;
    unsigned char *image_data;
    sg_image sg_image;
} golf_texture_t;

typedef struct golf_font_atlas_char_data {
    float x0, y0, x1, y1;
    float xoff, yoff, xadvance;
} golf_font_atlas_char_data_t;

typedef struct golf_font_atlas {
    int size;
    float ascent, descent, linegap, font_size;
    golf_font_atlas_char_data_t char_data[256];
    unsigned char *image_data;
    sg_image sg_image;
} golf_font_atlas_t;
typedef vec_t(golf_font_atlas_t) vec_golf_font_atlas_t;

typedef struct golf_font {
    vec_golf_font_atlas_t atlases;
} golf_font_t;

typedef struct golf_model_group {
    char material_name[GOLF_MAX_NAME_LEN];
    int start_vertex;
    int vertex_count;
} golf_model_group_t;
typedef vec_t(golf_model_group_t) vec_golf_group_t;
golf_model_group_t golf_model_group(const char *material_name, int start_vertex, int vertex_count);

typedef struct golf_model {
    vec_golf_group_t groups;
    vec_vec3_t positions;
    vec_vec3_t normals;
    vec_vec2_t texcoords;

    int sg_size;
    sg_buffer sg_positions_buf, sg_normals_buf, sg_texcoords_buf;
} golf_model_t;
typedef vec_t(golf_model_t) vec_golf_model_t;
golf_model_t golf_model_dynamic(vec_golf_group_t groups, vec_vec3_t positions, vec_vec3_t normals, vec_vec2_t texcoords);
void golf_model_dynamic_finalize(golf_model_t *model);
void golf_model_dynamic_update_sg_buf(golf_model_t *model);
//void golf_model_init(golf_model_t *model, int size);
//void golf_model_update_buf(golf_model_t *model);

typedef struct golf_shader {
    char *fs, *vs;
    sg_shader_desc shader_desc;
    sg_shader sg_shader;
} golf_shader_t;

typedef struct golf_pixel_pack_icon {
    vec2 uv0, uv1;
} golf_pixel_pack_icon_t;
typedef map_t(golf_pixel_pack_icon_t) map_golf_pixel_pack_icon_t;

typedef struct golf_pixel_pack_square {
    vec2 tl_uv0, tm_uv0, tr_uv0, tl_uv1, tm_uv1, tr_uv1;
    vec2 ml_uv0, mm_uv0, mr_uv0, ml_uv1, mm_uv1, mr_uv1;
    vec2 bl_uv0, bm_uv0, br_uv0, bl_uv1, bm_uv1, br_uv1;
} golf_pixel_pack_square_t;
typedef map_t(golf_pixel_pack_square_t) map_golf_pixel_pack_square_t;

typedef struct golf_pixel_pack {
    golf_texture_t *texture;
    float tile_size;
    float tile_padding;
    map_golf_pixel_pack_icon_t icons;
    map_golf_pixel_pack_square_t squares;
} golf_pixel_pack_t;

typedef enum golf_config_property_type {
    GOLF_CONFIG_PROPERTY_NUM,
    GOLF_CONFIG_PROPERTY_STRING,
    GOLF_CONFIG_PROPERTY_VEC2,
    GOLF_CONFIG_PROPERTY_VEC3,
    GOLF_CONFIG_PROPERTY_VEC4,
} golf_config_property_type_t;

typedef struct golf_config_property {
    golf_config_property_type_t type;
    union {
        float num_val;
        char *string_val;
        vec2 vec2_val;
        vec3 vec3_val;
        vec4 vec4_val;
    };
} golf_config_property_t;
typedef map_t(golf_config_property_t) map_golf_config_property_t;

typedef struct golf_config {
    map_golf_config_property_t properties;
} golf_config_t;

float golf_config_get_num(golf_config_t *cfg, const char *name);
const char *golf_config_get_string(golf_config_t *cfg, const char *name);
vec2 golf_config_get_vec2(golf_config_t *cfg, const char *name);
vec3 golf_config_get_vec3(golf_config_t *cfg, const char *name);
vec4 golf_config_get_vec4(golf_config_t *cfg, const char *name);

#define CFG_NUM(cfg, name) golf_config_get_num(cfg, name)
#define CFG_STRING(cfg, name) golf_config_get_string(cfg, name)
#define CFG_VEC2(cfg, name) golf_config_get_vec2(cfg, name)
#define CFG_VEC3(cfg, name) golf_config_get_vec3(cfg, name)
#define CFG_VEC4(cfg, name) golf_config_get_vec4(cfg, name)

typedef struct golf_ui_layout golf_ui_layout_t;
typedef struct golf_level golf_level_t;

typedef struct golf_static_data {
    vec_str_t data_paths;
} golf_static_data_t;

typedef struct golf_level golf_level_t;

typedef enum golf_ui_layout_entity_type {
    GOLF_UI_PIXEL_PACK_SQUARE,
    GOLF_UI_TEXT,
    GOLF_UI_BUTTON,
    GOLF_UI_GIF_TEXTURE,
    GOLF_UI_AIM_CIRCLE,
} golf_ui_layout_entity_type;

typedef struct golf_ui_layout_entity golf_ui_layout_entity_t;
typedef vec_t(golf_ui_layout_entity_t) vec_golf_ui_layout_entity_t;

typedef struct golf_ui_layout_entity {
    golf_ui_layout_entity_type type;
    char name[GOLF_MAX_NAME_LEN], parent_name[GOLF_MAX_NAME_LEN];
    vec2 pos, size, anchor;
    union {
        struct {
            golf_pixel_pack_t *pixel_pack; 
            char square_name[GOLF_MAX_NAME_LEN];
            float tile_size;
            vec4 overlay_color;
        } pixel_pack_square;

        struct {
            golf_font_t *font;
            golf_string_t text;
            float font_size;
            int horiz_align, vert_align;
            vec4 color;
        } text;

        struct {
            vec_golf_ui_layout_entity_t up_entities, down_entities;
        } button;

        struct {
            float t, total_time;
            golf_gif_texture_t *texture;
        } gif_texture;

        struct {
            float t, total_time;
            int num_squares;
            vec2 square_size;
            golf_texture_t *texture;
        } aim_circle;
    };
} golf_ui_layout_entity_t;

typedef struct golf_ui_layout {
    vec_golf_ui_layout_entity_t entities;
} golf_ui_layout_t;

typedef enum golf_data_type {
    GOLF_DATA_TEXTURE,
    GOLF_DATA_GIF_TEXTURE,
    GOLF_DATA_FONT,
    GOLF_DATA_MODEL,
    GOLF_DATA_SHADER,
    GOLF_DATA_PIXEL_PACK,
    GOLF_DATA_CONFIG,
    GOLF_DATA_LEVEL,
    GOLF_DATA_STATIC_DATA,
    GOLF_DATA_SCRIPT,
    GOLF_DATA_UI_LAYOUT,
} golf_data_type_t;

typedef struct golf_data {
    bool is_loaded;
    int load_count;
    golf_file_t file;

    golf_data_type_t type; 
    void *ptr;
} golf_data_t;
typedef map_t(golf_data_t) map_golf_data_t;

typedef enum golf_data_load_state {
    GOLF_DATA_UNLOADED,
    GOLF_DATA_LOADING,
    GOLF_DATA_LOADED,
} golf_data_load_state_t;

void golf_data_turn_off_reload(const char *ext);
void golf_data_init(void);
void golf_data_update(float dt);
void golf_data_load(const char *path, bool load_async);
golf_data_load_state_t golf_data_get_load_state(const char *path);
void golf_data_unload(const char *path);
void golf_data_debug_console_tab(void);
void golf_data_get_all_matching(golf_data_type_t type, const char *str, vec_golf_file_t *files);
void golf_data_force_remount(void);

golf_gif_texture_t *golf_data_get_gif_texture(const char *path);
golf_texture_t *golf_data_get_texture(const char *path);
golf_pixel_pack_t *golf_data_get_pixel_pack(const char *path);
golf_model_t *golf_data_get_model(const char *path);
golf_shader_t *golf_data_get_shader(const char *path);
golf_font_t *golf_data_get_font(const char *path);
golf_config_t *golf_data_get_config(const char *path);
golf_level_t *golf_data_get_level(const char *path);
golf_script_t *golf_data_get_script(const char *path);
golf_ui_layout_t *golf_data_get_ui_layout(const char *path);

#endif

