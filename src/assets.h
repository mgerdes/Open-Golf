#ifndef _ASSETS_H
#define _ASSETS_H

#include "file.h"
#include "maths.h"
#include "sokol_gfx.h"

struct model {
    char name[FILES_MAX_FILENAME];
    int num_points;
    vec3 *positions, *normals;
    vec2 *texture_coords;
    sg_buffer positions_buf, normals_buf, texture_coords_buf;
};

struct texture {
    char name[FILES_MAX_FILENAME];
    unsigned char *data;
    int width, height;
    sg_image image;
};

struct shader {
    char name[FILES_MAX_FILENAME];
    sg_pipeline pipeline;
    sg_shader shader;
};

void asset_store_init(void);
struct model *asset_store_get_model(const char *name);
struct texture *asset_store_get_texture(const char *name);

#endif
