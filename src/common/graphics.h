#ifndef _GOLF_COMMON_GRAPHICS
#define _GOLF_COMMON_GRAPHICS

#include "sokol/sokol_gfx.h"
#include "common/maths.h"

typedef struct golf_graphics {
    float framerate;
    vec2 window_size;
    vec2 viewport_pos, viewport_size;
    vec2 render_size;

    mat4 ui_proj_mat, proj_mat, view_mat, proj_view_mat;
    vec3 cam_pos, cam_dir, cam_up;
    //float cam_azimuth_angle, cam_inclination_angle;

    sg_pipeline render_image_pipeline,
                diffuse_color_material_pipeline,
                environment_material_pipeline,
                solid_color_material_pipeline,
                texture_material_pipeline,
                ui_pipeline,
                hole_pass1_pipeline,
                hole_pass2_pipeline,
                fxaa_pipeline;
} golf_graphics_t;

golf_graphics_t *golf_graphics_get(void);
void golf_graphics_init(void);
void golf_graphics_begin_frame(float dt);
void golf_graphics_update_proj_view_mat(void);
void golf_graphics_end_frame(void);
void golf_graphics_set_viewport(vec2 pos, vec2 size);
void golf_graphics_set_render_size(vec2 size);
vec2 golf_graphics_world_to_screen(vec3 world_pos);
vec3 golf_graphics_screen_to_world(vec3 screen_point);
void golf_graphics_debug_console_tab(void);

bool golf_graphics_get_shader_desc(const char *path, sg_shader_desc *desc);

#endif
