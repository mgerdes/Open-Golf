#ifndef _GOLF_RENDERER_H
#define _GOLF_RENDERER_H

#include "map/map.h"
#include "sokol/sokol_gfx.h"
#include "vec/vec.h"
#include "golf/maths.h"

typedef map_t(sg_pipeline) map_sg_pipeline_t;

typedef struct golf_renderer {
    vec2 viewport_pos, viewport_size;
    mat4 ui_proj_mat, proj_mat, view_mat, proj_view_mat;
    float cam_azimuth_angle, cam_inclination_angle;
    vec3 cam_pos, cam_dir, cam_up;

    sg_pipeline environment_pipeline,
                solid_color_material_pipeline,
                ui_sprites_pipeline;
} golf_renderer_t;

golf_renderer_t *golf_renderer_get(void);
void golf_renderer_init(void);
void golf_renderer_draw(void);
void golf_renderer_draw_editor(void);

#endif
