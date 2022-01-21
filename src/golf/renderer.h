#ifndef _GOLF_RENDERER_H
#define _GOLF_RENDERER_H

#include "sokol/sokol_gfx.h"
#include "golf/maths.h"

typedef struct golf_renderer {
    vec2 window_size;
    vec2 viewport_pos, viewport_size;
    vec2 render_size;

    mat4 ui_proj_mat, proj_mat, view_mat, proj_view_mat;
    float cam_azimuth_angle, cam_inclination_angle;
    vec3 cam_pos, cam_dir, cam_up;

    vec2 render_pass_size;
    bool render_pass_inited;
    sg_image render_pass_image, render_pass_depth_image;
    sg_pass render_pass;

    sg_pipeline render_image_pipeline,
                diffuse_color_material_pipeline,
                environment_material_pipeline,
                solid_color_material_pipeline,
                texture_material_pipeline,
                ui_sprites_pipeline,
                hole_pass1_pipeline,
                hole_pass2_pipeline;
} golf_renderer_t;

golf_renderer_t *golf_renderer_get(void);
void golf_renderer_init(void);
void golf_renderer_begin_frame(float dt);
void golf_renderer_end_frame(void);
void golf_renderer_set_viewport(vec2 pos, vec2 size);
void golf_renderer_set_render_size(vec2 size);
void golf_renderer_update(void);
void golf_renderer_draw(void);
void golf_renderer_draw_editor(void);
vec2 golf_renderer_world_to_screen(vec3 pos);
vec3 golf_renderer_screen_to_world(vec3 screen_point);
void golf_renderer_debug_console_tab(void);

#endif
