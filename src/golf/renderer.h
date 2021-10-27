#ifndef _GOLF_RENDERER_H
#define _GOLF_RENDERER_H

#include "3rd_party/map/map.h"
#include "3rd_party/sokol/sokol_gfx.h"
#include "3rd_party/vec/vec.h"
#include "golf/maths.h"

typedef map_t(sg_pipeline) map_sg_pipeline_t;

typedef struct golf_renderer {
    vec2 viewport_pos;
    vec2 viewport_size;
    mat4 proj_mat;
    mat4 view_mat;
    mat4 proj_view_mat;
    mat4 ui_proj_mat;
    vec3 cam_pos;
    vec3 cam_dir;
    map_sg_pipeline_t pipelines_map;
} golf_renderer_t;

golf_renderer_t *golf_renderer_get(void);
void golf_renderer_init(void);
void golf_renderer_draw(void);
void golf_renderer_draw_editor(void);

#endif
