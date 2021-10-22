#ifndef _GOLF_RENDERER_H
#define _GOLF_RENDERER_H

#include "3rd_party/map/map.h"
#include "3rd_party/sokol/sokol_gfx.h"
#include "3rd_party/vec/vec.h"
#include "golf/maths.h"

typedef struct golf_renderer {
    mat4 ui_proj_mat;
} golf_renderer_t;

golf_renderer_t *golf_renderer_get(void);
void golf_renderer_init(void);
void golf_renderer_draw(void);

#endif
