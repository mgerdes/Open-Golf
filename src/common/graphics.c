#include "common/graphics.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include "stb/stb_image.h"
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_imgui.h"
#include "common/data.h"
#include "common/log.h"
#include "common/maths.h"

static golf_graphics_t graphics;

golf_graphics_t *golf_graphics_get(void) {
    return &graphics;
}

void golf_graphics_init(void) {
    memset(&graphics, 0, sizeof(graphics));

    {
        graphics.window_size = V2((float)sapp_width(), (float)sapp_width());
        graphics.viewport_pos = V2(0, 0);
        graphics.viewport_size = V2((float)sapp_width(), (float)sapp_width());
    }
}

void golf_graphics_begin_frame(float dt) {
    graphics.framerate = igGetIO()->Framerate;
    graphics.window_size = V2((float)sapp_width(), (float)sapp_height());

    sg_pass_action action = {
        .colors[0] = {
            .action = SG_ACTION_CLEAR,
            .value = { 0.0f, 0.0f, 0.0f, 1.0f },
        },
    };
    sg_begin_default_pass(&action, sapp_width(), sapp_height());
    sg_end_pass();
    simgui_new_frame(sapp_width(), sapp_height(), dt);
}

void golf_graphics_update_proj_view_mat(void) {
    float near = 0.1f;
    float far = 150.0f;
    graphics.proj_mat = mat4_perspective_projection(66.0f,
            graphics.viewport_size.x / graphics.viewport_size.y, near, far);
    graphics.view_mat = mat4_look_at(graphics.cam_pos,
            vec3_add(graphics.cam_pos, graphics.cam_dir), graphics.cam_up);
    graphics.proj_view_mat = mat4_multiply(graphics.proj_mat, graphics.view_mat);
}

void golf_graphics_end_frame(void) {
    {
        sg_pass_action imgui_pass_action = {
            .colors[0] = {
                .action = SG_ACTION_DONTCARE,
            },
            .depth = {
                .action = SG_ACTION_CLEAR,
                .value = 1.0f,

            },
        };

        sg_begin_default_pass(&imgui_pass_action, sapp_width(), sapp_height());
        simgui_render();
    }
    sg_end_pass();
    sg_commit();
}

void golf_graphics_set_viewport(vec2 pos, vec2 size) {
    graphics.viewport_pos = pos;
    graphics.viewport_size = size;
}

vec2 golf_graphics_world_to_screen(vec3 world_pos) {
    vec4 t = V4(world_pos.x, world_pos.y, world_pos.z, 1);
    t = vec4_apply_mat(t, graphics.proj_view_mat);
    t = vec4_scale(t, 0.5f / t.w);
    t = vec4_add(t, V4(0.5f, 0.5f, 0, 0));
    t.y = 1 - t.y;
    t.x = t.x * graphics.viewport_size.x;
    t.y = t.y * graphics.viewport_size.y;
    t.x = t.x + graphics.viewport_pos.x;
    t.y = t.y + graphics.viewport_pos.y;
    return V2(t.x, t.y);
}

vec3 golf_graphics_screen_to_world(vec3 screen_point) {
    float near = 0.1f;
    float far = 150.0f;
    float c = (far + near) / (near - far);
    float d = (2.0f * far * near) / (near - far);
    screen_point.x = -1.0f + (2.0f * screen_point.x / graphics.viewport_size.x);
    screen_point.y = -1.0f + (2.0f * screen_point.y / graphics.viewport_size.y);
    float w = d / (screen_point.z + c);
    vec4 screen = V4(screen_point.x * w, screen_point.y * w, screen_point.z * w, w);
    vec4 world = vec4_apply_mat(screen, mat4_inverse(graphics.proj_view_mat));
    return V3(world.x, world.y, world.z);
}

void golf_graphics_debug_console_tab(void) {
    igText("Window size: <%.3f, %.3f>", graphics.window_size.x, graphics.window_size.y); 
    igText("Viewport Pos: <%.3f, %.3f>", graphics.viewport_pos.x, graphics.viewport_pos.y); 
    igText("Viewport Size: <%.3f, %.3f>", graphics.viewport_size.x, graphics.viewport_size.y); 
    igText("Cam Pos: <%.3f, %.3f, %.3f>", graphics.cam_pos.x, graphics.cam_pos.y, graphics.cam_pos.z); 
    igText("Cam Dir: <%.3f, %.3f, %.3f>", graphics.cam_dir.x, graphics.cam_dir.y, graphics.cam_dir.z); 
    igText("Cam Up: <%.3f, %.3f, %.3f>", graphics.cam_up.x, graphics.cam_up.y, graphics.cam_up.z); 
}
