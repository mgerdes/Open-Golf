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

#include "golf/shaders/diffuse_color_material.glsl.h"
#include "golf/shaders/environment_material.glsl.h"
#include "golf/shaders/pass_through.glsl.h"
#include "golf/shaders/solid_color_material.glsl.h"
#include "golf/shaders/texture_material.glsl.h"
#include "golf/shaders/ui.glsl.h"
#include "golf/shaders/render_image.glsl.h"

static golf_graphics_t graphics;

golf_graphics_t *golf_graphics_get(void) {
    return &graphics;
}

void golf_graphics_init(void) {
    memset(&graphics, 0, sizeof(graphics));

    {
        graphics.window_size = V2(sapp_width(), sapp_width());
        graphics.viewport_pos = V2(0, 0);
        graphics.viewport_size = V2(sapp_width(), sapp_width());
        graphics.render_size = V2(sapp_width(), sapp_width());
        graphics.render_pass_size = V2(sapp_width(), sapp_width());
        graphics.render_pass_inited = false;

        graphics.cam_azimuth_angle = 0.5f*MF_PI;
        graphics.cam_inclination_angle = 0;
        graphics.cam_pos = V3(5, 5, 5);
        graphics.cam_dir = vec3_normalize(V3(-5, -5, -5));
        float x = graphics.cam_dir.x;
        float y = graphics.cam_dir.y;
        float z = graphics.cam_dir.z;
        graphics.cam_inclination_angle = acosf(y);
        graphics.cam_azimuth_angle = atan2f(z, x);
        if (x < 0) {
            //graphics.cam_inclination_angle += MF_PI;
        }
        float theta = graphics.cam_inclination_angle;
        float phi = graphics.cam_azimuth_angle;
        graphics.cam_dir.x = sinf(theta) * cosf(phi);
        graphics.cam_dir.y = cosf(theta);
        graphics.cam_dir.z = sinf(theta) * sinf(phi);
        graphics.cam_up = V3(0, 1, 0);
    }

    {
        golf_shader_t *shader = golf_data_get_shader("data/shaders/render_image.glsl");
        sg_pipeline_desc pipeline_desc = {
            .shader = shader->sg_shader,
            .layout = {
                .attrs = {
                    [ATTR_render_image_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_render_image_vs_texture_coord] 
                        = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                },
            },
        };
        graphics.render_image_pipeline = sg_make_pipeline(&pipeline_desc);
    }

    {
        golf_shader_t *shader = golf_data_get_shader("data/shaders/diffuse_color_material.glsl");
        sg_pipeline_desc pipeline_desc = {
            .shader = shader->sg_shader,
            .layout = {
                .attrs = {
                    [ATTR_diffuse_color_material_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_diffuse_color_material_vs_normal] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 1 },
                },
            },
            .depth = {
                .compare = SG_COMPAREFUNC_LESS_EQUAL,
                .write_enabled = true,
            },
        };
        graphics.diffuse_color_material_pipeline = sg_make_pipeline(&pipeline_desc);
    }

    {
        golf_shader_t *shader = golf_data_get_shader("data/shaders/environment_material.glsl");
        sg_pipeline_desc pipeline_desc = {
            .shader = shader->sg_shader,
            .layout = {
                .attrs = {
                    [ATTR_environment_material_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_environment_material_vs_texturecoord] 
                        = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                    [ATTR_environment_material_vs_normal] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 2 },
                    [ATTR_environment_material_vs_lightmap_uv] 
                        = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 3 },
                },
            },
            .depth = {
                .compare = SG_COMPAREFUNC_LESS_EQUAL,
                .write_enabled = true,
            },
        };
        graphics.environment_material_pipeline = sg_make_pipeline(&pipeline_desc);
    }

    {
        golf_shader_t *shader = golf_data_get_shader("data/shaders/solid_color_material.glsl");
        sg_pipeline_desc pipeline_desc = {
            .shader = shader->sg_shader,
            .layout = {
                .attrs = {
                    [ATTR_solid_color_material_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                },
            },
            .depth = {
                .compare = SG_COMPAREFUNC_LESS_EQUAL,
                .write_enabled = true,
            },
            .colors[0] = {
                .blend = {
                    .enabled = true,
                    .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                    .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                }
            }
        };
        graphics.solid_color_material_pipeline = sg_make_pipeline(&pipeline_desc);
    }

    {
        golf_shader_t *shader = golf_data_get_shader("data/shaders/texture_material.glsl");
        sg_pipeline_desc pipeline_desc = {
            .shader = shader->sg_shader,
            .layout = {
                .attrs = {
                    [ATTR_texture_material_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_texture_material_vs_texturecoord] 
                        = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                    [ATTR_texture_material_vs_normal] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 2 },
                },
            },
            .depth = {
                .compare = SG_COMPAREFUNC_LESS_EQUAL,
                .write_enabled = true,
            },
        };
        graphics.texture_material_pipeline = sg_make_pipeline(&pipeline_desc);
    }

    {
        golf_shader_t *shader = golf_data_get_shader("data/shaders/ui.glsl");
        sg_pipeline_desc pipeline_desc = {
            .shader = shader->sg_shader,
            .layout = {
                .attrs = {
                    [ATTR_ui_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_ui_vs_texture_coord] 
                        = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                },
            },
            .depth = {
                .compare = SG_COMPAREFUNC_ALWAYS
            },
            .colors[0] = {
                .blend = {
                    .enabled = true,
                    .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                    .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                },
            },
        };
        graphics.ui_pipeline = sg_make_pipeline(&pipeline_desc);
    }

    {
        golf_shader_t *shader = golf_data_get_shader("data/shaders/pass_through.glsl");
        sg_pipeline_desc pipeline_desc = {
            .shader = shader->sg_shader,
            .layout = {
                .attrs = {
                    [ATTR_pass_through_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                },
            },
            .depth = {
                .compare = SG_COMPAREFUNC_LESS_EQUAL
            },
            .stencil = {
                .enabled = true,
                .front = {
                    .compare = SG_COMPAREFUNC_ALWAYS,
                    .depth_fail_op = SG_STENCILOP_KEEP,
                    .pass_op = SG_STENCILOP_REPLACE,
                },
                .back = {
                    .compare = SG_COMPAREFUNC_ALWAYS,
                    .depth_fail_op = SG_STENCILOP_KEEP,
                    .pass_op = SG_STENCILOP_REPLACE,
                },
                .write_mask = 255,
                .ref = 255,
            },
            .colors[0] = {
                .write_mask = SG_COLORMASK_NONE,
            },
        };
        graphics.hole_pass1_pipeline = sg_make_pipeline(&pipeline_desc);
    }

    {
        golf_shader_t *shader = golf_data_get_shader("data/shaders/texture_material.glsl");
        sg_pipeline_desc pipeline_desc = {
            .shader = shader->sg_shader,
            .layout = {
                .attrs = {
                    [ATTR_texture_material_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_texture_material_vs_texturecoord] 
                        = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                    [ATTR_texture_material_vs_normal] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 2 },
                },
            },
            .stencil = {
                .enabled = true,
                .front = {
                    .compare = SG_COMPAREFUNC_EQUAL,
                },
                .back = {
                    .compare = SG_COMPAREFUNC_EQUAL,
                },
                .read_mask = 255,
                .ref = 255,
            },
        };
        graphics.hole_pass2_pipeline = sg_make_pipeline(&pipeline_desc);
    }
}

