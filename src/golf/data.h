#ifndef _GOLF_DATA_H
#define _GOLF_DATA_H

#include <stdbool.h>
#include "3rd_party/map/map.h"
#include "3rd_party/sokol/sokol_gfx.h"
#include "golf/file.h"
#include "golf/string.h"

typedef struct golf_data_script {
    golf_string_t src;
} golf_data_script_t;

typedef struct golf_data_texture {
    int width, height;
    sg_image image;
} golf_data_texture_t;

typedef struct golf_data_font_atlas_char_data {
    float x0, y0, x1, y1;
    float xoff, yoff, xadvance;
} golf_data_font_atlas_char_data_t;

typedef struct golf_data_font_atlas {
    int size;
    float ascent, descent, linegap, font_size;
    golf_data_font_atlas_char_data_t char_data[256];
    sg_image image;
} golf_data_font_atlas_t;

typedef vec_t(golf_data_font_atlas_t) vec_golf_data_font_atlas_t;

typedef struct golf_data_font {
    vec_golf_data_font_atlas_t atlases;
} golf_data_font_t;

typedef enum golf_data_file_type {
    GOLF_DATA_SCRIPT,
    GOLF_DATA_TEXTURE,
    GOLF_DATA_FONT,
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
        golf_data_font_t *font;
    };
} golf_data_file_t;

typedef map_t(golf_data_file_t) map_golf_data_file_t;

void golf_data_init(void);
void golf_data_run_import(bool force_import);
void golf_data_update(float dt);
void golf_data_load_file(const char *path);
void golf_data_unload_file(const char *path);
void golf_data_debug_console_tab(void);

golf_data_script_t *golf_data_get_script(const char *path);

#endif

