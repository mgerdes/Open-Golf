#ifndef _ASSETS_H
#define _ASSETS_H

#include "3rd_party/sokol/sokol_gfx.h"

#include "golf/mfile.h"
#include "golf/maths.h"

struct model {
    char name[MFILE_MAX_NAME];
    int num_points;
    vec3 *positions, *normals;
    vec2 *texture_coords;
    sg_buffer positions_buf, normals_buf, texture_coords_buf;
};

struct texture {
    char name[MFILE_MAX_NAME];
    unsigned char *data;
    int width, height;
    sg_image image;
};

struct shader {
    char name[MFILE_MAX_NAME];
    sg_pipeline pipeline;
    sg_shader shader;
};

void asset_store_init(void);
struct model *asset_store_get_model(const char *name);
struct texture *asset_store_get_texture(const char *name);

#endif