void golf_graphics_begin_frame(float dt) {
    graphics.framerate = igGetIO()->Framerate;
    graphics.window_size = V2((float)sapp_width(), (float)sapp_height());

    {
        float theta = graphics.cam_inclination_angle;
        float phi = graphics.cam_azimuth_angle;
        graphics.cam_dir.x = sinf(theta) * cosf(phi);
        graphics.cam_dir.y = cosf(theta);
        graphics.cam_dir.z = sinf(theta) * sinf(phi);

        float near = 0.1f;
        float far = 150.0f;
        graphics.proj_mat = mat4_perspective_projection(66.0f,
                graphics.viewport_size.x / graphics.viewport_size.y, near, far);
        graphics.view_mat = mat4_look_at(graphics.cam_pos,
                vec3_add(graphics.cam_pos, graphics.cam_dir), graphics.cam_up);
        graphics.proj_view_mat = mat4_multiply(graphics.proj_mat, graphics.view_mat);
    }

    if (!graphics.render_pass_inited || !vec2_equal(graphics.render_size, graphics.render_pass_size)) {
        graphics.render_pass_size = graphics.render_size;

        if (graphics.render_pass_inited) {
            sg_destroy_image(graphics.render_pass_image);
            sg_destroy_image(graphics.render_pass_depth_image);
            sg_destroy_pass(graphics.render_pass);
        }

        sg_image_desc render_pass_image_desc = {
            .render_target = true,
            .width = (int)graphics.render_size.x,
            .height = (int)graphics.render_size.y,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
        };
        graphics.render_pass_image = sg_make_image(&render_pass_image_desc);

        sg_image_desc render_pass_depth_image_desc = {
            .render_target = true,
            .width = (int)graphics.render_size.x,
            .height = (int)graphics.render_size.y,
            .pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL,
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
        };
        graphics.render_pass_depth_image = sg_make_image(&render_pass_depth_image_desc);

        sg_pass_desc render_pass_desc = {
            .color_attachments[0] = {
                .image = graphics.render_pass_image,
            },
            .depth_stencil_attachment = {
                .image = graphics.render_pass_depth_image  
            },
        };
        graphics.render_pass = sg_make_pass(&render_pass_desc);

        graphics.render_pass_inited = true;
    }

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

void golf_graphics_end_frame(void) {
    {
        sg_pass_action action = {
            .colors[0] = {
                .action = SG_ACTION_CLEAR,
                .value = { 0.529f, 0.808f, 0.922f, 1.0f },
            },
        };

        sg_begin_default_pass(&action, sapp_width(), sapp_height());
        sg_apply_viewportf(graphics.viewport_pos.x, graphics.viewport_pos.y, 
                graphics.viewport_size.x, graphics.viewport_size.y, true);
        sg_apply_pipeline(graphics.render_image_pipeline);
        golf_model_t *square = golf_data_get_model("data/models/render_image_square.obj");
        sg_bindings bindings = {
            .vertex_buffers[ATTR_render_image_vs_position] = square->sg_positions_buf,
            .vertex_buffers[ATTR_render_image_vs_texture_coord] = square->sg_texcoords_buf,
            .fs_images[SLOT_render_image_texture] = graphics.render_pass_image,
        };
        sg_apply_bindings(&bindings);
        sg_draw(0, square->positions.length, 1);
        sg_end_pass();
    }

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

void golf_graphics_set_render_size(vec2 size) {
    graphics.render_size = size;
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
    igText("Render Size: <%.3f, %.3f>", graphics.render_size.x, graphics.render_size.y); 
    igText("Cam Pos: <%.3f, %.3f, %.3f>", graphics.cam_pos.x, graphics.cam_pos.y, graphics.cam_pos.z); 
    igText("Cam Dir: <%.3f, %.3f, %.3f>", graphics.cam_dir.x, graphics.cam_dir.y, graphics.cam_dir.z); 
    igText("Cam Up: <%.3f, %.3f, %.3f>", graphics.cam_up.x, graphics.cam_up.y, graphics.cam_up.z); 
    igText("Cam Inclination: %.3f", graphics.cam_inclination_angle); 
    igText("Cam Azimuth: %.3f", graphics.cam_azimuth_angle); 
}

bool golf_graphics_get_shader_desc(const char *path, sg_shader_desc *desc) {
    const sg_shader_desc *const_shader_desc = NULL;
    if (strcmp(path, "data/shaders/diffuse_color_material.glsl") == 0) {
        const_shader_desc = diffuse_color_material_shader_desc(sg_query_backend());
    }
    else if (strcmp(path, "data/shaders/environment_material.glsl") == 0) {
        const_shader_desc = environment_material_shader_desc(sg_query_backend());
    }
    else if (strcmp(path, "data/shaders/pass_through.glsl") == 0) {
        const_shader_desc = pass_through_shader_desc(sg_query_backend());
    }
    else if (strcmp(path, "data/shaders/solid_color_material.glsl") == 0) {
        const_shader_desc = solid_color_material_shader_desc(sg_query_backend());
    }
    else if (strcmp(path, "data/shaders/texture_material.glsl") == 0) {
        const_shader_desc = texture_material_shader_desc(sg_query_backend());
    }
    else if (strcmp(path, "data/shaders/ui.glsl") == 0) {
        const_shader_desc = ui_shader_desc(sg_query_backend());
    }
    else if (strcmp(path, "data/shaders/render_image.glsl") == 0) {
        const_shader_desc = render_image_shader_desc(sg_query_backend());
    }
    else {
        return false;
    }

    *desc = *const_shader_desc;
    return true;
}
