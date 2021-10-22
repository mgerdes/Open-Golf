#ifndef _MCORE_FONT_H
#define _MCORE_FONT_H

#include <stdbool.h>
#include "3rd_party/sokol/sokol_gfx.h"

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
    sg_image image;
} mdata_font_atlas_t;

typedef struct mdata_font {
    mdata_font_atlas_t atlases[3];
} mdata_font_t;

bool mdata_font_import(const char *path, char *data, int data_len);
bool mdata_font_load(const char *path, char *data, int data_len);
bool mdata_font_unload(const char *path);
bool mdata_font_reload(const char *path, char *data, int data_len);

#endif
