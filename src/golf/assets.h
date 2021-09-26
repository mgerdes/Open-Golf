#ifndef _ASSETS_H
#define _ASSETS_H

#include "3rd_party/sokol/sokol_gfx.h"

#include "mcore/maths.h"

struct model {
    char name[1024];
    int num_points;
    vec3 *positions, *normals;
    vec2 *texture_coords;
    sg_buffer positions_buf, normals_buf, texture_coords_buf;
};

void asset_store_init(void);
struct model *asset_store_get_model(const char *name);

#endif
