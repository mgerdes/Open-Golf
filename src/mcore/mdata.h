#ifndef _MCORE_MDATA_H
#define _MCORE_MDATA_H

#include "3rd_party/map/map.h"
#include "mcore/maths.h"
#include "mcore/mfile.h"

//
// TEXTURE
//

typedef struct mdata_texture_t {
    void *json_val;
    const char *filter;
    unsigned char *data;
    int data_len;
} mdata_texture_t;

void mdata_texture_import(mfile_t *file);
mdata_texture_t *mdata_texture_load(const char *path);
void mdata_texture_free(mdata_texture_t *data);

//
// CONFIG
//

typedef enum mdata_config_property_type {
    MDATA_CONFIG_PROPERTY_STRING,
    MDATA_CONFIG_PROPERTY_NUMBER,
    MDATA_CONFIG_PROPERTY_VEC2,
    MDATA_CONFIG_PROPERTY_VEC3,
    MDATA_CONFIG_PROPERTY_VEC4,
} mdata_config_property_type_t;

typedef struct mdata_config_property {
    mdata_config_property_type_t type; 
    const char *name;
    union {
        const char *string_val;
        float number_val;
        vec2 vec2_val;
        vec3 vec3_val;
        vec4 vec4_val;
    };
} mdata_config_property_t;

typedef vec_t(mdata_config_property_t) vec_mdata_config_property_t;
typedef map_t(mdata_config_property_t) map_mdata_config_property_t;

typedef struct mdata_config {
    void *json_val;
	vec_mdata_config_property_t properties;
} mdata_config_t;

void mdata_config_import(mfile_t *file);
mdata_config_t *mdata_config_load(const char *path);
void mdata_config_free(mdata_config_t *data);

//
// SHADER
//

typedef struct mdata_shader {
    void *json_val;
    struct {
        const char *fs, *vs;
    } glsl300es;
    struct {
        const char *fs, *vs;
    } glsl330;
} mdata_shader_t;

void mdata_shader_import(mfile_t *file);
mdata_shader_t *mdata_shader_load(const char *path);
void mdata_shader_free(mdata_shader_t *data);

//
// FONT
//

typedef struct mdata_font_atlas_char_data {
    float x0, y0, x1, y1;
    float xoff, yoff, xadvance;
} mdata_font_atlas_char_data_t;

typedef struct mdata_font_atlas {
    int bmp_size;
    int bmp_data_len;
    unsigned char *bmp_data;
    float ascent, descent, linegap, font_size;
    mdata_font_atlas_char_data_t char_data[256];
} mdata_font_atlas_t;

typedef struct mdata_font {
    void *json_val;
    mdata_font_atlas_t atlases[3];
} mdata_font_t;

void mdata_font_import(mfile_t *file);
mdata_font_t *mdata_font_load(const char *path);
void mdata_font_free(mdata_font_t *data);

//
// MODEL
//

typedef struct mdata_model {
    void *json_val;
    vec_vec3_t positions;
    vec_vec2_t texcoords;
    vec_vec3_t normals;
} mdata_model_t;

void mdata_model_import(mfile_t *file);
mdata_model_t *mdata_model_load(const char *path);
void mdata_model_free(mdata_model_t *data);

//
// UI PIXEL PACK
//

typedef struct mdata_ui_pixel_pack_square {
	const char *name;
	vec2 tl, tm, tr;
	vec2 ml, mm, mr;
	vec2 bl, bm, br;
} mdata_ui_pixel_pack_square_t;

typedef vec_t(mdata_ui_pixel_pack_square_t) vec_mdata_ui_pixel_pack_square_t;

typedef struct mdata_ui_pixel_pack_icon {
	const char *name;
	int x, y;
} mdata_ui_pixel_pack_icon_t;

typedef vec_t(mdata_ui_pixel_pack_icon_t) vec_mdata_ui_pixel_pack_icon_t;

typedef struct mdata_ui_pixel_pack {
	void *json_val;
	const char *texture;
	int tile_size;
	int tile_padding;
	vec_mdata_ui_pixel_pack_square_t squares;
	vec_mdata_ui_pixel_pack_icon_t icons;
} mdata_ui_pixel_pack_t;

typedef map_t(mdata_ui_pixel_pack_t*) map_mdata_ui_pixel_pack_t;

void mdata_ui_pixel_pack_import(mfile_t *file);
mdata_ui_pixel_pack_t *mdata_ui_pixel_pack_load(const char *path);
void mdata_ui_pixel_pack_free(mdata_ui_pixel_pack_t *data);

//
// MDATA
//

typedef enum mdata_type {
	MDATA_SHADER,
	MDATA_TEXTURE,
	MDATA_FONT,
	MDATA_MODEL,
	MDATA_CONFIG,
	MDATA_UI_PIXEL_PACK,
} mdata_type_t;

typedef struct mdata {
	int load_count;
    const char *path;
	mdata_type_t type;
	union { 
		mdata_shader_t *shader;
		mdata_texture_t *texture;
		mdata_font_t *font;
		mdata_model_t *model;
		mdata_config_t *config;
		mdata_ui_pixel_pack_t *ui_pixel_pack;
	};
} mdata_t;

typedef map_t(mdata_t) map_mdata_t;

typedef struct mdata_loader {
	mdata_type_t type;
	bool(*load)(const char *path, mdata_t data);
	bool(*unload)(const char *path, mdata_t data);
} mdata_loader_t;

typedef vec_t(mdata_loader_t) vec_mdata_loader_t;

void mdata_init(void);
void mdata_run_import(void);
void mdata_add_loader(mdata_type_t type, bool(*load)(const char *path, mdata_t data), bool(*unload)(const char *path, mdata_t data));
void mdata_load_file(const char *path);
void mdata_unload_file(const char *path);

#endif

