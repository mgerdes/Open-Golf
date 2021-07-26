#define _CRT_SECURE_NO_WARNINGS

#include "renderer.h"

#include <assert.h>
#include <float.h>
#include <string.h>

#include "assets.h"
#include "config.h"
#include "mfile.h"
#include "game.h"
#include "game_editor.h"
#include "hotloader.h"
#include "log.h"
#include "mdata.h"
#include "profiler.h"
#include "rnd.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include "sokol_gfx.h"
#include "sokol_imgui.h"
#include "sokol_time.h"

#include "shaders/aim_icon.h"
#include "shaders/aim_helper.h"
#include "shaders/ball.h"
#include "shaders/cup.h"
#include "shaders/environment.h"
#include "shaders/fxaa.h"
#include "shaders/hole_editor_environment.h"
#include "shaders/hole_editor_terrain.h"
#include "shaders/hole_editor_water.h"
#include "shaders/occluded_ball.h"
#include "shaders/pass_through.h"
#include "shaders/single_color.h"
#include "shaders/terrain.h"
#include "shaders/texture.h"
#include "shaders/water.h"
#include "shaders/water_around_ball.h"
#include "shaders/water_ripple.h"
#include "shaders/ui.h"
#include "shaders/ui_single_color.h"

static bool load_shader(const char *shader_name, const sg_shader_desc *const_shader_desc, sg_shader *shader) {
#if HOTLOADER_ACTIVE
    char fs_path[MFILE_MAX_PATH + 1];
    snprintf(fs_path, MFILE_MAX_PATH, "src/shaders/bare/%s_fs.glsl", shader_name);
    fs_path[MFILE_MAX_PATH] = 0;

    char vs_path[MFILE_MAX_PATH + 1];
    snprintf(vs_path, MFILE_MAX_PATH, "src/shaders/bare/%s_vs.glsl", shader_name);
    vs_path[MFILE_MAX_PATH] = 0;

    mfile_t fs_file = mfile(fs_path);
    mfile_t vs_file = mfile(vs_path);

    if (!mfile_load_data(&fs_file)) {
        return false;
    }

    if (!mfile_load_data(&vs_file)) {
        mfile_free_data(&fs_file);
        return false;
    }

    sg_shader_desc shader_desc = *const_shader_desc;
    shader_desc.fs.source = fs_file.data;
    shader_desc.vs.source = vs_file.data;
    *shader = sg_make_shader(&shader_desc);
#else
    *shader = sg_make_shader(const_shader_desc);
#endif
    return true;
}

static bool should_load_shader(mfile_t file, bool first_time) {
    // This can be called with either the bare shader files, in the src/shaders/bare/ folder, or normal shader 
    // files in the src/shaders/ folder. If it's called with a bare shader file it should only load if it's not
    // the first time. If it's called with a normal shader file it should only load if it is the first time.
    // If you update a normal shader file you have to recompile the bare shader files to get the shader resource
    // reloaded.
    bool is_bare = strstr(file.path, "src/shaders/bare/") == file.path;
    if (is_bare && first_time) {
        return false;
    }
    if (!is_bare && !first_time) {
        return false;
    }
    return true;
}

static bool load_aim_icon_shader(mfile_t file, bool first_time, struct renderer *renderer) {
    if (!should_load_shader(file, first_time)) {
        return true;
    }

    sg_shader shader;
    if (!load_shader("aim_icon", aim_icon_shader_desc(), &shader)) {
        return false;
    }

    sg_shader_info info = sg_query_shader_info(shader);
    if (info.slot.state == SG_RESOURCESTATE_FAILED) {
        return true;
    }

    // Only destroy the resources if they haven't already been made
    if (renderer->sokol.aim_icon_shader.id != SG_INVALID_ID) {
        sg_destroy_shader(renderer->sokol.aim_icon_shader);
        sg_destroy_pipeline(renderer->sokol.aim_icon_pipeline);
    }

    renderer->sokol.aim_icon_shader = shader;

    {
        sg_pipeline_desc aim_icon_pipeline_desc = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_aim_icon_vs_position] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_aim_icon_vs_alpha] = { .format = SG_VERTEXFORMAT_FLOAT, .buffer_index = 1 },
                },
            },
            .depth_stencil = {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = false,
            },
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            },
        };
        renderer->sokol.aim_icon_pipeline = sg_make_pipeline(&aim_icon_pipeline_desc);
    }

    return true;
}

static bool load_aim_helper_shader(mfile_t file, bool first_time, struct renderer *renderer) {
    if (!should_load_shader(file, first_time)) {
        return true;
    }

    sg_shader shader;
    if (!load_shader("aim_helper", aim_helper_shader_desc(), &shader)) {
        return false;
    }

    sg_shader_info info = sg_query_shader_info(shader);
    if (info.slot.state == SG_RESOURCESTATE_FAILED) {
        return true;
    }

    // Only destroy the resources if they haven't already been made
    if (renderer->sokol.aim_helper_shader.id != SG_INVALID_ID) {
        sg_destroy_shader(renderer->sokol.aim_helper_shader);
        sg_destroy_pipeline(renderer->sokol.aim_helper_pipeline);
    }

    renderer->sokol.aim_helper_shader = shader;

    {
        sg_pipeline_desc aim_helper_pipeline_desc = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_aim_helper_vs_position] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_aim_helper_vs_texture_coord] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                },
            },
            .depth_stencil = {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = false,
            },
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            },
        };
        renderer->sokol.aim_helper_pipeline = sg_make_pipeline(&aim_helper_pipeline_desc);
    }

    return true;
}

static bool load_ball_shader(mfile_t file, bool first_time, struct renderer *renderer) {
    if (!should_load_shader(file, first_time)) {
        return true;
    }

    sg_shader shader;
    if (!load_shader("ball", ball_shader_desc(), &shader)) {
        return false;
    }

    sg_shader_info info = sg_query_shader_info(shader);
    if (info.slot.state == SG_RESOURCESTATE_FAILED) {
        return true;
    }

    // Only destroy the resources if they haven't already been made
    if (renderer->sokol.ball_shader.id != SG_INVALID_ID) {
        sg_destroy_shader(renderer->sokol.ball_shader);
        sg_destroy_pipeline(renderer->sokol.ball_pipeline[0]);
        sg_destroy_pipeline(renderer->sokol.ball_pipeline[1]);
    }

    renderer->sokol.ball_shader = shader;

    {
        sg_pipeline_desc pipeline0_desc = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_ball_vs_position] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_ball_vs_normal] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 1 },
                    [ATTR_ball_vs_texture_coord] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 2 },
                },
            },
            .depth_stencil = {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = true,
            },
        };
        renderer->sokol.ball_pipeline[0] = sg_make_pipeline(&pipeline0_desc);
    }

    {
        sg_pipeline_desc pipeline1_desc = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_ball_vs_position] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_ball_vs_normal] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 1 },
                    [ATTR_ball_vs_texture_coord] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 2 },
                },
            },
            .depth_stencil = {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = true,
                .stencil_front = {
                    .fail_op = SG_STENCILOP_KEEP,
                    .depth_fail_op = SG_STENCILOP_KEEP,
                    .pass_op = SG_STENCILOP_KEEP,
                    .compare_func = SG_COMPAREFUNC_EQUAL,
                },
                .stencil_back = {
                    .fail_op = SG_STENCILOP_KEEP,
                    .depth_fail_op = SG_STENCILOP_KEEP,
                    .pass_op = SG_STENCILOP_KEEP,
                    .compare_func = SG_COMPAREFUNC_EQUAL,
                },
                .stencil_enabled = true,
                .stencil_read_mask = 255,
                .stencil_write_mask = 255,
                .stencil_ref = 255,
            },
        };
        renderer->sokol.ball_pipeline[1] = sg_make_pipeline(&pipeline1_desc);
    }

    return true;
}

static bool load_hole_editor_environment_shader(mfile_t file, bool first_time, struct renderer *renderer) {
    if (!should_load_shader(file, first_time)) {
        return true;
    }

    sg_shader shader;
    if (!load_shader("hole_editor_environment", hole_editor_environment_shader_desc(), &shader)) {
        return false;
    }

    sg_shader_info info = sg_query_shader_info(shader);
    if (info.slot.state == SG_RESOURCESTATE_FAILED) {
        return true;
    }

    // Only destroy the resources if they haven't already been made
    if (renderer->sokol.hole_editor_environment_shader.id != SG_INVALID_ID) {
        sg_destroy_shader(renderer->sokol.hole_editor_environment_shader);
        sg_destroy_pipeline(renderer->sokol.hole_editor_environment_pipeline);
    }

    renderer->sokol.hole_editor_environment_shader = shader;

    {
        sg_pipeline_desc environment_pipeline_desc = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_hole_editor_environment_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_hole_editor_environment_vs_normal] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 1 },
                    [ATTR_hole_editor_environment_vs_texture_coord] 
                        = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 2 },
                    [ATTR_hole_editor_environment_vs_lightmap_uv] 
                        = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 3 },
                },
            },
            .depth_stencil = {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = true,
            },
        };
        renderer->sokol.hole_editor_environment_pipeline = sg_make_pipeline(&environment_pipeline_desc);
    }

    return true;
}

static bool load_hole_editor_terrain_shader(mfile_t file, bool first_time, struct renderer *renderer) {
    if (!should_load_shader(file, first_time)) {
        return true;
    }

    sg_shader shader;
    if (!load_shader("hole_editor_terrain", hole_editor_terrain_shader_desc(), &shader)) {
        return false;
    }

    sg_shader_info info = sg_query_shader_info(shader);
    if (info.slot.state == SG_RESOURCESTATE_FAILED) {
        return true;
    }

    // Only destroy the resources if they haven't already been made
    if (renderer->sokol.hole_editor_terrain_shader.id != SG_INVALID_ID) {
        sg_destroy_shader(renderer->sokol.hole_editor_terrain_shader);
        sg_destroy_pipeline(renderer->sokol.hole_editor_terrain_pipeline);
    }

    renderer->sokol.hole_editor_terrain_shader = shader;

    {
        sg_pipeline_desc terrain_pipeline_desc = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_hole_editor_terrain_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_hole_editor_terrain_vs_normal] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 1 },
                    [ATTR_hole_editor_terrain_vs_texture_coord] 
                        = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 2 },
                    [ATTR_hole_editor_terrain_vs_lightmap_uv] 
                        = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 3 },
                    [ATTR_hole_editor_terrain_vs_material_idx] 
                        = { .format = SG_VERTEXFORMAT_FLOAT, .buffer_index = 4 },
                },
            },
            .depth_stencil = {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = true,
            },
        };
        renderer->sokol.hole_editor_terrain_pipeline = sg_make_pipeline(&terrain_pipeline_desc);
    }

    return true;
}

static bool load_hole_editor_water_shader(mfile_t file, bool first_time, struct renderer *renderer) {
    if (!should_load_shader(file, first_time)) {
        return true;
    }

    sg_shader shader;
    if (!load_shader("hole_editor_water", hole_editor_water_shader_desc(), &shader)) {
        return false;
    }

    sg_shader_info info = sg_query_shader_info(shader);
    if (info.slot.state == SG_RESOURCESTATE_FAILED) {
        return true;
    }

    // Only destroy the resources if they haven't already been made
    if (renderer->sokol.hole_editor_water_shader.id != SG_INVALID_ID) {
        sg_destroy_shader(renderer->sokol.hole_editor_water_shader);
        sg_destroy_pipeline(renderer->sokol.hole_editor_water_pipeline);
    }

    renderer->sokol.hole_editor_water_shader = shader;

    {
        sg_pipeline_desc pipeline_desc = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_hole_editor_water_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_hole_editor_water_vs_texture_coord] 
                        = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                    [ATTR_hole_editor_water_vs_lightmap_uv] 
                        = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 2 },
                },
            },
            .depth_stencil = {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = true,
            },
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            },
        };
        renderer->sokol.hole_editor_water_pipeline = sg_make_pipeline(&pipeline_desc);
    }

    return true;
}

static bool load_environment_shader(mfile_t file, bool first_time, struct renderer *renderer) {
    if (!should_load_shader(file, first_time)) {
        return true;
    }

    sg_shader shader;
    if (!load_shader("environment", environment_shader_desc(), &shader)) {
        return false;
    }

    sg_shader_info info = sg_query_shader_info(shader);
    if (info.slot.state == SG_RESOURCESTATE_FAILED) {
        return true;
    }

    // Only destroy the resources if they haven't already been made
    if (renderer->sokol.environment_shader.id != SG_INVALID_ID) {
        sg_destroy_shader(renderer->sokol.environment_shader);
        sg_destroy_pipeline(renderer->sokol.environment_pipeline);
    }

    renderer->sokol.environment_shader = shader;

    {
        sg_pipeline_desc environment_pipeline_desc = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_environment_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_environment_vs_texture_coord] 
                        = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                    [ATTR_environment_vs_lightmap_uv] 
                        = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 2 },
                },
            },
            .depth_stencil = {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = true,
            },
        };
        renderer->sokol.environment_pipeline = sg_make_pipeline(&environment_pipeline_desc);
    }

    return true;
}

static bool load_fxaa_shader(mfile_t file, bool first_time, struct renderer *renderer) {
    if (!should_load_shader(file, first_time)) {
        return true;
    }

    sg_shader shader;
    if (!load_shader("fxaa", fxaa_shader_desc(), &shader)) {
        return false;
    }

    sg_shader_info info = sg_query_shader_info(shader);
    if (info.slot.state == SG_RESOURCESTATE_FAILED) {
        return true;
    }

    // Only destroy the resources if they haven't already been made
    if (renderer->sokol.fxaa_shader.id != SG_INVALID_ID) {
        sg_destroy_shader(renderer->sokol.fxaa_shader);
        sg_destroy_pipeline(renderer->sokol.fxaa_pipeline);
    }

    renderer->sokol.fxaa_shader = shader;

    {
        sg_pipeline_desc pipeline_desc = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_fxaa_vs_position] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_fxaa_vs_texture_coord] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                },
            },
        };
        renderer->sokol.fxaa_pipeline = sg_make_pipeline(&pipeline_desc);
    }

    return true;
}

static bool load_cup_shader(mfile_t file, bool first_time, struct renderer *renderer) {
    if (!should_load_shader(file, first_time)) {
        return true;
    }

    sg_shader shader;
    if (!load_shader("cup", cup_shader_desc(), &shader)) {
        return false;
    }

    sg_shader_info info = sg_query_shader_info(shader);
    if (info.slot.state == SG_RESOURCESTATE_FAILED) {
        return true;
    }

    // Only destroy the resources if they haven't already been made
    if (renderer->sokol.cup_shader.id != SG_INVALID_ID) {
        sg_destroy_shader(renderer->sokol.cup_shader);
        sg_destroy_pipeline(renderer->sokol.cup_pipeline[1]);
    }

    renderer->sokol.cup_shader = shader;

    {
        sg_pipeline_desc cup_pipeline1_desc = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_cup_vs_position] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_cup_vs_lightmap_uv] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                },
            },
            .depth_stencil = {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = true,
                .stencil_front = {
                    .fail_op = SG_STENCILOP_KEEP,
                    .depth_fail_op = SG_STENCILOP_KEEP,
                    .pass_op = SG_STENCILOP_KEEP,
                    .compare_func = SG_COMPAREFUNC_EQUAL,
                },
                .stencil_back = {
                    .fail_op = SG_STENCILOP_KEEP,
                    .depth_fail_op = SG_STENCILOP_KEEP,
                    .pass_op = SG_STENCILOP_KEEP,
                    .compare_func = SG_COMPAREFUNC_EQUAL,
                },
                .stencil_enabled = true,
                .stencil_read_mask = 255,
                .stencil_write_mask = 255,
                .stencil_ref = 255,
            },
        };
        renderer->sokol.cup_pipeline[1] = sg_make_pipeline(&cup_pipeline1_desc);
    }

    return true;
}

static bool load_occluded_ball_shader(mfile_t file, bool first_time, struct renderer *renderer) {
    if (!should_load_shader(file, first_time)) {
        return true;
    }

    sg_shader shader;
    if (!load_shader("occluded_ball", occluded_ball_shader_desc(), &shader)) {
        return false;
    }

    sg_shader_info info = sg_query_shader_info(shader);
    if (info.slot.state == SG_RESOURCESTATE_FAILED) {
        return true;
    }

    // Only destroy the resources if they haven't already been made
    if (renderer->sokol.occluded_ball_shader.id != SG_INVALID_ID) {
        sg_destroy_shader(renderer->sokol.occluded_ball_shader);
        sg_destroy_pipeline(renderer->sokol.occluded_ball_pipeline);
    }

    renderer->sokol.occluded_ball_shader = shader;

    {
        sg_pipeline_desc occluded_ball_pipeline = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_occluded_ball_vs_position] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_occluded_ball_vs_normal] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 1 },
                },
            },
            .depth_stencil = {
                .depth_compare_func = SG_COMPAREFUNC_GREATER,
            },
            .rasterizer = {
                .cull_mode = SG_CULLMODE_FRONT,
            },
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            },
        };
        renderer->sokol.occluded_ball_pipeline = sg_make_pipeline(&occluded_ball_pipeline);
    }

    return true;
}

static bool load_pass_through_shader(mfile_t file, bool first_time, struct renderer *renderer) {
    if (!should_load_shader(file, first_time)) {
        return true;
    }

    sg_shader shader;
    if (!load_shader("pass_through", pass_through_shader_desc(), &shader)) {
        return false;
    }

    sg_shader_info info = sg_query_shader_info(shader);
    if (info.slot.state == SG_RESOURCESTATE_FAILED) {
        return true;
    }

    if (renderer->sokol.pass_through_shader.id != SG_INVALID_ID) {
        sg_destroy_shader(renderer->sokol.pass_through_shader);
        sg_destroy_pipeline(renderer->sokol.cup_pipeline[0]);
    }

    renderer->sokol.pass_through_shader = shader;

    {
        sg_pipeline_desc cup_pipeline0_desc = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_pass_through_vs_position] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                },
            },
            .depth_stencil = {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = false,
                .stencil_front = {
                    .fail_op = SG_STENCILOP_KEEP,
                    .depth_fail_op = SG_STENCILOP_KEEP,
                    .pass_op = SG_STENCILOP_REPLACE,
                    .compare_func = SG_COMPAREFUNC_ALWAYS,
                },
                .stencil_back = {
                    .fail_op = SG_STENCILOP_KEEP,
                    .depth_fail_op = SG_STENCILOP_KEEP,
                    .pass_op = SG_STENCILOP_REPLACE,
                    .compare_func = SG_COMPAREFUNC_ALWAYS,
                },
                .stencil_enabled = true,
                .stencil_write_mask = 255,
                .stencil_ref = 255,
            },
            .blend = {
                .enabled = true,
                .color_write_mask = SG_COLORMASK_NONE,
            },
        };
        renderer->sokol.cup_pipeline[0] = sg_make_pipeline(&cup_pipeline0_desc);
    }

    return true;
}

static bool load_single_color_shader(mfile_t file, bool first_time, struct renderer *renderer) {
    if (!should_load_shader(file, first_time)) {
        return true;
    }

    sg_shader shader;
    if (!load_shader("single_color", single_color_shader_desc(), &shader)) {
        return false;
    }

    sg_shader_info info = sg_query_shader_info(shader);
    if (info.slot.state == SG_RESOURCESTATE_FAILED) {
        return true;
    }

    if (renderer->sokol.single_color_shader.id != SG_INVALID_ID) {
        sg_destroy_shader(renderer->sokol.single_color_shader);
        sg_destroy_pipeline(renderer->sokol.physics_debug_pipeline);
        sg_destroy_pipeline(renderer->sokol.objects_pipeline);
    }

    renderer->sokol.single_color_shader = shader;

    {
        sg_pipeline_desc physics_pipeline_desc = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_single_color_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_single_color_vs_normal] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 1 },
                },
            },
            .depth_stencil = {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = false,
            },
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            },
        };
        renderer->sokol.physics_debug_pipeline = sg_make_pipeline(&physics_pipeline_desc);
    }

    {
        sg_pipeline_desc objects_pipeline_desc = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_single_color_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_single_color_vs_normal] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 1 },
                },
            },
            .depth_stencil = {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = true,
            },
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            },
        };
        renderer->sokol.objects_pipeline = sg_make_pipeline(&objects_pipeline_desc);
    }

    return true;
}

static bool load_terrain_shader(mfile_t file, bool first_time, struct renderer *renderer) {
    if (!should_load_shader(file, first_time)) {
        return true;
    }

    sg_shader shader;
    if (!load_shader("terrain", terrain_shader_desc(), &shader)) {
        return false;
    }

    sg_shader_info info = sg_query_shader_info(shader);
    if (info.slot.state == SG_RESOURCESTATE_FAILED) {
        return true;
    }

    if (renderer->sokol.terrain_shader.id != SG_INVALID_ID) {
        sg_destroy_shader(renderer->sokol.terrain_shader);
        sg_destroy_pipeline(renderer->sokol.terrain_pipeline[0]);
        sg_destroy_pipeline(renderer->sokol.terrain_pipeline[1]);
    }

    renderer->sokol.terrain_shader = shader;

    {
        sg_pipeline_desc terrain_pipeline_desc = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_terrain_vs_position] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_terrain_vs_normal] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 1 },
                    [ATTR_terrain_vs_texture_coord] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 2 },
                    [ATTR_terrain_vs_lightmap_uv] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 3 },
                    [ATTR_terrain_vs_material_idx] = { .format = SG_VERTEXFORMAT_FLOAT, .buffer_index = 4 },
                },
            },
            .depth_stencil = {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = true,
            },
            .rasterizer = {
                .cull_mode = SG_CULLMODE_FRONT,
            },
        };
        renderer->sokol.terrain_pipeline[0] = sg_make_pipeline(&terrain_pipeline_desc);
    }

    {
        sg_pipeline_desc terrain_pipeline_desc = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_terrain_vs_position] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_terrain_vs_normal] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 1 },
                    [ATTR_terrain_vs_texture_coord] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 2 },
                    [ATTR_terrain_vs_lightmap_uv] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 3 },
                    [ATTR_terrain_vs_material_idx] = { .format = SG_VERTEXFORMAT_FLOAT, .buffer_index = 4 },
                },
            },
            .depth_stencil = {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = true,
            },
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            },
            .rasterizer = {
                .cull_mode = SG_CULLMODE_BACK,
            },
        };
        renderer->sokol.terrain_pipeline[1] = sg_make_pipeline(&terrain_pipeline_desc);
    }

    return true;
}

static bool load_texture_shader(mfile_t file, bool first_time, struct renderer *renderer) {
    if (!should_load_shader(file, first_time)) {
        return true;
    }

    sg_shader shader;
    if (!load_shader("texture", texture_shader_desc(), &shader)) {
        return false;
    }

    sg_shader_info info = sg_query_shader_info(shader);
    if (info.slot.state == SG_RESOURCESTATE_FAILED) {
        return true;
    }

    if (renderer->sokol.texture_shader.id != SG_INVALID_ID) {
        sg_destroy_shader(renderer->sokol.texture_shader);
        sg_destroy_pipeline(renderer->sokol.texture_pipeline);
    }

    renderer->sokol.texture_shader = shader;

    {
        sg_pipeline_desc texture_pipeline_desc = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_texture_vs_position] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_texture_vs_texture_coord] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                },
            },
        };
        renderer->sokol.texture_pipeline = sg_make_pipeline(&texture_pipeline_desc);
    }

    return true;
}

static bool load_ui_shader(mfile_t file, bool first_time, struct renderer *renderer) {
    if (!should_load_shader(file, first_time)) {
        return true;
    }

    sg_shader shader;
    if (!load_shader("ui", ui_shader_desc(), &shader)) {
        return false;
    }

    sg_shader_info info = sg_query_shader_info(shader);
    if (info.slot.state == SG_RESOURCESTATE_FAILED) {
        return true;
    }

    if (renderer->sokol.ui_shader.id != SG_INVALID_ID) {
        sg_destroy_shader(renderer->sokol.ui_shader);
        sg_destroy_pipeline(renderer->sokol.ui_pipeline);
    }

    renderer->sokol.ui_shader = shader;

    {
        sg_pipeline_desc pipeline_desc = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_ui_vs_position] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_ui_vs_texture_coord] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                },
            },
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            },
        };
        renderer->sokol.ui_pipeline = sg_make_pipeline(&pipeline_desc);
    }

    return true;
}

static bool load_ui_single_color_shader(mfile_t file, bool first_time, struct renderer *renderer) {
    if (!should_load_shader(file, first_time)) {
        return true;
    }

    sg_shader shader;
    if (!load_shader("ui_single_color", ui_single_color_shader_desc(), &shader)) {
        return false;
    }

    sg_shader_info info = sg_query_shader_info(shader);
    if (info.slot.state == SG_RESOURCESTATE_FAILED) {
        return true;
    }

    if (renderer->sokol.ui_single_color_shader.id != SG_INVALID_ID) {
        sg_destroy_shader(renderer->sokol.ui_single_color_shader);
        sg_destroy_pipeline(renderer->sokol.ui_single_color_pipeline);
    }

    renderer->sokol.ui_single_color_shader = shader;

    {
        sg_pipeline_desc ui_single_color_pipeline = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_ui_single_color_vs_position] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                },
            },
            .depth_stencil = {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = false,
            },
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            },
        };
        renderer->sokol.ui_single_color_pipeline = sg_make_pipeline(&ui_single_color_pipeline);
    }

    return true;
}

static bool load_water_shader(mfile_t file, bool first_time, struct renderer *renderer) {
    if (!should_load_shader(file, first_time)) {
        return true;
    }

    sg_shader shader;
    if (!load_shader("water", water_shader_desc(), &shader)) {
        return false;
    }

    sg_shader_info info = sg_query_shader_info(shader);
    if (info.slot.state == SG_RESOURCESTATE_FAILED) {
        return true;
    }

    if (renderer->sokol.water_shader.id != SG_INVALID_ID) {
        sg_destroy_shader(renderer->sokol.water_shader);
        sg_destroy_pipeline(renderer->sokol.water_pipeline);
    }

    renderer->sokol.water_shader = shader;

    {
        sg_pipeline_desc water_pipeline_desc = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_water_vs_position] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_water_vs_texture_coord] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                    [ATTR_water_vs_lightmap_uv] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 2 },
                },
            },
            .depth_stencil = {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = true,
                .stencil_front = {
                    .fail_op = SG_STENCILOP_KEEP,
                    .depth_fail_op = SG_STENCILOP_REPLACE,
                    .pass_op = SG_STENCILOP_REPLACE,
                    .compare_func = SG_COMPAREFUNC_ALWAYS,
                },
                .stencil_back = {
                    .fail_op = SG_STENCILOP_KEEP,
                    .depth_fail_op = SG_STENCILOP_REPLACE,
                    .pass_op = SG_STENCILOP_REPLACE,
                    .compare_func = SG_COMPAREFUNC_ALWAYS,
                },
                .stencil_enabled = true,
                .stencil_write_mask = 255,
                .stencil_read_mask = 255,
                .stencil_ref = 255,
            },
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            },
        };
        renderer->sokol.water_pipeline = sg_make_pipeline(&water_pipeline_desc);
    }

    return true;
}

static bool load_water_around_ball_shader(mfile_t file, bool first_time, struct renderer *renderer) {
    if (!should_load_shader(file, first_time)) {
        return true;
    }

    sg_shader shader;
    if (!load_shader("water_around_ball", water_around_ball_shader_desc(), &shader)) {
        return false;
    }

    sg_shader_info info = sg_query_shader_info(shader);
    if (info.slot.state == SG_RESOURCESTATE_FAILED) {
        return true;
    }

    if (renderer->sokol.water_around_ball_shader.id != SG_INVALID_ID) {
        sg_destroy_shader(renderer->sokol.water_around_ball_shader);
        sg_destroy_pipeline(renderer->sokol.water_around_ball_pipeline);
    }

    renderer->sokol.water_around_ball_shader = shader;

    {
        sg_pipeline_desc water_around_ball_pipeline_desc = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_water_vs_position] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_water_vs_texture_coord] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                },
            },
            .depth_stencil = {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = false,
                .stencil_front = {
                    .fail_op = SG_STENCILOP_KEEP,
                    .depth_fail_op = SG_STENCILOP_KEEP,
                    .pass_op = SG_STENCILOP_KEEP,
                    .compare_func = SG_COMPAREFUNC_EQUAL,
                },
                .stencil_back = {
                    .fail_op = SG_STENCILOP_KEEP,
                    .depth_fail_op = SG_STENCILOP_KEEP,
                    .pass_op = SG_STENCILOP_KEEP,
                    .compare_func = SG_COMPAREFUNC_EQUAL,
                },
                .stencil_enabled = true,
                .stencil_write_mask = 255,
                .stencil_read_mask = 255,
                .stencil_ref = 255,
            },
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            },
        };
        renderer->sokol.water_around_ball_pipeline = sg_make_pipeline(&water_around_ball_pipeline_desc);
    }

    return true;
}

static bool load_water_ripple_shader(mfile_t file, bool first_time, struct renderer *renderer) {
    if (!should_load_shader(file, first_time)) {
        return true;
    }

    sg_shader shader;
    if (!load_shader("water_ripple", water_ripple_shader_desc(), &shader)) {
        return false;
    }

    sg_shader_info info = sg_query_shader_info(shader);
    if (info.slot.state == SG_RESOURCESTATE_FAILED) {
        return true;
    }

    if (renderer->sokol.water_ripple_shader.id != SG_INVALID_ID) {
        sg_destroy_shader(renderer->sokol.water_ripple_shader);
        sg_destroy_pipeline(renderer->sokol.water_ripple_pipeline);
    }

    renderer->sokol.water_ripple_shader = shader;

    {
        sg_pipeline_desc water_ripple_pipeline_desc = {
            .shader = shader,
            .layout = {
                .attrs = {
                    [ATTR_water_vs_position] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_water_vs_texture_coord] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                },
            },
            .depth_stencil = {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = false,
                .stencil_front = {
                    .fail_op = SG_STENCILOP_KEEP,
                    .depth_fail_op = SG_STENCILOP_KEEP,
                    .pass_op = SG_STENCILOP_KEEP,
                    .compare_func = SG_COMPAREFUNC_EQUAL,
                },
                .stencil_back = {
                    .fail_op = SG_STENCILOP_KEEP,
                    .depth_fail_op = SG_STENCILOP_KEEP,
                    .pass_op = SG_STENCILOP_KEEP,
                    .compare_func = SG_COMPAREFUNC_EQUAL,
                },
                .stencil_enabled = true,
                .stencil_write_mask = 255,
                .stencil_read_mask = 255,
                .stencil_ref = 255,
            },
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            },
        };
        renderer->sokol.water_ripple_pipeline = sg_make_pipeline(&water_ripple_pipeline_desc);
    }

    return true;
}

static void renderer_watch_shader(const char *name, struct renderer *renderer, 
        bool (*callback)(mfile_t file, bool first_time, struct renderer *udata)) {
    char shader_filename[MFILE_MAX_PATH+1];
    snprintf(shader_filename, MFILE_MAX_PATH, "src/shaders/%s.glsl", name);
    shader_filename[MFILE_MAX_PATH] = 0;
    hotloader_watch_file(shader_filename, renderer, 
            (bool (*)(mfile_t file, bool first_time, void *udata))callback);

#if HOTLOADER_ACTIVE
    char bare_fs_filename[MFILE_MAX_PATH+1];
    snprintf(bare_fs_filename, MFILE_MAX_PATH, "src/shaders/bare/%s_fs.glsl", name);
    bare_fs_filename[MFILE_MAX_PATH] = 0;
    hotloader_watch_file(bare_fs_filename, renderer, 
            (bool (*)(mfile_t file, bool first_time, void *udata))callback);

    char bare_vs_filename[MFILE_MAX_PATH+1];
    snprintf(bare_vs_filename, MFILE_MAX_PATH, "src/shaders/bare/%s_vs.glsl", name);
    bare_vs_filename[MFILE_MAX_PATH] = 0;
    hotloader_watch_file(bare_vs_filename, renderer,
            (bool (*)(mfile_t file, bool first_time, void *udata))callback);
#endif
}

static int get_font_file_property(const char *line_buffer, const char *prop) {
    const char *str = strstr(line_buffer, prop);
    assert(str);
    return atoi(str + strlen(prop) + 1);
}

static void create_font(struct font *font, mfile_t *file) {
    mfile_load_data(file);

    char *line_buffer = NULL;
    int line_buffer_len = 0;
    while (mfile_copy_line(file, &line_buffer, &line_buffer_len)) {
        if (strstr(line_buffer, "char") != line_buffer || strstr(line_buffer, "chars") == line_buffer) {
            continue;
        }

        int id = get_font_file_property(line_buffer, "id");
        assert(id >= 0 && id < 256);

        font->chars[id].x = get_font_file_property(line_buffer, "x");
        font->chars[id].y = get_font_file_property(line_buffer, "y");
        font->chars[id].width = get_font_file_property(line_buffer, "width");
        font->chars[id].height = get_font_file_property(line_buffer, "height");
        font->chars[id].xoffset = get_font_file_property(line_buffer, "xoffset");
        font->chars[id].yoffset = get_font_file_property(line_buffer, "yoffset");
        font->chars[id].xadvance = get_font_file_property(line_buffer, "xadvance");
    }

    mfile_free_data(file);
}

static bool _mdata_texture_mdata_file_creator(mfile_t file, mdata_file_t *mdata_file, void *udata) {
    mdata_file_add_val_int(mdata_file, "version", 0);

    if (!mfile_load_data(&file)) {
        return false;
    }

    {
        int x, y, n;
        int force_channels = 4;
        stbi_set_flip_vertically_on_load(0);
        unsigned char *tex_data = stbi_load_from_memory((unsigned char*) file.data, file.data_len, &x, &y, &n, force_channels);
        mdata_file_add_val_binary_data(mdata_file, "tex_data", (char*)tex_data, n * x * y, true);
        free(tex_data);
    }

    mfile_free_data(&file);
    return true;
}

static bool _mdata_texture_mdata_file_handler(mdata_file_t *mdata_file, struct renderer *renderer) {
    return true;
}

void renderer_init(struct renderer *renderer) {
    profiler_push_section("renderer_init");
    memset(renderer, 0, sizeof(struct renderer));

    renderer->cam_pos = V3(0.0f, 1.0f, 4.0f);
    renderer->cam_up = V3(0.0f, 1.0f, 0.0f);
    renderer->cam_dir = V3(1.0f, 0.0f, 0.0f);
    renderer->cam_azimuth_angle = 0.0f * (float)M_PI;
    renderer->cam_inclination_angle = 0.64f * (float)M_PI;
    renderer->game_fb_width = 1280;
    renderer->game_fb_height = 720;

    //mdata_add_extension_handler(".png", _mdata_texture_mdata_file_creator, NULL);

    {
        sg_image_desc fxaa_image_desc = {
            .render_target = true,
            .width = renderer->game_fb_width,
            .height = renderer->game_fb_height,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
        };
        renderer->sokol.fxaa_image = sg_make_image(&fxaa_image_desc);

        sg_pass_desc fxaa_pass_desc = {
            .color_attachments[0].image = renderer->sokol.fxaa_image,
        };
        renderer->sokol.fxaa_pass = sg_make_pass(&fxaa_pass_desc);

        sg_image_desc game_color_image_desc = {
            .render_target = true,
            .width = renderer->game_fb_width,
            .height = renderer->game_fb_height,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
        };
        renderer->sokol.game_color_image = sg_make_image(&game_color_image_desc);

        sg_image_desc game_depth_image_desc = {
            .render_target = true,
            .width = renderer->game_fb_width,
            .height = renderer->game_fb_height,
            .pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL,
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
        };
        renderer->sokol.game_depth_image = sg_make_image(&game_depth_image_desc);

        sg_pass_desc game_pass_desc = {
            .color_attachments[0].image = renderer->sokol.game_color_image,
            .depth_stencil_attachment.image = renderer->sokol.game_depth_image,
        };
        renderer->sokol.game_pass = sg_make_pass(&game_pass_desc);
    }

    {
        renderer_watch_shader("aim_icon", renderer, &load_aim_icon_shader);
        renderer_watch_shader("aim_helper", renderer, &load_aim_helper_shader);
        renderer_watch_shader("ball", renderer, &load_ball_shader);
        renderer_watch_shader("hole_editor_environment", renderer, &load_hole_editor_environment_shader);
        renderer_watch_shader("hole_editor_terrain", renderer, &load_hole_editor_terrain_shader);
        renderer_watch_shader("hole_editor_water", renderer, &load_hole_editor_water_shader);
        renderer_watch_shader("environment", renderer, &load_environment_shader);
        renderer_watch_shader("fxaa", renderer, &load_fxaa_shader);
        renderer_watch_shader("cup", renderer, &load_cup_shader);
        renderer_watch_shader("occluded_ball", renderer, &load_occluded_ball_shader);
        renderer_watch_shader("pass_through", renderer, &load_pass_through_shader);
        renderer_watch_shader("single_color", renderer, &load_single_color_shader);
        renderer_watch_shader("terrain", renderer, &load_terrain_shader);
        renderer_watch_shader("texture", renderer, &load_texture_shader);
        renderer_watch_shader("ui", renderer, &load_ui_shader);
        renderer_watch_shader("ui_single_color", renderer, &load_ui_single_color_shader);
        renderer_watch_shader("water", renderer, &load_water_shader);
        renderer_watch_shader("water_around_ball", renderer, &load_water_around_ball_shader);
        renderer_watch_shader("water_ripple", renderer, &load_water_ripple_shader);
    }

    {
        renderer->sokol.game_aim.positions_buf = 
            sg_make_buffer(&(sg_buffer_desc) {
                    .size = 6*50*sizeof(vec3),
                    .type = SG_BUFFERTYPE_VERTEXBUFFER,
                    .usage = SG_USAGE_DYNAMIC,
                    .content = NULL,
                    .label = NULL,
                    }
                    );
        renderer->sokol.game_aim.alpha_buf = 
            sg_make_buffer(&(sg_buffer_desc) {
                    .size = 6*50*sizeof(float),
                    .type = SG_BUFFERTYPE_VERTEXBUFFER,
                    .usage = SG_USAGE_DYNAMIC,
                    .content = NULL,
                    .label = NULL,
                    }
                    );
        renderer->sokol.game_aim.color = V4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    {
        mfile_t small_font_file = mfile("assets/font/font_small.fnt");
        create_font(&renderer->small_font, &small_font_file);

        mfile_t medium_font_file = mfile("assets/font/font_medium.fnt");
        create_font(&renderer->medium_font, &medium_font_file);

        mfile_t large_font_file = mfile("assets/font/font_large.fnt");
        create_font(&renderer->large_font, &large_font_file);
    }

    profiler_pop_section();
}

void renderer_update_game_icon_buffer(struct renderer *renderer, 
        vec2 off, vec2 bp0, vec2 bp1, vec2 bp2, vec2 bp3) {
    vec3 *positions = malloc(6*50*sizeof(vec3)); 
    float *alpha = malloc(6*50*sizeof(float));
    for (int i = 0; i < 50; i++) {
        float t0 = i / 50.0f;
        float t1 = (i + 1) / 50.0f;

        vec2 p0_l = vec2_bezier(bp0, bp1, bp2, bp3, t0);
        vec2 p1_l = vec2_bezier(bp0, bp1, bp2, bp3, t1);
        p0_l.x *= 200.0f;
        p1_l.x *= 200.0f;
        p0_l.y = 1.0f - p0_l.y;
        p1_l.y = 1.0f - p1_l.y;
        p0_l.y *= 250.0f;
        p1_l.y *= 250.0f;
        p0_l = vec2_add(p0_l, off);
        p1_l = vec2_add(p1_l, off);

        vec2 p0_r = vec2_bezier(bp0, bp1, bp2, bp3, t0);
        vec2 p1_r = vec2_bezier(bp0, bp1, bp2, bp3, t1);
        p0_r.x *= 200.0f;
        p1_r.x *= 200.0f;
        p0_r.y = 1.0f - p0_r.y;
        p1_r.y = 1.0f - p1_r.y;
        p0_r.y *= 250.0f;
        p1_r.y *= 250.0f;
        p0_r.x = -p0_r.x;
        p1_r.x = -p1_r.x;
        p0_r = vec2_add(p0_r, V2(-off.x, off.y));
        p1_r = vec2_add(p1_r, V2(-off.x, off.y));

        vec3 p0, p1, p2, p3;
        p0 = V3(p0_l.x, p0_l.y, 0.0f);
        p1 = V3(p0_r.x, p0_r.y, 0.0f);
        p2 = V3(p1_r.x, p1_r.y, 0.0f);
        p3 = V3(p1_l.x, p1_l.y, 0.0f);

        positions[6*i + 0] = p0;
        positions[6*i + 1] = p1;
        positions[6*i + 2] = p2;
        positions[6*i + 3] = p3;
        positions[6*i + 4] = p0;
        positions[6*i + 5] = p2;

        float alpha0 = 1.0f, alpha1 = 1.0f;
        if (i < 3) {
            alpha0 = i/3.0f;
            alpha1 = (i + 1)/3.0f;
        }
        if (i > 46) {
            alpha0 = (50 - i)/3.0f;
            alpha1 = (50 - i - 1)/3.0f;
        }

        alpha[6*i + 0] = alpha0;
        alpha[6*i + 1] = alpha0;
        alpha[6*i + 2] = alpha1;
        alpha[6*i + 3] = alpha1;
        alpha[6*i + 4] = alpha0;
        alpha[6*i + 5] = alpha1;
    }
    sg_update_buffer(renderer->sokol.game_aim.positions_buf, positions, 6*50*sizeof(vec3));
    sg_update_buffer(renderer->sokol.game_aim.alpha_buf, alpha, 6*50*sizeof(float));
    renderer->sokol.game_aim.num_points = 6*50;
    free(positions);
}

void renderer_new_frame(struct renderer *renderer, float dt) {
    profiler_push_section("renderer_new_frame");
    renderer->window_width = sapp_width();
    renderer->window_height = sapp_height();
    vec3 cam_dir;
    {
        float theta = renderer->cam_inclination_angle;
        float phi = renderer->cam_azimuth_angle;
        cam_dir.x = sinf(theta) * cosf(phi);
        cam_dir.y = cosf(theta);
        cam_dir.z = sinf(theta) * sinf(phi);
    }
    {
        float fb_width = (float) renderer->game_fb_width;
        float fb_height = (float) renderer->game_fb_height;
        float w_width = (float) renderer->window_width;
        float w_height = (float) renderer->window_height;
        float w_fb_width = w_width;
        float w_fb_height = (fb_height/fb_width)*w_fb_width;
        if (w_fb_height > w_height) {
            w_fb_height = w_height;
            w_fb_width = (fb_width/fb_height)*w_fb_height;
        }
        renderer->near_plane = 0.1f;
        renderer->far_plane = 150.0f;
        renderer->proj_mat = mat4_perspective_projection(66.0f, fb_width/fb_height, 
                renderer->near_plane, renderer->far_plane);
        renderer->view_mat = mat4_look_at(renderer->cam_pos, 
                vec3_add(renderer->cam_pos, renderer->cam_dir), 
                renderer->cam_up);
        renderer->proj_view_mat = mat4_multiply(renderer->proj_mat, renderer->view_mat);
        renderer->ui_proj_mat = mat4_multiply_n(3,
                mat4_orthographic_projection(0.0f, w_width, 0.0f, w_height, 0.0f, 1.0f),
                mat4_translation(V3(0.5f*w_width - 0.5f*w_fb_width, 0.5f*w_height - 0.5f*w_fb_height, 0.0f)),
                mat4_scale(V3(w_fb_width/fb_width, w_fb_height/fb_height, 1.0f))
                );
    }
    {
        sg_pass_action action = {
            .colors[0] = {
                .action = SG_ACTION_CLEAR,
                .val = { 0.0f, 0.0f, 0.0f, 1.0f },
            },
        };
        sg_begin_default_pass(&action, sapp_width(), sapp_height());
        sg_end_pass();
    }
    profiler_pop_section();
}

void renderer_end_frame(struct renderer *renderer) {
    {
        sg_pass_action imgui_pass_action = {
            .colors[0] = {
                .action = SG_ACTION_DONTCARE,
            },
            .depth = {
                .action = SG_ACTION_CLEAR,
                .val = 1.0f,

            },
        };
        sg_begin_default_pass(&imgui_pass_action, sapp_width(), sapp_height());
        simgui_render();
        sg_end_pass();
    }
    sg_commit();
}

vec3 renderer_world_to_screen(struct renderer *renderer, vec3 world_point) {
    vec4 point = V4(world_point.x, world_point.y, world_point.z, 1.0f);
    point = vec4_apply_mat(point, renderer->proj_view_mat);
    point.x = renderer->game_fb_width * (point.x / point.w + 1.0f) / 2.0f;
    point.y = renderer->game_fb_height * (point.y / point.w + 1.0f) / 2.0f;
    point.z = point.z / point.w;
    return V3(point.x, point.y, point.z);
}

vec3 renderer_screen_to_world(struct renderer *renderer, vec3 screen_point) {
    float c = (renderer->far_plane + renderer->near_plane) /
        (renderer->near_plane - renderer->far_plane);
    float d = (2.0f * renderer->far_plane * renderer->near_plane) /
        (renderer->near_plane - renderer->far_plane);
    screen_point.x = -1.0f + (2.0f * screen_point.x / renderer->game_fb_width);
    screen_point.y = -1.0f + (2.0f * screen_point.y / renderer->game_fb_height);
    float w = d / (screen_point.z + c);
    vec4 screen = V4(screen_point.x * w, screen_point.y * w, screen_point.z * w, w);
    vec4 world = vec4_apply_mat(screen, mat4_inverse(renderer->proj_view_mat));
    return V3(world.x, world.y, world.z);
}

void renderer_draw_line(struct renderer *renderer, vec3 p0, vec3 p1, float size, vec4 color) {
    static bool inited = false;
    static struct model *cube_model = NULL;
    static sg_pipeline pipeline;

    if (!inited) {
        inited = true;
        cube_model = asset_store_get_model("cube");
        sg_pipeline_desc pipeline_desc = {
            .shader = renderer->sokol.single_color_shader,
            .layout = {
                .attrs = {
                    [ATTR_single_color_vs_position] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_single_color_vs_normal] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 1 },
                },
            },
            .depth_stencil = {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = true,
            },
        };
        pipeline = sg_make_pipeline(&pipeline_desc);
    }

    sg_apply_pipeline(pipeline);

    sg_bindings bindings = {
        .vertex_buffers[0] = cube_model->positions_buf,
        .vertex_buffers[1] = cube_model->normals_buf,
    };
    sg_apply_bindings(&bindings);

    single_color_vs_params_t vs_params = {
        .model_mat = mat4_transpose(mat4_box_to_line_transform(p0, p1, size)),
        .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
    };
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params, &vs_params, sizeof(vs_params));

    single_color_fs_params_t fs_params = {
        .color = V4(color.x, color.y, color.z, color.w),
        .kd_scale = 0.0f,
    };
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params, &fs_params, sizeof(fs_params));

    sg_draw(0, cube_model->num_points, 1);
}

static void renderer_draw_text(struct renderer *renderer, struct font *font, 
        vec2 pos, float size, vec4 color, const char *str, bool center_text) {
    struct model *square_model = asset_store_get_model("square");
    int i = 0;

    float total_width = 0.0f;
    while (str[i]) {
        float xadvance = (float)font->chars[(int)str[i]].xadvance;
        total_width += xadvance*size;
        i++;
    }
    if (center_text) {
        pos.x -= 0.5f*total_width;
    }

    i = 0;
    while (str[i]) {
        int c = (int)str[i];
        float x = (float) font->chars[c].x;
        float y = (float) font->chars[c].y;
        float width = (float) font->chars[c].width;
        float height = (float) font->chars[c].height;
        float xoffset = (float) font->chars[c].xoffset;
        float yoffset = (float) font->chars[c].yoffset;
        float xadvance = (float) font->chars[c].xadvance;

        vec2 p = V2(pos.x + 0.5f*width*size + xoffset*size, pos.y - 0.5f*height*size - yoffset*size);
        vec2 sz = V2(width*size, height*size);
        pos.x += xadvance*size;

        ui_vs_params_t vs_params = {
            .mvp_mat = mat4_transpose(mat4_multiply_n(3, 
                        renderer->ui_proj_mat,
                        mat4_translation(V3(p.x, p.y, 0.0f)),
                        mat4_scale(V3(0.5f*sz.x, 0.5f*sz.y, 1.0f)))),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_ui_vs_params, &vs_params, sizeof(vs_params));

        ui_fs_params_t fs_params = {
            .tex_x = x,
            .tex_y = y + height,
            .tex_dx = width,
            .tex_dy = -height,
            .is_font = 1.0f,
            .color = color,
        };
        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ui_fs_params, &fs_params, sizeof(fs_params));

        sg_draw(0, square_model->num_points, 1);

        i++;
    }
}

static void renderer_draw_ui_box(struct renderer *renderer, vec2 pos, vec2 size, 
        vec2 tl, vec2 t, vec2 tr, vec2 l, vec2 m, vec2 r, vec2 bl, vec2 b, vec2 br, float alpha) {
    struct model *square_model = asset_store_get_model("square");
    float w = 40.0f;

    {
        vec2 p;
        p.x = pos.x - (0.5f*size.x - 0.5f*w);
        p.y = pos.y + (0.5f*size.y - 0.5f*w);
        vec2 sz = V2(w, w);

        ui_vs_params_t vs_params = {
            .mvp_mat = mat4_transpose(mat4_multiply_n(3, 
                        renderer->ui_proj_mat,
                        mat4_translation(V3(p.x, p.y, 0.0f)),
                        mat4_scale(V3(0.5f*sz.x, 0.5f*sz.y, 1.0f)))),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_ui_vs_params, &vs_params, sizeof(vs_params));

        ui_fs_params_t fs_params = {
            .tex_x = 18.0f*tl.x,
            .tex_y = 18.0f*tl.y + 16.0f,
            .tex_dx = 16.0f,
            .tex_dy = -16.0f,
            .is_font = 0.0f,
            .color = V4(0.0f, 1.0f, 1.0f, alpha),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ui_fs_params, &fs_params, sizeof(fs_params));

        sg_draw(0, square_model->num_points, 1);
    }

    {
        vec2 p;
        p.x = pos.x - (0.5f*size.x - 0.5f*w);
        p.y = pos.y - (0.5f*size.y - 0.5f*w);
        vec2 sz = V2(w, w);

        ui_vs_params_t vs_params = {
            .mvp_mat = mat4_transpose(mat4_multiply_n(3, 
                        renderer->ui_proj_mat,
                        mat4_translation(V3(p.x, p.y, 0.0f)),
                        mat4_scale(V3(0.5f*sz.x, 0.5f*sz.y, 1.0f)))),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_ui_vs_params, &vs_params, sizeof(vs_params));

        ui_fs_params_t fs_params = {
            .tex_x = 18.0f*bl.x,
            .tex_y = 18.0f*bl.y + 16.0f,
            .tex_dx = 16.0f,
            .tex_dy = -16.0f,
            .is_font = 0.0f,
            .color = V4(0.0f, 1.0f, 1.0f, alpha),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ui_fs_params, &fs_params, sizeof(fs_params));

        sg_draw(0, square_model->num_points, 1);
    }

    {
        vec2 p;
        p.x = pos.x + (0.5f*size.x - 0.5f*w);
        p.y = pos.y - (0.5f*size.y - 0.5f*w);
        vec2 sz = V2(w, w);

        ui_vs_params_t vs_params = {
            .mvp_mat = mat4_transpose(mat4_multiply_n(3, 
                        renderer->ui_proj_mat,
                        mat4_translation(V3(p.x, p.y, 0.0f)),
                        mat4_scale(V3(0.5f*sz.x, 0.5f*sz.y, 1.0f)))),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_ui_vs_params, &vs_params, sizeof(vs_params));

        ui_fs_params_t fs_params = {
            .tex_x = 18.0f*br.x,
            .tex_y = 18.0f*br.y + 16.0f,
            .tex_dx = 16.0f,
            .tex_dy = -16.0f,
            .is_font = 0.0f,
            .color = V4(0.0f, 1.0f, 1.0f, alpha),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ui_fs_params, &fs_params, sizeof(fs_params));

        sg_draw(0, square_model->num_points, 1);
    }

    {
        vec2 p;
        p.x = pos.x + (0.5f*size.x - 0.5f*w);
        p.y = pos.y + (0.5f*size.y - 0.5f*w);
        vec2 sz = V2(w, w);

        ui_vs_params_t vs_params = {
            .mvp_mat = mat4_transpose(mat4_multiply_n(3, 
                        renderer->ui_proj_mat,
                        mat4_translation(V3(p.x, p.y, 0.0f)),
                        mat4_scale(V3(0.5f*sz.x, 0.5f*sz.y, 1.0f)))),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_ui_vs_params, &vs_params, sizeof(vs_params));

        ui_fs_params_t fs_params = {
            .tex_x = 18.0f*tr.x,
            .tex_y = 18.0f*tr.y + 16.0f,
            .tex_dx = 16.0f,
            .tex_dy = -16.0f,
            .is_font = 0.0f,
            .color = V4(0.0f, 1.0f, 1.0f, alpha),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ui_fs_params, &fs_params, sizeof(fs_params));

        sg_draw(0, square_model->num_points, 1);
    }

    {
        vec2 p;
        p.x = pos.x;
        p.y = pos.y + (0.5f*size.y - 0.5f*w);
        vec2 sz = V2(size.x - w, w);

        ui_vs_params_t vs_params = {
            .mvp_mat = mat4_transpose(mat4_multiply_n(3, 
                        renderer->ui_proj_mat,
                        mat4_translation(V3(p.x, p.y, 0.0f)),
                        mat4_scale(V3(0.5f*sz.x, 0.5f*sz.y, 1.0f)))),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_ui_vs_params, &vs_params, sizeof(vs_params));

        ui_fs_params_t fs_params = {
            .tex_x = 18.0f*t.x,
            .tex_y = 18.0f*t.y + 16.0f,
            .tex_dx = 16.0f,
            .tex_dy = -16.0f,
            .is_font = 0.0f,
            .color = V4(0.0f, 1.0f, 1.0f, alpha),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ui_fs_params, &fs_params, sizeof(fs_params));

        sg_draw(0, square_model->num_points, 1);
    }

    {
        vec2 p;
        p.x = pos.x;
        p.y = pos.y - (0.5f*size.y - 0.5f*w);
        vec2 sz = V2(size.x - w, w);

        ui_vs_params_t vs_params = {
            .mvp_mat = mat4_transpose(mat4_multiply_n(3, 
                        renderer->ui_proj_mat,
                        mat4_translation(V3(p.x, p.y, 0.0f)),
                        mat4_scale(V3(0.5f*sz.x, 0.5f*sz.y, 1.0f)))),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_ui_vs_params, &vs_params, sizeof(vs_params));

        ui_fs_params_t fs_params = {
            .tex_x = 18.0f*b.x,
            .tex_y = 18.0f*b.y + 16.0f,
            .tex_dx = 16.0f,
            .tex_dy = -16.0f,
            .is_font = 0.0f,
            .color = V4(0.0f, 1.0f, 1.0f, alpha),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ui_fs_params, &fs_params, sizeof(fs_params));

        sg_draw(0, square_model->num_points, 1);
    }

    {
        vec2 p;
        p.x = pos.x + (0.5f*size.x - 0.5f*w);
        p.y = pos.y;
        vec2 sz = V2(w, size.y - w);

        ui_vs_params_t vs_params = {
            .mvp_mat = mat4_transpose(mat4_multiply_n(3, 
                        renderer->ui_proj_mat,
                        mat4_translation(V3(p.x, p.y, 0.0f)),
                        mat4_scale(V3(0.5f*sz.x, 0.5f*sz.y, 1.0f)))),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_ui_vs_params, &vs_params, sizeof(vs_params));

        ui_fs_params_t fs_params = {
            .tex_x = 18.0f*r.x,
            .tex_y = 18.0f*r.y + 16.0f,
            .tex_dx = 16.0f,
            .tex_dy = -16.0f,
            .is_font = 0.0f,
            .color = V4(0.0f, 1.0f, 1.0f, alpha),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ui_fs_params, &fs_params, sizeof(fs_params));

        sg_draw(0, square_model->num_points, 1);
    }

    {
        vec2 p;
        p.x = pos.x - (0.5f*size.x - 0.5f*w);
        p.y = pos.y;
        vec2 sz = V2(w, size.y - w);

        ui_vs_params_t vs_params = {
            .mvp_mat = mat4_transpose(mat4_multiply_n(3, 
                        renderer->ui_proj_mat,
                        mat4_translation(V3(p.x, p.y, 0.0f)),
                        mat4_scale(V3(0.5f*sz.x, 0.5f*sz.y, 1.0f)))),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_ui_vs_params, &vs_params, sizeof(vs_params));

        ui_fs_params_t fs_params = {
            .tex_x = 18.0f*l.x,
            .tex_y = 18.0f*l.y + 16.0f,
            .tex_dx = 16.0f,
            .tex_dy = -16.0f,
            .is_font = 0.0f,
            .color = V4(0.0f, 1.0f, 1.0f, alpha),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ui_fs_params, &fs_params, sizeof(fs_params));

        sg_draw(0, square_model->num_points, 1);
    }

    {
        vec2 p;
        p.x = pos.x;
        p.y = pos.y;
        vec2 sz = V2(size.x - w, size.y - w);

        ui_vs_params_t vs_params = {
            .mvp_mat = mat4_transpose(mat4_multiply_n(3, 
                        renderer->ui_proj_mat,
                        mat4_translation(V3(p.x, p.y, 0.0f)),
                        mat4_scale(V3(0.5f*sz.x, 0.5f*sz.y, 1.0f)))),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_ui_vs_params, &vs_params, sizeof(vs_params));

        ui_fs_params_t fs_params = {
            .tex_x = 18.0f*m.x,
            .tex_y = 18.0f*m.y + 16.0f,
            .tex_dx = 16.0f,
            .tex_dy = -16.0f,
            .is_font = 0.0f,
            .color = V4(0.0f, 1.0f, 1.0f, alpha),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ui_fs_params, &fs_params, sizeof(fs_params));

        sg_draw(0, square_model->num_points, 1);
    }
}

static void renderer_draw_button(struct renderer *renderer, struct ui_button *button) {
    vec2 tl = V2(12.0f, 2.0f);
    vec2 t = V2(13.0f, 2.0f);
    vec2 tr = V2(14.0f, 2.0f); 
    vec2 l = V2(12.0f, 3.0f);
    vec2 m = V2(13.0f, 3.0f);
    vec2 r = V2(14.0f, 3.0f);
    vec2 bl = V2(12.0f, 4.0f);
    vec2 b = V2(13.0f, 4.0f);
    vec2 br = V2(14.0f, 4.0f);
    if (button->is_hovered) {
         bl = V2(15.0f, 4.0f); 
         b = V2(16.0f, 4.0f);
         br = V2(17.0f, 4.0f);
    }
    renderer_draw_ui_box(renderer, button->pos, button->size, 
            tl, t, tr, l, m, r, bl, b, br, 1.0f);
}


void renderer_draw_main_menu(struct renderer *renderer, struct main_menu *main_menu) {
    profiler_push_section("renderer_draw_main_menu");
    struct model *square_model = asset_store_get_model("square");

    {
        sg_pass_action action = {
            .colors[0] = {
                .action = SG_ACTION_DONTCARE,
                .val = { 0.529f, 0.808f, 0.922f, 1.0f },
            },
        };
        sg_begin_default_pass(&action, sapp_width(), sapp_height());
        sg_apply_pipeline(renderer->sokol.ui_pipeline);

        {
            sg_bindings bindings = {
                .vertex_buffers[0] = square_model->positions_buf,
                .vertex_buffers[1] = square_model->texture_coords_buf,
                .fs_images[SLOT_ui_texture] = asset_store_get_texture("UIpackSheet_transparent.png")->image,
            };
            sg_apply_bindings(&bindings);

            vec2 background_pos = config_get_vec2("main_menu_background_pos");
            vec2 background_size = config_get_vec2("main_menu_background_size");
            renderer_draw_ui_box(renderer, background_pos, background_size,
                    V2(13.0f, 13.0f), V2(14.0f, 13.0f), V2(15.0f, 13.0f), 
                    V2(13.0f, 14.0f), V2(14.0f, 14.0f), V2(15.0f, 14.0f),
                    V2(13.0f, 15.0f), V2(14.0f, 15.0f), V2(15.0f, 15.0f), 1.0f);
            renderer_draw_button(renderer, &main_menu->start_game_button);
        }

        {
            sg_bindings bindings = {
                .vertex_buffers[0] = square_model->positions_buf,
                .vertex_buffers[1] = square_model->texture_coords_buf,
                .fs_images[SLOT_ui_texture] = asset_store_get_texture("font_large.png")->image,
            };
            sg_apply_bindings(&bindings);

            vec2 pos;
            pos = config_get_vec2("main_menu_header_pos");
            pos.y += 55.0f;
            vec4 color = V4(0.0f, 0.0f, 0.0f, 1.0f);
            renderer_draw_text(renderer, &renderer->large_font, pos, 1.0f, color, "MINIGOLF", true);
        }

        {
            sg_bindings bindings = {
                .vertex_buffers[0] = square_model->positions_buf,
                .vertex_buffers[1] = square_model->texture_coords_buf,
                .fs_images[SLOT_ui_texture] = asset_store_get_texture("font_medium.png")->image,
            };
            sg_apply_bindings(&bindings);

            vec4 color = V4(0.0f, 0.0f, 0.0f, 1.0f);
            vec2 pos = main_menu->start_game_button.pos;
            pos.y += 25.0f;
            if (main_menu->start_game_button.is_hovered) {
                pos.y -= 5.0f;
            }
            renderer_draw_text(renderer, &renderer->medium_font, pos, 0.8f, color, "START", true);
        }

        {
            sg_bindings bindings = {
                .vertex_buffers[0] = square_model->positions_buf,
                .vertex_buffers[1] = square_model->texture_coords_buf,
                .fs_images[SLOT_ui_texture] = asset_store_get_texture("font_medium.png")->image,
            };
            sg_apply_bindings(&bindings);

            {
                vec4 color = V4(0.1f, 0.2f, 0.1f, 1.0f);
                vec2 pos = config_get_vec2("main_menu_controls_text_pos");
                float size = config_get_float("main_menu_controls_text_size");
                const char *text = "Controls:";
                renderer_draw_text(renderer, &renderer->medium_font, pos, size, color, text, false);
            }

            {
                vec4 color = V4(0.1f, 0.2f, 0.1f, 1.0f);
                vec2 pos = config_get_vec2("main_menu_controls_1_text_pos");
                float size = config_get_float("main_menu_controls_1_text_size");
                const char *text = "A and D to rotate the camera.";
                renderer_draw_text(renderer, &renderer->medium_font, pos, size, color, text, false);
            }

            {
                vec4 color = V4(0.1f, 0.2f, 0.1f, 1.0f);
                vec2 pos = config_get_vec2("main_menu_controls_2_text_pos");
                float size = config_get_float("main_menu_controls_2_text_size");
                const char *text = "TAB to view the scorecard.";
                renderer_draw_text(renderer, &renderer->medium_font, pos, size, color, text, false);
            }
        }

        sg_end_pass();
    }

    profiler_pop_section();
}

void renderer_draw_game(struct renderer *renderer, struct game *game, struct game_editor *ed) {
    profiler_push_section("renderer_draw_game");

    {
        sg_pass_action game_pass_action = {
            .colors[0] = {
                .action = SG_ACTION_CLEAR,
                .val = { 0.529f, 0.808f, 0.922f, 1.0f },
            },
        };
        sg_begin_pass(renderer->sokol.game_pass, &game_pass_action);

        profiler_push_section("draw environments");
        {
            sg_apply_pipeline(renderer->sokol.environment_pipeline);

            struct lightmap *lightmap = &game->hole.environment_lightmap;
            int lightmap_uvs_offset = 0;
            for (int i = 0; i < game->hole.environment_entities.length; i++) {
                struct environment_entity *entity = &game->hole.environment_entities.data[i];
                struct model *model = entity->model;

                sg_bindings bindings = {
                    .vertex_buffers[0] = model->positions_buf,
                    .vertex_buffers[1] = model->texture_coords_buf,
                    .vertex_buffers[2] = lightmap->uvs_buf,
                    .vertex_buffer_offsets[2] = lightmap_uvs_offset,
                    .fs_images[SLOT_environment_lightmap_tex] = lightmap->images.data[0].sg_image,
                    .fs_images[SLOT_environment_material_tex] = 
                        asset_store_get_texture("environment_material.bmp")->image,
                };
                sg_apply_bindings(&bindings);

                environment_vs_params_t vs_params = {
                    .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                    .model_mat = mat4_transpose(environment_entity_get_transform(entity)),
                };
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_environment_vs_params, &vs_params, sizeof(vs_params));

                vec3 bp = game->player_ball.draw_position;
                environment_fs_params_t fs_params = {
                    .ball_position = V4(bp.x, bp.y, bp.z, 1.0),
                };
                sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_environment_fs_params, &fs_params, sizeof(fs_params));

                sg_draw(0, model->num_points, 1);
                lightmap_uvs_offset += sizeof(vec2)*model->num_points;
            }
        }
        profiler_pop_section();

        {
            sg_apply_pipeline(renderer->sokol.terrain_pipeline[0]);
            for (int i = 0; i < game->hole.terrain_entities.length; i++) {
                struct terrain_entity *entity = &game->hole.terrain_entities.data[i];
                struct terrain_model *model = &entity->terrain_model;
                struct lightmap *lightmap = &entity->lightmap;

                sg_bindings bindings = {
                    .vertex_buffers[0] = model->positions_buf,
                    .vertex_buffers[1] = model->normals_buf,
                    .vertex_buffers[2] = model->texture_coords_buf,
                    .vertex_buffers[3] = lightmap->uvs_buf,
                    .vertex_buffers[4] = model->material_idxs_buf,
                    .fs_images[SLOT_lightmap_tex0] = lightmap->images.data[0].sg_image,
                    .fs_images[SLOT_lightmap_tex1] = lightmap->images.data[0].sg_image,
                    .fs_images[SLOT_mat_tex0] = asset_store_get_texture("ground.png")->image,
                    .fs_images[SLOT_mat_tex1] = asset_store_get_texture("wood.jpg")->image,
                };
                sg_apply_bindings(&bindings);

                vec3 c0[MAX_NUM_TERRAIN_MODEL_MATERIALS];
                vec3 c1[MAX_NUM_TERRAIN_MODEL_MATERIALS];
                for (int i = 0; i < MAX_NUM_TERRAIN_MODEL_MATERIALS; i++) {
                    c0[i] = model->materials[i].color0;
                    c1[i] = model->materials[i].color1;
                }
                terrain_vs_params_t vs_params = {
                    .model_mat = mat4_transpose(terrain_entity_get_transform(entity)),
                    .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                    .color0[0] = V4(c0[0].x, c0[0].y, c0[0].z, 1.0f),
                    .color0[1] = V4(c0[1].x, c0[1].y, c0[1].z, 1.0f),
                    .color0[2] = V4(c0[2].x, c0[2].y, c0[2].z, 1.0f),
                    .color0[3] = V4(c0[3].x, c0[3].y, c0[3].z, 1.0f),
                    .color0[4] = V4(c0[4].x, c0[4].y, c0[4].z, 1.0f),
                    .color1[0] = V4(c1[0].x, c1[0].y, c1[0].z, 1.0f),
                    .color1[1] = V4(c1[1].x, c1[1].y, c1[1].z, 1.0f),
                    .color1[2] = V4(c1[2].x, c1[2].y, c1[2].z, 1.0f),
                    .color1[3] = V4(c1[3].x, c1[3].y, c1[3].z, 1.0f),
                    .color1[4] = V4(c1[4].x, c1[4].y, c1[4].z, 1.0f),
                };
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_terrain_vs_params, &vs_params, sizeof(vs_params));

                vec3 ball_pos = game->player_ball.draw_position;
                terrain_fs_params_t fs_params = {
                    .ball_position = V4(ball_pos.x, ball_pos.y, ball_pos.z, 0.0),
                    .lightmap_t = 0.0f,
                    .opacity = 1.0f,
                };
                sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_terrain_fs_params, &fs_params, sizeof(fs_params));

                sg_draw(0, model->num_elements, 1);
            }

            for (int i = 0; i < game->hole.multi_terrain_entities.length; i++) {
                struct multi_terrain_entity *entity = &game->hole.multi_terrain_entities.data[i];
                struct terrain_model *static_model = &entity->static_terrain_model;
                struct terrain_model *moving_model = &entity->moving_terrain_model;
                struct lightmap *static_lightmap = &entity->static_lightmap;
                struct lightmap *moving_lightmap = &entity->moving_lightmap;

                float t = fmodf(game->t, entity->movement_data.length)/entity->movement_data.length;
                if (entity->movement_data.type == MOVEMENT_TYPE_PENDULUM ||
                        entity->movement_data.type == MOVEMENT_TYPE_TO_AND_FROM ||
                        entity->movement_data.type == MOVEMENT_TYPE_RAMP) {
                    // For these movement types the second half of the movement is just the first have but in reverse 
                    t = 2.0f*t;
                    if (t > 1.0f) {
                        t = 2.0f - t;
                    }
                }

                {
                    int static_lightmap_i0 = 0, static_lightmap_i1 = 1;
                    for (int i = 1; i < static_lightmap->images.length; i++) {
                        if (t < i / ((float) (static_lightmap->images.length - 1))) {
                            static_lightmap_i0 = i - 1;
                            static_lightmap_i1 = i;
                            break;
                        }
                    }
                    float static_lightmap_t = 0.0f;
                    float static_lightmap_t0 =
                        static_lightmap_i0 / ((float) (static_lightmap->images.length - 1));
                    float static_lightmap_t1 =
                        static_lightmap_i1 / ((float) (static_lightmap->images.length - 1));
                    static_lightmap_t = (t - static_lightmap_t0) / (static_lightmap_t1 - static_lightmap_t0);
                    if (static_lightmap_t < 0.0f) static_lightmap_t = 0.0f;
                    if (static_lightmap_t > 1.0f) static_lightmap_t = 1.0f;

                    sg_bindings bindings = {
                        .vertex_buffers[0] = static_model->positions_buf,
                        .vertex_buffers[1] = static_model->normals_buf,
                        .vertex_buffers[2] = static_model->texture_coords_buf,
                        .vertex_buffers[3] = static_lightmap->uvs_buf,
                        .vertex_buffers[4] = static_model->material_idxs_buf,
                        .fs_images[SLOT_lightmap_tex0] =
                            static_lightmap->images.data[static_lightmap_i0].sg_image,
                        .fs_images[SLOT_lightmap_tex1] =
                            static_lightmap->images.data[static_lightmap_i1].sg_image,
                        .fs_images[SLOT_mat_tex0] = asset_store_get_texture("ground.png")->image,
                        .fs_images[SLOT_mat_tex1] = asset_store_get_texture("wood.jpg")->image,
                    };
                    sg_apply_bindings(&bindings);

                    vec3 c0[MAX_NUM_TERRAIN_MODEL_MATERIALS];
                    vec3 c1[MAX_NUM_TERRAIN_MODEL_MATERIALS];
                    for (int i = 0; i < MAX_NUM_TERRAIN_MODEL_MATERIALS; i++) {
                        c0[i] = static_model->materials[i].color0;
                        c1[i] = static_model->materials[i].color1;
                    }

                    terrain_vs_params_t vs_params = {
                        .model_mat = mat4_transpose(multi_terrain_entity_get_static_transform(entity)),
                        .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                        .color0[0] = V4(c0[0].x, c0[0].y, c0[0].z, 1.0f),
                        .color0[1] = V4(c0[1].x, c0[1].y, c0[1].z, 1.0f),
                        .color0[2] = V4(c0[2].x, c0[2].y, c0[2].z, 1.0f),
                        .color0[3] = V4(c0[3].x, c0[3].y, c0[3].z, 1.0f),
                        .color0[4] = V4(c0[4].x, c0[4].y, c0[4].z, 1.0f),
                        .color1[0] = V4(c1[0].x, c1[0].y, c1[0].z, 1.0f),
                        .color1[1] = V4(c1[1].x, c1[1].y, c1[1].z, 1.0f),
                        .color1[2] = V4(c1[2].x, c1[2].y, c1[2].z, 1.0f),
                        .color1[3] = V4(c1[3].x, c1[3].y, c1[3].z, 1.0f),
                        .color1[4] = V4(c1[4].x, c1[4].y, c1[4].z, 1.0f),
                    };
                    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_terrain_vs_params, 
                            &vs_params, sizeof(vs_params));

                    vec3 ball_pos = game->player_ball.draw_position;
                    terrain_fs_params_t fs_params = {
                        .ball_position = V4(ball_pos.x, ball_pos.y, ball_pos.z, 0.0),
                        .lightmap_t = static_lightmap_t,
                    };
                    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_terrain_fs_params, 
                            &fs_params, sizeof(fs_params));

                    sg_draw(0, static_model->num_elements, 1);
                }

                {
                    int moving_lightmap_i0 = 0, moving_lightmap_i1 = 1;
                    for (int i = 1; i < moving_lightmap->images.length; i++) {
                        if (t < i / ((float) (moving_lightmap->images.length - 1))) {
                            moving_lightmap_i0 = i - 1;
                            moving_lightmap_i1 = i;
                            break;
                        }
                    }
                    float moving_lightmap_t = 0.0f;
                    float moving_lightmap_t0 =
                        moving_lightmap_i0 / ((float) (moving_lightmap->images.length - 1));
                    float moving_lightmap_t1 =
                        moving_lightmap_i1 / ((float) (moving_lightmap->images.length - 1));
                    moving_lightmap_t =
                        (t - moving_lightmap_t0) / (moving_lightmap_t1 - moving_lightmap_t0);
                    if (moving_lightmap_t < 0.0f) moving_lightmap_t = 0.0f;
                    if (moving_lightmap_t > 1.0f) moving_lightmap_t = 1.0f;

                    sg_bindings bindings = {
                        .vertex_buffers[0] = moving_model->positions_buf,
                        .vertex_buffers[1] = moving_model->normals_buf,
                        .vertex_buffers[2] = moving_model->texture_coords_buf,
                        .vertex_buffers[3] = moving_lightmap->uvs_buf,
                        .vertex_buffers[4] = moving_model->material_idxs_buf,
                        .fs_images[SLOT_lightmap_tex0] =
                            moving_lightmap->images.data[moving_lightmap_i0].sg_image,
                        .fs_images[SLOT_lightmap_tex1] =
                            moving_lightmap->images.data[moving_lightmap_i1].sg_image,
                        .fs_images[SLOT_mat_tex0] = asset_store_get_texture("ground.png")->image,
                        .fs_images[SLOT_mat_tex1] = asset_store_get_texture("wood.jpg")->image,
                    };
                    sg_apply_bindings(&bindings);

                    vec3 c0[MAX_NUM_TERRAIN_MODEL_MATERIALS];
                    vec3 c1[MAX_NUM_TERRAIN_MODEL_MATERIALS];
                    for (int i = 0; i < MAX_NUM_TERRAIN_MODEL_MATERIALS; i++) {
                        c0[i] = moving_model->materials[i].color0;
                        c1[i] = moving_model->materials[i].color1;
                    }

                    terrain_vs_params_t vs_params = {
                        .model_mat = mat4_transpose(multi_terrain_entity_get_moving_transform(entity, game->t)),
                        .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                        .color0[0] = V4(c0[0].x, c0[0].y, c0[0].z, 1.0f),
                        .color0[1] = V4(c0[1].x, c0[1].y, c0[1].z, 1.0f),
                        .color0[2] = V4(c0[2].x, c0[2].y, c0[2].z, 1.0f),
                        .color0[3] = V4(c0[3].x, c0[3].y, c0[3].z, 1.0f),
                        .color0[4] = V4(c0[4].x, c0[4].y, c0[4].z, 1.0f),
                        .color1[0] = V4(c1[0].x, c1[0].y, c1[0].z, 1.0f),
                        .color1[1] = V4(c1[1].x, c1[1].y, c1[1].z, 1.0f),
                        .color1[2] = V4(c1[2].x, c1[2].y, c1[2].z, 1.0f),
                        .color1[3] = V4(c1[3].x, c1[3].y, c1[3].z, 1.0f),
                        .color1[4] = V4(c1[4].x, c1[4].y, c1[4].z, 1.0f),
                    };
                    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_terrain_vs_params, 
                            &vs_params, sizeof(vs_params));

                    vec3 ball_pos = game->player_ball.draw_position;
                    terrain_fs_params_t fs_params = {
                        .ball_position = V4(ball_pos.x, ball_pos.y, ball_pos.z, 0.0),
                        .lightmap_t = moving_lightmap_t,
                    };
                    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_terrain_fs_params, 
                            &fs_params, sizeof(fs_params));

                    sg_draw(0, moving_model->num_elements, 1);
                }
            }
        }
        {
            struct ball_entity *ball = &game->player_ball;
            mat4 transform = mat4_multiply_n(3,
                    mat4_translation(ball->draw_position),
                    mat4_scale(V3(ball->radius, ball->radius, ball->radius)),
                    mat4_from_quat(ball->orientation));
            vec3 color = ball->color;
            struct model *model = asset_store_get_model("golf_ball");
            struct texture *texture = asset_store_get_texture("golf_ball_normal_map_128x128.jpg");

            sg_apply_pipeline(renderer->sokol.ball_pipeline[0]);

            sg_bindings bindings = {
                .vertex_buffers[0] = model->positions_buf,
                .vertex_buffers[1] = model->normals_buf,
                .vertex_buffers[2] = model->texture_coords_buf,
                .fs_images[SLOT_normal_map] = texture->image,
            };
            sg_apply_bindings(&bindings);

            ball_vs_params_t vs_params = {
                .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                .model_mat = mat4_transpose(transform),
            };
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_ball_vs_params, &vs_params, sizeof(vs_params));

            ball_fs_params_t fs_params = {
                .color = V4(color.x, color.y, color.z, 1.0f),
            };
            sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ball_fs_params, &fs_params, sizeof(fs_params));

            sg_draw(0, model->num_points, 1);
        }
        {
            struct ball_entity *ball = &game->player_ball;
            mat4 transform = mat4_multiply_n(2,
                    mat4_translation(ball->draw_position),
                    mat4_scale(V3(ball->radius + 0.001f, ball->radius + 0.001f, ball->radius + 0.001f)));
            struct model *model = asset_store_get_model("golf_ball");

            sg_apply_pipeline(renderer->sokol.occluded_ball_pipeline);

            sg_bindings bindings = {
                .vertex_buffers[0] = model->positions_buf,
                .vertex_buffers[1] = model->normals_buf,
            };
            sg_apply_bindings(&bindings);

            occluded_ball_vs_params_t vs_params = {
                .model_mat = mat4_transpose(transform),
                .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
            };
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_occluded_ball_vs_params, &vs_params, sizeof(vs_params));

            vec3 bp = ball->draw_position;
            vec3 cp = renderer->cam_pos;
            occluded_ball_fs_params_t fs_params = {
                .ball_position = V4(bp.x, bp.y, bp.z, 0.0f),
                .cam_position = V4(cp.x, cp.y, cp.z, 0.0f),
            };
            sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_occluded_ball_fs_params, &fs_params, sizeof(fs_params));

            sg_draw(0, model->num_points, 1);
        }
        if (ed->physics.draw_triangles) {
            struct model *cube_model = asset_store_get_model("cube");

            sg_apply_pipeline(renderer->sokol.physics_debug_pipeline);

            sg_bindings bindings = {
                .vertex_buffers[0] = cube_model->positions_buf,
                .vertex_buffers[1] = cube_model->normals_buf,
            };
            sg_apply_bindings(&bindings);

            single_color_fs_params_t fs_params = {
                .color = V4(1.0f, 1.0f, 1.0f, 1.0f),
                .kd_scale = 0.0f,
            };
            sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params,
                    &fs_params, sizeof(fs_params));

            float size = 0.01f;
            for (int i = 0; i < game->physics.hole_triangles.length; i++) {
                vec3 a = game->physics.hole_triangles.data[i].a;
                vec3 b = game->physics.hole_triangles.data[i].b;
                vec3 c = game->physics.hole_triangles.data[i].c;

                single_color_vs_params_t vs_params = {
                    .model_mat = mat4_transpose(mat4_box_to_line_transform(a, b, size)),
                    .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                };
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                        &vs_params, sizeof(vs_params));
                sg_draw(0, cube_model->num_points, 1);

                vs_params = (single_color_vs_params_t) {
                    .model_mat = mat4_transpose(mat4_box_to_line_transform(b, c, size)),
                        .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                };
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                        &vs_params, sizeof(vs_params));
                sg_draw(0, cube_model->num_points, 1);

                vs_params = (single_color_vs_params_t) {
                    .model_mat = mat4_transpose(mat4_box_to_line_transform(c, a, size)),
                        .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                };
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                        &vs_params, sizeof(vs_params));
                sg_draw(0, cube_model->num_points, 1);
            }

            for (int i = 0; i < game->physics.hole_triangles.length; i++) {
                vec3 a = game->physics.hole_triangles.data[i].a;
                vec3 b = game->physics.hole_triangles.data[i].b;
                vec3 c = game->physics.hole_triangles.data[i].c;

                single_color_vs_params_t vs_params = {
                    .model_mat = mat4_transpose(mat4_box_to_line_transform(a, b, size)),
                    .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                };
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                        &vs_params, sizeof(vs_params));
                sg_draw(0, cube_model->num_points, 1);

                vs_params = (single_color_vs_params_t) {
                    .model_mat = mat4_transpose(mat4_box_to_line_transform(b, c, size)),
                        .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                };
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                        &vs_params, sizeof(vs_params));
                sg_draw(0, cube_model->num_points, 1);

                vs_params = (single_color_vs_params_t) {
                    .model_mat = mat4_transpose(mat4_box_to_line_transform(c, a, size)),
                        .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                };
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                        &vs_params, sizeof(vs_params));
                sg_draw(0, cube_model->num_points, 1);
            }
        }
        if (ed->physics.draw_cup_debug) {
            struct model *sphere_model = asset_store_get_model("sphere");

            sg_apply_pipeline(renderer->sokol.physics_debug_pipeline);

            sg_bindings bindings = {
                .vertex_buffers[0] = sphere_model->positions_buf,
                .vertex_buffers[1] = sphere_model->normals_buf,
            };
            sg_apply_bindings(&bindings);

            single_color_fs_params_t fs_params = {
                .color = V4(1.0f, 0.0f, 0.0f, 0.3f),
                .kd_scale = 0.0f,
            };
            sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params,
                    &fs_params, sizeof(fs_params));

            float r = ed->game->hole.cup_entity.radius;
            single_color_vs_params_t vs_params = {
                .model_mat = mat4_transpose(mat4_multiply(
                            mat4_translation(ed->game->hole.cup_entity.position),
                            mat4_scale(V3(r, r, r)))),
                .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
            };
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                    &vs_params, sizeof(vs_params));
            sg_draw(0, sphere_model->num_points, 1);
        }
        if (ed->physics.debug_collisions) {
            struct model *sphere_model = asset_store_get_model("sphere");
            struct model *cube_model = asset_store_get_model("cube");

            sg_apply_pipeline(renderer->sokol.physics_debug_pipeline);

            for (int i = 0; i < ed->physics.collision_data_array.length; i++) {
                struct physics_collision_data data = ed->physics.collision_data_array.data[i];

                {
                    sg_bindings bindings = {
                        .vertex_buffers[0] = sphere_model->positions_buf,
                        .vertex_buffers[1] = sphere_model->normals_buf,
                    };
                    sg_apply_bindings(&bindings);

                    single_color_vs_params_t vs_params = {
                        .model_mat = mat4_transpose(mat4_multiply(
                                    mat4_translation(data.ball_pos),
                                    mat4_scale(V3(0.01f, 0.01f, 0.01f)))),
                        .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                    };
                    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                            &vs_params, sizeof(vs_params));

                    single_color_fs_params_t fs_params = {
                        .color = V4(1.0f, 1.0f, 1.0f, 1.0f),
                        .kd_scale = 0.0f,
                    };
                    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params,
                            &fs_params, sizeof(fs_params));

                    sg_draw(0, sphere_model->num_points, 1);
                }

                if (i == ed->physics.selected_collision_data_idx) {
                    for (int j = 0; j < data.num_ball_contacts; j++) {
                        struct ball_contact contact = data.ball_contacts[j];

                        {
                            sg_bindings bindings = {
                                .vertex_buffers[0] = sphere_model->positions_buf,
                                .vertex_buffers[1] = sphere_model->normals_buf,
                            };
                            sg_apply_bindings(&bindings);

                            single_color_vs_params_t vs_params = {
                                .model_mat = mat4_transpose(mat4_multiply(
                                            mat4_translation(contact.position),
                                            mat4_scale(V3(0.01f, 0.01f, 0.01f)))),
                                .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                            };
                            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                                    &vs_params, sizeof(vs_params));

                            single_color_fs_params_t fs_params = {
                                .color = V4(1.0f, 0.0f, 0.0f, 1.0f),
                                .kd_scale = 0.0f,
                            };
                            sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params,
                                    &fs_params, sizeof(fs_params));

                            sg_draw(0, sphere_model->num_points, 1);
                        }

                        {
                            float line_sz = 0.012f;
                            sg_bindings bindings = {
                                .vertex_buffers[0] = cube_model->positions_buf,
                                .vertex_buffers[1] = cube_model->normals_buf,
                            };
                            sg_apply_bindings(&bindings);

                            single_color_fs_params_t fs_params = {
                                .color = V4(1.0f, 0.0f, 0.0f, 1.0f),
                                .kd_scale = 0.0f,
                            };
                            sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params,
                                    &fs_params, sizeof(fs_params));

                            single_color_vs_params_t vs_params = {
                                .model_mat = mat4_transpose(
                                        mat4_box_to_line_transform(contact.triangle_a, contact.triangle_b,
                                            line_sz)),
                                .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                            };
                            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                                    &vs_params, sizeof(vs_params));
                            sg_draw(0, cube_model->num_points, 1);

                            vs_params = (single_color_vs_params_t) {
                                .model_mat = mat4_transpose(
                                        mat4_box_to_line_transform(contact.triangle_b, contact.triangle_c,
                                            line_sz)),
                                    .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                            };
                            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                                    &vs_params, sizeof(vs_params));
                            sg_draw(0, cube_model->num_points, 1);

                            vs_params = (single_color_vs_params_t) {
                                .model_mat = mat4_transpose(
                                        mat4_box_to_line_transform(contact.triangle_c, contact.triangle_a,
                                            line_sz)),
                                    .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                            };
                            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                                    &vs_params, sizeof(vs_params));
                            sg_draw(0, cube_model->num_points, 1);
                        }
                    }
                }
            }
        }
        if (ed->physics.draw_triangle_chunks) {
            struct model *cube_model = asset_store_get_model("cube");
            vec3 pos = ed->game->physics.grid.corner_pos;
            float cell_size = ed->game->physics.grid.cell_size;
            int num_cols = ed->game->physics.grid.num_cols;
            int num_rows = ed->game->physics.grid.num_rows;

            {
                sg_apply_pipeline(renderer->sokol.physics_debug_pipeline);

                sg_bindings bindings = {
                    .vertex_buffers[0] = cube_model->positions_buf,
                    .vertex_buffers[1] = cube_model->normals_buf,
                };
                sg_apply_bindings(&bindings);

                single_color_fs_params_t fs_params = {
                    .color = V4(1.0f, 0.0f, 0.0f, 1.0f),
                    .kd_scale = 0.0f,
                };
                sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params,
                        &fs_params, sizeof(fs_params));
            }

            for (int i = 0; i <= num_cols; i++) {
                vec3 p0;
                p0.x = pos.x + i*cell_size;
                p0.y = pos.y; 
                p0.z = pos.z; 

                vec3 p1;
                p1.x = pos.x + i*cell_size;
                p1.y = pos.y; 
                p1.z = pos.z + num_cols*cell_size; 

                {
                    float line_sz = 0.012f;
                    single_color_vs_params_t vs_params = {
                        .model_mat = mat4_transpose(
                                mat4_box_to_line_transform(p0, p1, line_sz)),
                        .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                    };
                    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                            &vs_params, sizeof(vs_params));
                    sg_draw(0, cube_model->num_points, 1);
                }
            }

            for (int i = 0; i <= num_rows; i++) {
                vec3 p0;
                p0.x = pos.x; 
                p0.y = pos.y; 
                p0.z = pos.z + i*cell_size;

                vec3 p1;
                p1.x = pos.x + num_rows*cell_size; 
                p1.y = pos.y; 
                p1.z = pos.z + i*cell_size;

                {
                    float line_sz = 0.012f;
                    single_color_vs_params_t vs_params = {
                        .model_mat = mat4_transpose(
                                mat4_box_to_line_transform(p0, p1, line_sz)),
                        .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                    };
                    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                            &vs_params, sizeof(vs_params));
                    sg_draw(0, cube_model->num_points, 1);
                }
            }

            {
                vec3 bp = game->player_ball.position;
                float cell_dx = bp.x - pos.x - 0.5f*cell_size;
                float cell_dz = bp.z - pos.z - 0.5f*cell_size; 

                int col0 = (int) floorf(cell_dx/cell_size);
                int row0 = (int) floorf(cell_dz/cell_size);
                int col1 = (int) ceilf(cell_dx/cell_size);
                int row1 = (int) ceilf(cell_dz/cell_size);

                float x0 = pos.x + (col0 + 0.5f)*cell_size;
                float z0 = pos.z + (row0 + 0.5f)*cell_size;
                float x1 = pos.x + (col0 + 0.5f)*cell_size;
                float z1 = pos.z + (row1 + 0.5f)*cell_size;
                float x2 = pos.x + (col1 + 0.5f)*cell_size;
                float z2 = pos.z + (row0 + 0.5f)*cell_size;
                float x3 = pos.x + (col1 + 0.5f)*cell_size;
                float z3 = pos.z + (row1 + 0.5f)*cell_size;

                float width = 0.5f*cell_size;
                float height = 0.5f*cell_size;

                {
                    sg_apply_pipeline(renderer->sokol.physics_debug_pipeline);

                    sg_bindings bindings = {
                        .vertex_buffers[0] = cube_model->positions_buf,
                        .vertex_buffers[1] = cube_model->normals_buf,
                    };
                    sg_apply_bindings(&bindings);

                    single_color_fs_params_t fs_params = {
                        .color = V4(1.0f, 0.0f, 0.0f, 0.3f),
                        .kd_scale = 0.0f,
                    };
                    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params,
                            &fs_params, sizeof(fs_params));
                }

                {
                    single_color_vs_params_t vs_params = {
                        .model_mat = mat4_transpose(
                                mat4_multiply_n(2,
                                    mat4_translation(V3(x0, 0.0f, z0)),
                                    mat4_scale(V3(width, 100.0f, height)))),
                        .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                    };
                    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                            &vs_params, sizeof(vs_params));
                    sg_draw(0, cube_model->num_points, 1);
                }

                {
                    single_color_vs_params_t vs_params = {
                        .model_mat = mat4_transpose(
                                mat4_multiply_n(2,
                                    mat4_translation(V3(x1, 0.0f, z1)),
                                    mat4_scale(V3(width, 100.0f, height)))),
                        .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                    };
                    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                            &vs_params, sizeof(vs_params));
                    sg_draw(0, cube_model->num_points, 1);
                }

                {
                    single_color_vs_params_t vs_params = {
                        .model_mat = mat4_transpose(
                                mat4_multiply_n(2,
                                    mat4_translation(V3(x2, 0.0f, z2)),
                                    mat4_scale(V3(width, 100.0f, height)))),
                        .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                    };
                    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                            &vs_params, sizeof(vs_params));
                    sg_draw(0, cube_model->num_points, 1);
                }

                {
                    single_color_vs_params_t vs_params = {
                        .model_mat = mat4_transpose(
                                mat4_multiply_n(2,
                                    mat4_translation(V3(x3, 0.0f, z3)),
                                    mat4_scale(V3(width, 100.0f, height)))),
                        .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                    };
                    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                            &vs_params, sizeof(vs_params));
                    sg_draw(0, cube_model->num_points, 1);
                }
            }
        }
        sg_end_pass();
    }

    {
        sg_pass_action water_pass_action = {
            .colors[0] = { .action = SG_ACTION_DONTCARE },
            .depth = {
                .action = SG_ACTION_DONTCARE,
            },
            .stencil = {
                .action = SG_ACTION_CLEAR,
            },
        };
        sg_begin_pass(renderer->sokol.game_pass, &water_pass_action);
        {
            sg_apply_pipeline(renderer->sokol.water_pipeline);
            for (int i = 0; i < game->hole.water_entities.length; i++) {
                struct water_entity *entity = &game->hole.water_entities.data[i];
                struct terrain_model *model = &entity->model;
                struct lightmap *lightmap = &entity->lightmap;

                sg_bindings bindings = {
                    .vertex_buffers[0] = model->positions_buf,
                    .vertex_buffers[1] = model->texture_coords_buf,
                    .vertex_buffers[2] = lightmap->uvs_buf,
                    .fs_images[SLOT_water_lightmap_tex] =
                        lightmap->images.data[0].sg_image,
                    .fs_images[SLOT_water_noise_tex0] =
                        asset_store_get_texture("water_noise_1.png")->image,
                    .fs_images[SLOT_water_noise_tex1] =
                        asset_store_get_texture("water_noise_2.png")->image,
                };
                sg_apply_bindings(&bindings);

                water_vs_params_t vs_params = {
                    .model_mat = mat4_transpose(water_entity_get_transform(entity)),
                    .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                };
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_water_vs_params,
                        &vs_params, sizeof(vs_params));

                water_fs_params_t fs_params = {
                    .t = game->drawing.water_t,
                };
                sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_water_fs_params,
                        &fs_params, sizeof(fs_params));

                sg_draw(0, model->num_elements, 1);
            }
        }
        {
            static float t = 0.0f;
            t += 1.0f / 60.0f;

            struct ball_entity *ball = &game->player_ball;
            if (ball->in_water) {
                sg_apply_pipeline(renderer->sokol.water_around_ball_pipeline);

                struct model *model = asset_store_get_model("square");
                sg_bindings bindings = {
                    .vertex_buffers[0] = model->positions_buf,
                    .vertex_buffers[1] = model->texture_coords_buf,
                    .fs_images[SLOT_water_around_ball_noise_tex] =
                        asset_store_get_texture("water_noise_3.png")->image,
                };
                sg_apply_bindings(&bindings);

                water_around_ball_vs_params_t vs_params = {
                    .mvp_mat = mat4_transpose(
                            mat4_multiply_n(4, 
                                renderer->proj_view_mat,
                                mat4_translation(vec3_sub(ball->draw_position, V3(0.0f, 0.02f, 0.0f))),
                                mat4_scale(V3(0.4f, 0.4f, 0.4f)),
                                mat4_rotation_x(-0.5f * MF_PI))),
                };
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_water_around_ball_vs_params,
                        &vs_params, sizeof(vs_params));

                water_around_ball_fs_params_t fs_params = {
                    .t = t,
                };
                sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_water_around_ball_fs_params,
                        &fs_params, sizeof(fs_params));

                sg_draw(0, model->num_points, 1);
            }
        }
        {
            sg_apply_pipeline(renderer->sokol.water_ripple_pipeline);
            for (int i = 0; i < GAME_MAX_NUM_WATER_RIPPLES; i++) {
                float t = game->drawing.water_ripples[i].t;
                if (t > game->drawing.water_ripple_t_length) {
                    continue;
                }

                vec4 color = V4(0.0f, 0.0f, 0.0f, 0.0f);
                if (i % 4 == 0) {
                    color = config_get_vec4("water_ripple_color_0");
                }
                else if (i % 4 == 1) {
                    color = config_get_vec4("water_ripple_color_1");
                }
                else if (i % 4 == 2) {
                    color = config_get_vec4("water_ripple_color_2");
                }
                else if (i % 4 == 3) {
                    color = config_get_vec4("water_ripple_color_3");
                }

                struct model *model = asset_store_get_model("square");
                sg_bindings bindings = {
                    .vertex_buffers[0] = model->positions_buf,
                    .vertex_buffers[1] = model->texture_coords_buf,
                    .fs_images[SLOT_water_ripple_noise_tex] =
                        asset_store_get_texture("water_noise_3.png")->image,
                };
                sg_apply_bindings(&bindings);

                water_ripple_vs_params_t vs_params = {
                    .mvp_mat = mat4_transpose(
                            mat4_multiply_n(4, 
                                renderer->proj_view_mat,
                                mat4_translation(game->drawing.water_ripples[i].pos),
                                mat4_scale(V3(0.4f, 0.4f, 0.4f)),
                                mat4_rotation_x(-0.5f * MF_PI))),
                };
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_water_ripple_vs_params,
                        &vs_params, sizeof(vs_params));

                water_ripple_fs_params_t fs_params = {
                    .t = t,
                    .uniform_color = color,
                };
                sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_water_ripple_fs_params,
                        &fs_params, sizeof(fs_params));

                sg_draw(0, model->num_points, 1);
            }
        }
        sg_end_pass();
    }

    if (game->state == GAME_STATE_AIMING) {
        sg_pass_action aim_direction_action = {
            .colors[0] = { .action = SG_ACTION_DONTCARE },
            .depth = {
                .action = SG_ACTION_DONTCARE,
            },
            .stencil = {
                .action = SG_ACTION_DONTCARE,
            },
        };
        sg_begin_pass(renderer->sokol.game_pass, &aim_direction_action);
        sg_apply_pipeline(renderer->sokol.aim_helper_pipeline);

        struct model *model = asset_store_get_model("square");
        sg_bindings bindings = {
            .vertex_buffers[0] = model->positions_buf,
            .vertex_buffers[1] = model->texture_coords_buf,
            .fs_images[SLOT_aim_helper_image] = asset_store_get_texture("arrow.png")->image,
        };
        sg_apply_bindings(&bindings);

        float total_length = 0.0f;
        for (int i = 0; i < game->aim.num_line_points - 1; i++) {
            vec3 p0 = game->aim.line_points[i];
            vec3 p1 = game->aim.line_points[i + 1];
            total_length += vec3_distance(p1, p0);
        }

        vec2 offset = game->aim.line_offset; 
        float l0 = 0.0f, l1 = 0.0f;
        for (int i = 0; i < game->aim.num_line_points - 1; i++) {
            vec3 p0 = game->aim.line_points[i];
            vec3 p1 = game->aim.line_points[i + 1];
            vec3 d = vec3_sub(p1, p0);
            float l = vec3_length(d);
            d = vec3_scale(d, 1.0f/l);
            vec2 d2 = vec2_normalize(V2(d.x, d.z));
            l0 = l1;
            l1 += l;

            float z_rotation = asinf(d.y);
            float y_rotation = acosf(d2.x);
            if (d2.y > 0.0f) y_rotation *= -1.0f;
            mat4 model_mat = mat4_multiply_n(6,
                    mat4_translation(p0),
                    mat4_rotation_y(y_rotation),
                    mat4_rotation_z(z_rotation),
                    mat4_scale(V3(0.5f*l, 1.0f, 0.1f)),
                    mat4_translation(V3(1.0f, 0.0f, 0.0f)),
                    mat4_rotation_x(0.5f*MF_PI)
                    );

            aim_helper_vs_params_t vs_params = {
                .mvp_mat = mat4_transpose(mat4_multiply(renderer->proj_view_mat, model_mat)),
            };
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_aim_helper_vs_params, &vs_params, sizeof(vs_params));
            aim_helper_fs_params_t fs_params = {
                .color = V4(1.0f, 1.0f, 1.0f, 1.0f),
                .texture_coord_offset = offset,
                .texture_coord_scale = V2(4.0f*l, 1.0f),
                .length0 = l0,
                .length1 = l1,
                .total_length = total_length,
            };
            sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_aim_helper_fs_params, &fs_params, sizeof(fs_params));
            sg_draw(0, model->num_points, 1);

            offset.x += 4.0f*l;
        }

        sg_end_pass();
    }

    {
        sg_pass_action hole_action0 = {
            .colors[0] = { .action = SG_ACTION_DONTCARE },
            .depth = {
                .action = SG_ACTION_DONTCARE,
            },
            .stencil = {
                .action = SG_ACTION_CLEAR,
            },
        };
        sg_begin_pass(renderer->sokol.game_pass, &hole_action0);
        {
            struct cup_entity *hole = &game->hole.cup_entity;
            struct model *model = asset_store_get_model("hole-cover");
            mat4 transform = cup_entity_get_transform(hole);

            sg_apply_pipeline(renderer->sokol.cup_pipeline[0]);

            sg_bindings bindings = {
                .vertex_buffers[0] = model->positions_buf,
            };
            sg_apply_bindings(&bindings);

            pass_through_vs_params_t vs_params = {
                .mvp_mat = mat4_transpose(mat4_multiply_n(2, renderer->proj_view_mat, transform)),
            };
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_pass_through_vs_params, &vs_params, sizeof(vs_params));

            sg_draw(0, model->num_points, 1);
        }
        sg_end_pass();
    }

    {
        sg_pass_action action = {
            .colors[0] = { .action = SG_ACTION_DONTCARE },
            .depth = {
                .action = SG_ACTION_DONTCARE,
            },
            .stencil = {
                .action = SG_ACTION_DONTCARE,
            },
        };
        sg_begin_pass(renderer->sokol.game_pass, &action);
        sg_apply_pipeline(renderer->sokol.terrain_pipeline[1]);
        for (int i = 0; i < game->hole.terrain_entities.length; i++) {
            struct terrain_entity *entity = &game->hole.terrain_entities.data[i];
            struct terrain_model *model = &entity->terrain_model;
            struct lightmap *lightmap = &entity->lightmap;

            sg_bindings bindings = {
                .vertex_buffers[0] = model->positions_buf,
                .vertex_buffers[1] = model->normals_buf,
                .vertex_buffers[2] = model->texture_coords_buf,
                .vertex_buffers[3] = lightmap->uvs_buf,
                .vertex_buffers[4] = model->material_idxs_buf,
                .fs_images[SLOT_lightmap_tex0] = lightmap->images.data[0].sg_image,
                .fs_images[SLOT_lightmap_tex1] = lightmap->images.data[0].sg_image,
                .fs_images[SLOT_mat_tex0] = asset_store_get_texture("ground.png")->image,
                .fs_images[SLOT_mat_tex1] = asset_store_get_texture("wood.jpg")->image,
            };
            sg_apply_bindings(&bindings);

            vec3 c0[MAX_NUM_TERRAIN_MODEL_MATERIALS];
            vec3 c1[MAX_NUM_TERRAIN_MODEL_MATERIALS];
            for (int i = 0; i < MAX_NUM_TERRAIN_MODEL_MATERIALS; i++) {
                c0[i] = model->materials[i].color0;
                c1[i] = model->materials[i].color1;
            }
            terrain_vs_params_t vs_params = {
                .model_mat = mat4_transpose(terrain_entity_get_transform(entity)),
                .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                .color0[0] = V4(c0[0].x, c0[0].y, c0[0].z, 1.0f),
                .color0[1] = V4(c0[1].x, c0[1].y, c0[1].z, 1.0f),
                .color0[2] = V4(c0[2].x, c0[2].y, c0[2].z, 1.0f),
                .color0[3] = V4(c0[3].x, c0[3].y, c0[3].z, 1.0f),
                .color0[4] = V4(c0[4].x, c0[4].y, c0[4].z, 1.0f),
                .color1[0] = V4(c1[0].x, c1[0].y, c1[0].z, 1.0f),
                .color1[1] = V4(c1[1].x, c1[1].y, c1[1].z, 1.0f),
                .color1[2] = V4(c1[2].x, c1[2].y, c1[2].z, 1.0f),
                .color1[3] = V4(c1[3].x, c1[3].y, c1[3].z, 1.0f),
                .color1[4] = V4(c1[4].x, c1[4].y, c1[4].z, 1.0f),
            };
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_terrain_vs_params, &vs_params, sizeof(vs_params));

            vec3 ball_pos = game->player_ball.draw_position;
            terrain_fs_params_t fs_params = {
                .ball_position = V4(ball_pos.x, ball_pos.y, ball_pos.z, 0.0),
                .lightmap_t = 0.0f,
                .opacity = 0.6f,
            };
            sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_terrain_fs_params, &fs_params, sizeof(fs_params));

            sg_draw(0, model->num_elements, 1);
        }
        sg_end_pass();
    }

    {
        sg_pass_action hole_action1 = {
            .colors[0] = { .action = SG_ACTION_DONTCARE },
            .depth = {
                .action = SG_ACTION_CLEAR,
                .val = 1.0f,
            },
            .stencil = {
                .action = SG_ACTION_DONTCARE,
            },
        };
        sg_begin_pass(renderer->sokol.game_pass, &hole_action1);
        {
            struct cup_entity *hole = &game->hole.cup_entity;
            struct model *model = asset_store_get_model("hole");
            struct lightmap *lightmap = &hole->lightmap;
            mat4 transform = cup_entity_get_transform(hole);

            sg_apply_pipeline(renderer->sokol.cup_pipeline[1]);

            sg_bindings bindings = {
                .vertex_buffers[0] = model->positions_buf,
                .vertex_buffers[1] = lightmap->uvs_buf,
                .fs_images[SLOT_lightmap_tex] = lightmap->images.data[0].sg_image,
            };
            sg_apply_bindings(&bindings);

            cup_vs_params_t vs_params = {
                .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                .model_mat = mat4_transpose(transform),
            };
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_cup_vs_params, &vs_params, sizeof(vs_params));

            sg_draw(0, model->num_points, 1);
        }
        {
            struct ball_entity *ball = &game->player_ball;
            mat4 transform = mat4_multiply_n(3,
                    mat4_translation(ball->draw_position),
                    mat4_scale(V3(ball->radius, ball->radius, ball->radius)),
                    mat4_from_quat(ball->orientation));
            vec3 color = ball->color;
            struct model *model = asset_store_get_model("golf_ball");
            struct texture *texture = asset_store_get_texture("golf_ball_normal_map_128x128.jpg");

            sg_apply_pipeline(renderer->sokol.ball_pipeline[1]);

            sg_bindings bindings = {
                .vertex_buffers[0] = model->positions_buf,
                .vertex_buffers[1] = model->normals_buf,
                .vertex_buffers[2] = model->texture_coords_buf,
                .fs_images[SLOT_normal_map] = texture->image,
            };
            sg_apply_bindings(&bindings);

            ball_vs_params_t vs_params = {
                .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                .model_mat = mat4_transpose(transform),
            };
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_ball_vs_params, &vs_params, sizeof(vs_params));

            ball_fs_params_t fs_params = {
                .color = V4(color.x, color.y, color.z, 1.0f),
            };
            sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ball_fs_params, &fs_params, sizeof(fs_params));

            sg_draw(0, model->num_points, 1);
        }
        sg_end_pass();
    }

    /*
    {
        struct model *model = asset_store_get_model("square");
        sg_pass_action fxaa_pass_action = {
            .colors[0] = { .action = SG_ACTION_CLEAR },
        };
        sg_begin_default_pass(&fxaa_pass_action, renderer->window_width, renderer->window_height);
        sg_apply_pipeline(renderer->sokol.fxaa_pipeline);
        sg_bindings bindings = {
            .vertex_buffers[0] = model->positions_buf,
            .vertex_buffers[1] = model->texture_coords_buf,
            .fs_images[SLOT_fxaa_tex] = renderer->sokol.game_color_image,
        };
        sg_apply_bindings(&bindings);
        fxaa_vs_params_t vs_params = {
            .mvp_mat = mat4_transpose(mat4_multiply_n(4,
                        renderer->ui_proj_mat,
                        mat4_scale(V3((float) renderer->game_fb_width, (float) renderer->game_fb_height, 1.0f)),
                        mat4_scale(V3(0.5f, 0.5f, 1.0f)),
                        mat4_translation(V3(1.0f, 1.0f, 0.0f))
                        )),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_fxaa_vs_params, &vs_params, sizeof(vs_params));
        sg_draw(0, model->num_points, 1);
        sg_end_pass();
    }
    */

    {
        struct model *model = asset_store_get_model("square");
        sg_pass_action game_pass_action = {
            .colors[0] = { .
                action = SG_ACTION_DONTCARE ,
            },
        };
        sg_begin_default_pass(&game_pass_action, renderer->window_width, renderer->window_height);
        sg_apply_pipeline(renderer->sokol.texture_pipeline);
        sg_bindings bindings = {
            .vertex_buffers[0] = model->positions_buf,
            .vertex_buffers[1] = model->texture_coords_buf,
            .fs_images[SLOT_texture_image] = renderer->sokol.game_color_image,
        };
        sg_apply_bindings(&bindings);
        texture_vs_params_t vs_params = {
            .mvp_mat = mat4_transpose(mat4_multiply_n(4,
                        renderer->ui_proj_mat,
                        mat4_scale(V3((float) renderer->game_fb_width, (float) renderer->game_fb_height, 1.0f)),
                        mat4_scale(V3(0.5f, 0.5f, 1.0f)),
                        mat4_translation(V3(1.0f, 1.0f, 0.0f))
                        )),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_texture_vs_params, &vs_params, sizeof(vs_params));
        sg_draw(0, model->num_points, 1);
        sg_end_pass();
    }

    {
        struct model *square_model = asset_store_get_model("square");
        sg_pass_action ui_pass_action = {
            .colors[0] = {
                .action = SG_ACTION_DONTCARE 
            },
            .depth = {
                .action = SG_ACTION_CLEAR,
            },
        };
        sg_begin_default_pass(&ui_pass_action, renderer->window_width, renderer->window_height);

        if (game->state == GAME_STATE_WAITING_FOR_AIM || game->state == GAME_STATE_BEGINNING_AIM) {
            static float theta0 = 0.0f;
            theta0 += 0.5f*1.0f/60.0f;
            if (theta0 > 2.0f*MF_PI) theta0 = 0.0f;

            sg_apply_pipeline(renderer->sokol.ui_single_color_pipeline);
            sg_bindings bindings = {
                .vertex_buffers[0] = square_model->positions_buf,
            };
            sg_apply_bindings(&bindings);

            float radius = game->aim.circle_radius;
            float opacity = 1.0f;
            if (game->state == GAME_STATE_BEGINNING_AIM) {
                float t = game->aim.length/game->aim.min_power_length;
                radius = 25.0f*t + (1.0f - t)*game->aim.circle_radius;
                opacity = 1.0f - t;
            }

            for (int i = 0; i < 30; i++) {
                float theta = theta0 + 2.0f*MF_PI*i/29.0f;
                vec2 pos = game->aim.circle_pos;
                pos.x += radius*cosf(theta);
                pos.y += radius*sinf(theta);

                ui_single_color_vs_params_t vs_params = {
                    .mvp_mat = mat4_transpose(mat4_multiply_n(3, 
                                renderer->ui_proj_mat,
                                mat4_translation(V3(pos.x, pos.y, 0.0f)),
                                mat4_scale(V3(2.0f, 2.0f, 1.0f)))),
                };
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_ui_single_color_vs_params, 
                        &vs_params, sizeof(vs_params));
                ui_single_color_fs_params_t fs_params = {
                    .color = V4(1.0f, 1.0f, 1.0f, opacity),
                };
                sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ui_single_color_fs_params,
                        &fs_params, sizeof(fs_params));
                sg_draw(0, square_model->num_points, 1);
            }
        }

        if (game->state == GAME_STATE_BEGINNING_AIM || game->state == GAME_STATE_AIMING) {
            vec2 position = game->aim.circle_pos;
            float aim_angle = acosf(game->aim.delta.x);
            if (game->aim.delta.y < 0.0f) {
                aim_angle *= -1.0f;
            }
            aim_angle -= 0.5f * MF_PI;
            float scale = game->aim.length/game->aim.max_power_length;
            vec4 wanted_color = V4(1.0f, 1.0f, 1.0f, 1.0f);
            if (game->aim.power < game->aim.green_power) {
                wanted_color = game->aim.green_color;
            }
            else if (game->aim.power < game->aim.yellow_power) {
                wanted_color = game->aim.yellow_color;
            }
            else if (game->aim.power < game->aim.red_power) {
                wanted_color = game->aim.red_color;
            }
            else {
                wanted_color = game->aim.dark_red_color;
            }

            vec4 color = renderer->sokol.game_aim.color;
            color = vec4_add(color, vec4_scale(vec4_sub(wanted_color, color), 0.05f));
            color.w = game->aim.length/game->aim.min_power_length;
            if (color.w > 1.0f) color.w = 1.0f;
            renderer->sokol.game_aim.color = color;

            sg_apply_pipeline(renderer->sokol.aim_icon_pipeline);
            sg_bindings bindings = {
                .vertex_buffers[0] = renderer->sokol.game_aim.positions_buf,
                .vertex_buffers[1] = renderer->sokol.game_aim.alpha_buf,
            };
            sg_apply_bindings(&bindings);
            aim_icon_vs_params_t vs_params = {
                .mvp_mat = mat4_transpose(mat4_multiply_n(4, 
                            renderer->ui_proj_mat,
                            mat4_translation(V3(position.x, position.y, 0.0f)),
                            mat4_rotation_z(aim_angle),
                            mat4_scale(V3(1.0f, scale, 1.0f)))),
            };
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_aim_icon_vs_params, &vs_params, sizeof(vs_params));
            aim_icon_fs_params_t fs_params = {
                .color = color,
            };
            sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_aim_icon_fs_params, &fs_params, sizeof(fs_params));
            sg_draw(0, renderer->sokol.game_aim.num_points, 1);
        }

        if (game->state == GAME_STATE_BEGIN_HOLE) {
            float t = game->begin_hole.t;
            float t_length = config_get_float("begin_hole_t_length");
            float a = t/t_length;

            sg_apply_pipeline(renderer->sokol.ui_pipeline);

            if (a < 0.5f) {
                sg_bindings bindings = {
                    .vertex_buffers[0] = square_model->positions_buf,
                    .vertex_buffers[1] = square_model->texture_coords_buf,
                    .fs_images[SLOT_ui_texture] = asset_store_get_texture("UIpackSheet_transparent.png")->image,
                };
                sg_apply_bindings(&bindings);

                vec2 background_pos = config_get_vec2("begin_hole_background_pos");
                vec2 background_size = config_get_vec2("begin_hole_background_size");
                renderer_draw_ui_box(renderer, background_pos, background_size,
                        V2(13.0f, 20.0f), V2(14.0f, 20.0f), V2(15.0f, 20.0f), 
                        V2(13.0f, 21.0f), V2(14.0f, 21.0f), V2(15.0f, 21.0f),
                        V2(13.0f, 22.0f), V2(14.0f, 22.0f), V2(15.0f, 22.0f), 1.0f);
            }

            if (a < 0.5f) {
                sg_bindings bindings = {
                    .vertex_buffers[0] = square_model->positions_buf,
                    .vertex_buffers[1] = square_model->texture_coords_buf,
                    .fs_images[SLOT_ui_texture] = asset_store_get_texture("font_large.png")->image,
                };
                sg_apply_bindings(&bindings);

                vec4 text_color = config_get_vec4("begin_hole_text_color");

                char hole_num_text[256];
                snprintf(hole_num_text, 256, "HOLE %d", game->cur_hole + 1);
                hole_num_text[255] = 0;

                vec2 hole_num_text_pos = config_get_vec2("begin_hole_num_text_pos");
                float hole_num_text_size = config_get_float("begin_hole_num_text_size");
                renderer_draw_text(renderer, &renderer->large_font, 
                        hole_num_text_pos, hole_num_text_size, text_color, hole_num_text, true);

                char hole_par_text[256];
                int hole_par = game->ui.scoreboard.hole_par[game->cur_hole];
                snprintf(hole_par_text, 256, "PAR %d", hole_par);
                hole_par_text[255] = 0;

                vec2 hole_par_text_pos = config_get_vec2("begin_hole_par_text_pos");
                float hole_par_text_size = config_get_float("begin_hole_par_text_size");
                renderer_draw_text(renderer, &renderer->large_font, 
                        hole_par_text_pos, hole_par_text_size, text_color, hole_par_text, true);
            }
        }

        if (game->state == GAME_STATE_WAITING_FOR_AIM ||
                game->state == GAME_STATE_BEGINNING_AIM ||
                game->state == GAME_STATE_AIMING ||
                game->state == GAME_STATE_SIMULATING_BALL) {
            sg_apply_pipeline(renderer->sokol.ui_pipeline);

            sg_bindings bindings = {
                .vertex_buffers[0] = square_model->positions_buf,
                .vertex_buffers[1] = square_model->texture_coords_buf,
                .fs_images[SLOT_ui_texture] = asset_store_get_texture("font_large.png")->image,
            };
            sg_apply_bindings(&bindings);

            char stroke_text[256];
            snprintf(stroke_text, 256, "STROKE: %d", game->player_ball.stroke_num);
            stroke_text[255] = 0;

            vec2 stroke_text_pos = config_get_vec2("stroke_text_pos");
            float stroke_text_size = config_get_float("stroke_text_size");
            vec4 color = V4(1.0f, 1.0f, 1.0f, 1.0f);
            vec2 stroke_text_shadow_pos = config_get_vec2("stroke_text_shadow_pos");
            vec4 shadow_color = V4(0.0f, 0.0f, 0.0f, 1.0f);
            renderer_draw_text(renderer, &renderer->large_font, 
                    stroke_text_shadow_pos, stroke_text_size, shadow_color, stroke_text, false);
            renderer_draw_text(renderer, &renderer->large_font, 
                    stroke_text_pos, stroke_text_size, color, stroke_text, false);
        }

        {
            sg_apply_pipeline(renderer->sokol.ui_pipeline);

            if (game->state == GAME_STATE_HOLE_COMPLETE) {
                {
                    sg_bindings bindings = {
                        .vertex_buffers[0] = square_model->positions_buf,
                        .vertex_buffers[1] = square_model->texture_coords_buf,
                        .fs_images[SLOT_ui_texture] =
                            asset_store_get_texture("UIpackSheet_transparent.png")->image,
                    };
                    sg_apply_bindings(&bindings);
                    renderer_draw_button(renderer, &game->ui.next_hole_button);
                }

                {
                    sg_bindings bindings = {
                        .vertex_buffers[0] = square_model->positions_buf,
                        .vertex_buffers[1] = square_model->texture_coords_buf,
                        .fs_images[SLOT_ui_texture] = asset_store_get_texture("font_medium.png")->image,
                    };
                    sg_apply_bindings(&bindings);

                    vec2 pos;
                    pos = game->ui.next_hole_button.pos;
                    pos.y += 25.0f;
                    if (game->ui.next_hole_button.is_hovered) {
                        pos.y -= 5.0f;
                    }
                    vec4 color = V4(0.0f, 0.0f, 0.0f, 1.0f);
                    float size = 0.8f;

                    const char *text = NULL;
                    if (game->cur_hole + 1 < config_get_int("game_num_holes")) {
                        text = "NEXT HOLE";
                    }
                    else {
                        text = "FINISH";
                    }
                    renderer_draw_text(renderer, &renderer->medium_font, pos, size, color, text, true);
                }
            }

            if (game->ui.is_scoreboard_open) {
                struct scoreboard *scoreboard = &game->ui.scoreboard;

                {
                    sg_bindings bindings = {
                        .vertex_buffers[0] = square_model->positions_buf,
                        .vertex_buffers[1] = square_model->texture_coords_buf,
                        .fs_images[SLOT_ui_texture] = asset_store_get_texture("UIpackSheet_transparent.png")->image,
                    };
                    sg_apply_bindings(&bindings);

                    vec2 scoreboard_pos = config_get_vec2("scoreboard_pos");
                    vec2 scoreboard_size = config_get_vec2("scoreboard_size");
                    renderer_draw_ui_box(renderer, scoreboard_pos, scoreboard_size,
                            V2(13.0f, 20.0f), V2(14.0f, 20.0f), V2(15.0f, 20.0f), 
                            V2(13.0f, 21.0f), V2(14.0f, 21.0f), V2(15.0f, 21.0f),
                            V2(13.0f, 22.0f), V2(14.0f, 22.0f), V2(15.0f, 22.0f), 1.0f);
                }

                {
                    sg_bindings bindings = {
                        .vertex_buffers[0] = square_model->positions_buf,
                        .vertex_buffers[1] = square_model->texture_coords_buf,
                        .fs_images[SLOT_ui_texture] = asset_store_get_texture("font_medium.png")->image,
                    };
                    sg_apply_bindings(&bindings);

                    vec2 scoreboard_text_pos = config_get_vec2("scoreboard_text_pos");
                    float scoreboard_text_size = config_get_float("scoreboard_text_size");
                    vec4 color = V4(0.0f, 0.0f, 0.0f, 1.0f);
                    renderer_draw_text(renderer, &renderer->medium_font, 
                            scoreboard_text_pos, scoreboard_text_size, color, "SCORECARD", true);

                    vec2 hole_text_pos = config_get_vec2("scoreboard_hole_text_pos");
                    float hole_text_size = config_get_float("scoreboard_hole_text_size");
                    renderer_draw_text(renderer, &renderer->medium_font, 
                            hole_text_pos, hole_text_size, color, "HOLE", true);

                    vec2 par_text_pos = config_get_vec2("scoreboard_par_text_pos");
                    float par_text_size = config_get_float("scoreboard_par_text_size");
                    renderer_draw_text(renderer, &renderer->medium_font, 
                            par_text_pos, par_text_size, color, "PAR", true);

                    vec2 total_text_pos = config_get_vec2("scoreboard_total_text_pos");
                    float total_text_size = config_get_float("scoreboard_total_text_size");
                    renderer_draw_text(renderer, &renderer->medium_font, 
                            total_text_pos, total_text_size, color, "TOTAL", true);

                    vec2 player_text_pos = config_get_vec2("scoreboard_player_text_pos");
                    float player_text_size = config_get_float("scoreboard_player_text_size");
                    renderer_draw_text(renderer, &renderer->medium_font, 
                            player_text_pos, player_text_size, color, "SCORE", true);

                    vec2 value_text_pos;
                    value_text_pos.x = config_get_float("scoreboard_value_text_start_pos_x");
                    float value_text_size = config_get_float("scoreboard_value_text_size");
                    float value_text_delta = config_get_float("scoreboard_value_text_delta");
                    int total_par = 0;
                    int total_score = 0;
                    int num_holes = config_get_int("game_num_holes");
                    for (int i = 0; i < num_holes; i++) {
                        char str[256];

                        color = V4(0.0f, 0.0f, 0.0f, 1.0f);
                        snprintf(str, 256, "%d", i + 1);
                        str[255] = 0;
                        value_text_pos.y = hole_text_pos.y;
                        renderer_draw_text(renderer, &renderer->medium_font, value_text_pos, value_text_size,
                                color, str, true);

                        snprintf(str, 256, "%d", scoreboard->hole_par[i]);
                        str[255] = 0;
                        value_text_pos.y = par_text_pos.y;
                        renderer_draw_text(renderer, &renderer->medium_font, value_text_pos, value_text_size,
                                color, str, true);

                        color = V4(0.7f, 0.2f, 0.3f, 1.0f);
                        if (scoreboard->hole_score[i] == -1) {
                            str[0] = '-';
                            str[1] = 0;
                        }
                        else {
                            snprintf(str, 256, "%d", scoreboard->hole_score[i]);
                            str[255] = 0;
                        }
                        value_text_pos.y = player_text_pos.y;
                        renderer_draw_text(renderer, &renderer->medium_font, value_text_pos, value_text_size,
                                color, str, true);

                        value_text_pos.x += value_text_delta;

                        if (scoreboard->hole_score[i] > 0) {
                            total_score += scoreboard->hole_score[i];
                        }
                        if (scoreboard->hole_par[i] > 0) {
                            total_par += scoreboard->hole_par[i];
                        }
                    }

                    {
                        char str[256];

                        vec2 total_par_pos = V2(total_text_pos.x, par_text_pos.y);
                        color = V4(0.0f, 0.0f, 0.0f, 1.0f);
                        snprintf(str, 256, "%d", total_par);
                        str[255] = 0;
                        renderer_draw_text(renderer, &renderer->medium_font, total_par_pos, value_text_size,
                                color, str, true);

                        vec2 total_score_pos = V2(total_text_pos.x, player_text_pos.y);
                        color = V4(0.7f, 0.2f, 0.3f, 1.0f);
                        snprintf(str, 256, "%d", total_score);
                        str[255] = 0;
                        renderer_draw_text(renderer, &renderer->medium_font, total_score_pos, value_text_size,
                                color, str, true);
                    }
                }
            }

            // Tutorial
            if (game->cur_hole == 0 && game->player_ball.stroke_num == 1) {
                sg_bindings bindings = {
                    .vertex_buffers[0] = square_model->positions_buf,
                    .vertex_buffers[1] = square_model->texture_coords_buf,
                    .fs_images[SLOT_ui_texture] = asset_store_get_texture("font_medium.png")->image,
                };
                sg_apply_bindings(&bindings);

                vec2 tutorial_text_pos = config_get_vec2("tutorial_text_pos");
                float tutorial_text_size = config_get_float("tutorial_text_size");
                vec2 tutorial_text_shadow_pos = vec2_add(tutorial_text_pos, V2(2, -2));
                vec4 shadow_color = V4(0.0f, 0.0f, 0.0f, 1.0f);
                vec4 tutorial_text_color = V4(0.7f, 0.3f, 0.2f, 1.0f);

                if (game->state == GAME_STATE_WAITING_FOR_AIM) {
                    const char *text = "To begin aiming click in the middle of the dotted circle.";
                    renderer_draw_text(renderer, &renderer->medium_font, 
                            tutorial_text_shadow_pos, tutorial_text_size, shadow_color, 
                            text, true);
                    renderer_draw_text(renderer, &renderer->medium_font, 
                            tutorial_text_pos, tutorial_text_size, tutorial_text_color, 
                            text, true);
                }
                else if (game->state == GAME_STATE_BEGINNING_AIM ||
                        game->state == GAME_STATE_AIMING) {
                    const char *text = "Now move the mouse down to aim. Click again to hit the ball.";
                    renderer_draw_text(renderer, &renderer->medium_font, 
                            tutorial_text_shadow_pos, tutorial_text_size, shadow_color, 
                            text, true);
                    renderer_draw_text(renderer, &renderer->medium_font, 
                            tutorial_text_pos, tutorial_text_size, tutorial_text_color, 
                            text, true);
                }
            }
        }

        sg_end_pass();
    }

    if (game->drawing.is_blink) {
        struct model *square_model = asset_store_get_model("square");
        float opacity = 1.0f;
        if (game->drawing.blink_t > 0.25f*game->drawing.blink_t_length) {
            float a = (game->drawing.blink_t - 0.25f*game->drawing.blink_t_length);
            a /= (0.75f*game->drawing.blink_t_length);
            a = 1.0f - a;
            opacity = a*a;
        }

        sg_pass_action action = {
            .colors[0] = {
                .action = SG_ACTION_DONTCARE,
            },
            .depth = {
                .action = SG_ACTION_CLEAR,
            },
        };
        sg_begin_default_pass(&action, renderer->window_width, renderer->window_height);
        sg_apply_pipeline(renderer->sokol.ui_single_color_pipeline);
        sg_bindings bindings = {
            .vertex_buffers[0] = square_model->positions_buf,
        };
        sg_apply_bindings(&bindings);
        ui_single_color_vs_params_t vs_params = {
            .mvp_mat = mat4_transpose(mat4_multiply_n(3, 
                        renderer->ui_proj_mat,
                        mat4_translation(V3(640.0f, 360.0f, 0.0f)),
                        mat4_scale(V3(1280.0f, 720.0f, 1.0f)))),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_ui_single_color_vs_params, 
                &vs_params, sizeof(vs_params));
        ui_single_color_fs_params_t fs_params = {
            .color = V4(1.0f, 1.0f, 1.0f, opacity),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ui_single_color_fs_params,
                &fs_params, sizeof(fs_params));
        sg_draw(0, square_model->num_points, 1);
        sg_end_pass();
    }

    profiler_pop_section();
}

//
// Needs the hole_editor_terrain_pipeline applied
// 
static void renderer_hole_editor_draw_terrain_model(struct renderer *renderer,
        struct terrain_model *model, mat4 model_mat, struct lightmap *lightmap,
        int lightmap_i0, int lightmap_i1, float lightmap_t, int draw_type) {
    sg_bindings bindings = {
        .vertex_buffers[0] = model->positions_buf,
        .vertex_buffers[1] = model->normals_buf,
        .vertex_buffers[2] = model->texture_coords_buf,
        .vertex_buffers[3] = lightmap->uvs_buf,
        .vertex_buffers[4] = model->material_idxs_buf,
        .fs_images[SLOT_ce_lightmap_tex0] = lightmap->images.data[lightmap_i0].sg_image,
        .fs_images[SLOT_ce_lightmap_tex1] = lightmap->images.data[lightmap_i1].sg_image,
        .fs_images[SLOT_ce_tex0] = 
            asset_store_get_texture("ground.png")->image,
        .fs_images[SLOT_ce_tex1] = 
            asset_store_get_texture("wood.jpg")->image,
    };
    sg_apply_bindings(&bindings);

    vec3 c0[MAX_NUM_TERRAIN_MODEL_MATERIALS];
    vec3 c1[MAX_NUM_TERRAIN_MODEL_MATERIALS];
    for (int i = 0; i < MAX_NUM_TERRAIN_MODEL_MATERIALS; i++) {
        c0[i] = model->materials[i].color0;
        c1[i] = model->materials[i].color1;
    }

    hole_editor_terrain_vs_params_t vs_params = {
        .model_mat = mat4_transpose(model_mat),
        .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
        .color0[0] = V4(c0[0].x, c0[0].y, c0[0].z, 1.0f),
        .color0[1] = V4(c0[1].x, c0[1].y, c0[1].z, 1.0f),
        .color0[2] = V4(c0[2].x, c0[2].y, c0[2].z, 1.0f),
        .color0[3] = V4(c0[3].x, c0[3].y, c0[3].z, 1.0f),
        .color0[4] = V4(c0[4].x, c0[4].y, c0[4].z, 1.0f),
        .color1[0] = V4(c1[0].x, c1[0].y, c1[0].z, 1.0f),
        .color1[1] = V4(c1[1].x, c1[1].y, c1[1].z, 1.0f),
        .color1[2] = V4(c1[2].x, c1[2].y, c1[2].z, 1.0f),
        .color1[3] = V4(c1[3].x, c1[3].y, c1[3].z, 1.0f),
        .color1[4] = V4(c1[4].x, c1[4].y, c1[4].z, 1.0f),
    };
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_hole_editor_terrain_vs_params, 
            &vs_params, sizeof(vs_params));

    hole_editor_terrain_fs_params_t fs_params = {
        .draw_type = (float) draw_type,
        .lightmap_t = lightmap_t,
    };
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_hole_editor_terrain_fs_params, 
            &fs_params, sizeof(fs_params));


    // 0-3 draws Default, No AO, Only AO, Lightmap UVS
    if (draw_type < 4) {
        sg_draw(0, model->num_elements, 1);
    }
    // 4 - 6 draws COR, Friction, Vel Scale
    else if (draw_type < 7) {
        // We don't send COR, Friction, and Vel Scale values to the shader so need to draw each face seperately
        int start_element = 0;
        for (int i = 0; i < model->faces.length; i++) {
            struct terrain_model_face face = model->faces.data[i];
            float c = 0.0f;
            if (draw_type == 4) {
                c = face.cor;
            }
            else if (draw_type == 5) {
                c = face.friction;
            }
            else if (draw_type == 6) {
                c = face.vel_scale;
            }

            hole_editor_terrain_fs_params_t fs_params = {
                .draw_type = (float) draw_type,
                .uniform_color = V4(c, c, c, 1.0f),
                .lightmap_t = 0.0f,
            };
            sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_hole_editor_terrain_fs_params, 
                    &fs_params, sizeof(fs_params));

            if (face.num_points == 3) {
                sg_draw(start_element, 3, 1);
                start_element += 3;
            }
            else if (face.num_points == 4) {
                sg_draw(start_element, 6, 1);
                start_element += 6;
            }
            else {
                assert(false);
            }
        }
    }
}

void renderer_draw_hole_editor(struct renderer *renderer, struct game *game, struct hole_editor *ce) {
    {
        sg_pass_action pass_action = {
            .colors[0] = { .action = SG_ACTION_CLEAR },
        };
        sg_begin_pass(renderer->sokol.game_pass, &pass_action);

        {
            profiler_push_section("draw_environments");
            sg_apply_pipeline(renderer->sokol.hole_editor_environment_pipeline);
            struct lightmap *lightmap = &ce->hole->environment_lightmap;
            int lightmap_uvs_offset = 0;
            for (int i = 0; i < ce->hole->environment_entities.length; i++) {
                struct environment_entity *entity = &ce->hole->environment_entities.data[i];
                struct model *model = entity->model;

                sg_bindings bindings = {
                    .vertex_buffers[0] = model->positions_buf,
                    .vertex_buffers[1] = model->normals_buf,
                    .vertex_buffers[2] = model->texture_coords_buf,
                    .vertex_buffers[3] = lightmap->uvs_buf,
                    .vertex_buffer_offsets[3] = lightmap_uvs_offset,
                    .fs_images[SLOT_ce_lightmap_tex] = lightmap->images.data[0].sg_image,
                    .fs_images[SLOT_ce_material_tex] = asset_store_get_texture("environment_material.bmp")->image,
                };
                sg_apply_bindings(&bindings);

                hole_editor_environment_vs_params_t vs_params = {
                    .model_mat = mat4_transpose(environment_entity_get_transform(entity)),
                    .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                };
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_hole_editor_environment_vs_params, 
                        &vs_params, sizeof(vs_params));

                hole_editor_environment_fs_params_t fs_params = {
                    .draw_type = (float) ce->drawing.terrain_entities.draw_type,
                };
                sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_hole_editor_environment_fs_params,
                        &fs_params, sizeof(fs_params));

                sg_draw(0, model->num_points, 1);
                lightmap_uvs_offset += sizeof(vec2)*model->num_points;
            }
            profiler_pop_section();
        }

        {
            profiler_push_section("draw_terrain");
            sg_apply_pipeline(renderer->sokol.hole_editor_terrain_pipeline);

            for (int i = 0; i < ce->hole->multi_terrain_entities.length; i++) {
                struct multi_terrain_entity *entity = &ce->hole->multi_terrain_entities.data[i];
                struct terrain_model *static_model = &entity->static_terrain_model;
                struct terrain_model *moving_model = &entity->moving_terrain_model;
                struct lightmap *static_lightmap = &entity->static_lightmap;
                struct lightmap *moving_lightmap = &entity->moving_lightmap;

                float t = fmodf(game->t, entity->movement_data.length)/entity->movement_data.length;
                if (entity->movement_data.type == MOVEMENT_TYPE_PENDULUM ||
                        entity->movement_data.type == MOVEMENT_TYPE_TO_AND_FROM ||
                        entity->movement_data.type == MOVEMENT_TYPE_RAMP) {
                    // For these movement types the second half of the movement is just the first have but in reverse 
                    t = 2.0f*t;
                    if (t > 1.0f) {
                        t = 2.0f - t;
                    }
                }

                // Draw the static part
                {
                    int static_lightmap_i0 = 0, static_lightmap_i1 = 1;
                    for (int i = 1; i < static_lightmap->images.length; i++) {
                        if (t < i / ((float) (static_lightmap->images.length - 1))) {
                            static_lightmap_i0 = i - 1;
                            static_lightmap_i1 = i;
                            break;
                        }
                    }
                    float static_lightmap_t = 0.0f;
                    float static_lightmap_t0 = static_lightmap_i0 / ((float) (static_lightmap->images.length - 1));
                    float static_lightmap_t1 = static_lightmap_i1 / ((float) (static_lightmap->images.length - 1));
                    static_lightmap_t = (t - static_lightmap_t0) / (static_lightmap_t1 - static_lightmap_t0);
                    if (static_lightmap_t < 0.0f) static_lightmap_t = 0.0f;
                    if (static_lightmap_t > 1.0f) static_lightmap_t = 1.0f;
                    mat4 static_model_mat = multi_terrain_entity_get_static_transform(entity);

                    renderer_hole_editor_draw_terrain_model(renderer, static_model, static_model_mat, 
                            static_lightmap, static_lightmap_i0, static_lightmap_i1, static_lightmap_t, 
                            ce->drawing.terrain_entities.draw_type);
                }

                // Draw the moving part
                {
                    int moving_lightmap_i0 = 0, moving_lightmap_i1 = 1;
                    for (int i = 1; i < moving_lightmap->images.length; i++) {
                        if (t < i / ((float) (moving_lightmap->images.length - 1))) {
                            moving_lightmap_i0 = i - 1;
                            moving_lightmap_i1 = i;
                            break;
                        }
                    }
                    float moving_lightmap_t = 0.0f;
                    float moving_lightmap_t0 = moving_lightmap_i0 / ((float) (moving_lightmap->images.length - 1));
                    float moving_lightmap_t1 = moving_lightmap_i1 / ((float) (moving_lightmap->images.length - 1));
                    moving_lightmap_t = (t - moving_lightmap_t0) / (moving_lightmap_t1 - moving_lightmap_t0);
                    if (moving_lightmap_t < 0.0f) moving_lightmap_t = 0.0f;
                    if (moving_lightmap_t > 1.0f) moving_lightmap_t = 1.0f;
                    mat4 moving_model_mat = multi_terrain_entity_get_moving_transform(entity, game->t);

                    renderer_hole_editor_draw_terrain_model(renderer, moving_model, moving_model_mat, 
                            moving_lightmap, moving_lightmap_i0, moving_lightmap_i1, moving_lightmap_t, 
                            ce->drawing.terrain_entities.draw_type);
                }
            }

            for (int i = 0; i < ce->hole->terrain_entities.length; i++) {
                struct terrain_entity *entity = &ce->hole->terrain_entities.data[i];
                struct terrain_model *model = &entity->terrain_model;
                mat4 model_mat = terrain_entity_get_transform(entity);
                struct lightmap *lightmap = &entity->lightmap;
                int lightmap_i0 = 0;
                int lightmap_i1 = 0;
                float lightmap_t = 0.0f;

                renderer_hole_editor_draw_terrain_model(renderer, model, model_mat, 
                        lightmap, lightmap_i0, lightmap_i1, lightmap_t,
                        ce->drawing.terrain_entities.draw_type);
            }
            profiler_pop_section();
        }

        {
            static float t = 0.0f;
            t += 0.001f;

            profiler_push_section("draw_water");
            sg_apply_pipeline(renderer->sokol.hole_editor_water_pipeline);
            for (int i = 0; i < ce->hole->water_entities.length; i++) {
                struct water_entity *entity = &ce->hole->water_entities.data[i];
                struct terrain_model *model = &entity->model;
                struct lightmap *lightmap = &entity->lightmap;

                sg_bindings bindings = {
                    .vertex_buffers[0] = model->positions_buf,
                    .vertex_buffers[1] = model->texture_coords_buf,
                    .vertex_buffers[2] = lightmap->uvs_buf,
                    .fs_images[SLOT_ce_water_lightmap_tex] =
                        lightmap->images.data[0].sg_image,
                    .fs_images[SLOT_ce_water_noise_tex0] =
                        asset_store_get_texture("water_noise_1.png")->image,
                    .fs_images[SLOT_ce_water_noise_tex1] =
                        asset_store_get_texture("water_noise_2.png")->image,
                };
                sg_apply_bindings(&bindings);

                hole_editor_water_vs_params_t vs_params = {
                    .model_mat = mat4_transpose(water_entity_get_transform(entity)),
                    .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                };
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_hole_editor_water_vs_params,
                        &vs_params, sizeof(vs_params));

                hole_editor_water_fs_params_t fs_params = {
                    .draw_type = (float)ce->drawing.terrain_entities.draw_type,
                    .t = t,
                };
                sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_hole_editor_water_fs_params,
                        &fs_params, sizeof(fs_params));

                sg_draw(0, model->num_elements, 1);
            }
            profiler_pop_section();
        }

        if (ce->drawing.helper_lines.active) {
            profiler_push_section("draw_helper_lines");
            float line_dist = ce->drawing.helper_lines.line_dist;
            float sz = 0.01f;
            for (int i = -50; i <= 50; i++) {
                vec3 p0 = V3(i * line_dist, 0.0f, -50.0f);
                vec3 p1 = V3(i * line_dist, 0.0f, 50.0f);
                renderer_draw_line(renderer, p0, p1, sz, V4(1.0f, 1.0f, 1.0f, 1.0f));
            }
            for (int i = -50; i <= 50; i++) {
                vec3 p0 = V3(-50.0f, 0.0f, i * line_dist);
                vec3 p1 = V3(50.0f, 0.0f, i * line_dist);
                renderer_draw_line(renderer, p0, p1, sz, V4(1.0f, 1.0f, 1.0f, 1.0f));
            }
            profiler_pop_section();
        }

        {
            sg_apply_pipeline(renderer->sokol.objects_pipeline);
            profiler_push_section("draw_objects");
            if (ce->edit_terrain_model.active) {
                struct editor_entity_array *hovered_array = &ce->edit_terrain_model.hovered_array;
                struct editor_entity_array *selected_array = &ce->edit_terrain_model.selected_array;
                struct terrain_model *terrain_model = ce->edit_terrain_model.model;
                mat4 terrain_model_mat = ce->edit_terrain_model.model_mat;
                struct model *sphere_model = asset_store_get_model("sphere");
                struct model *cube_model = asset_store_get_model("cube");

                for (int i = 0; i < terrain_model->points.length; i++) {
                    vec3 point = terrain_model->points.data[i];
                    struct editor_entity editor_entity = make_editor_entity(EDITOR_ENTITY_POINT, i);
                    bool is_selected = editor_entity_array_contains_entity(selected_array, editor_entity);
                    bool is_hovered = editor_entity_array_contains_entity(hovered_array, editor_entity);
                    vec4 c = V4(0.5f, 0.5f, 0.5f, 1.0f);
                    if (is_selected && is_hovered) {
                        c = V4(1.0f, 1.0f, 1.0f, 1.0f);
                    }
                    else if (is_selected || is_hovered) {
                        c = V4(0.8f, 0.8f, 0.8f, 1.0f);
                    }

                    mat4 model_mat = mat4_multiply_n(3, 
                            terrain_model_mat, 
                            mat4_translation(point),
                            mat4_scale(V3(0.04f, 0.04f, 0.04f)));

                    sg_bindings bindings = {
                        .vertex_buffers[0] = sphere_model->positions_buf,
                        .vertex_buffers[1] = sphere_model->normals_buf,
                    };
                    sg_apply_bindings(&bindings);

                    single_color_vs_params_t vs_params = {
                        .model_mat = mat4_transpose(model_mat),
                        .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                    };
                    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                            &vs_params, sizeof(vs_params));

                    single_color_fs_params_t fs_params = {
                        .color = c,
                        .kd_scale = 0.0f,
                    };
                    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params,
                            &fs_params, sizeof(fs_params));

                    sg_draw(0, sphere_model->num_points, 1);
                }

                int buf_idx = 0;
                for (int i = 0; i < terrain_model->faces.length; i++) {
                    struct terrain_model_face face = terrain_model->faces.data[i];
                    struct editor_entity editor_entity = make_editor_entity(EDITOR_ENTITY_FACE, i);
                    bool is_selected = editor_entity_array_contains_entity(selected_array, editor_entity);
                    bool is_hovered = editor_entity_array_contains_entity(hovered_array, editor_entity);
                    vec4 c = V4(0.9f, 0.1f, 0.2f, 0.0f);
                    if (is_selected && is_hovered) {
                        c.w = 0.5f;
                    }
                    else if (is_selected || is_hovered) {
                        c.w = 0.35f;
                    }

                    int num_elements = 0;
                    if (face.num_points == 3) {
                        num_elements = 3;
                    }
                    else if (face.num_points == 4) {
                        num_elements = 6;
                    }
                    else {
                        assert(false);
                    }

                    if (c.w > 0.0f) {
                        sg_bindings bindings = {
                            .vertex_buffers[0] = terrain_model->positions_buf,
                            .vertex_buffers[1] = terrain_model->normals_buf,
                        };
                        sg_apply_bindings(&bindings);

                        single_color_vs_params_t vs_params = {
                            .model_mat = mat4_transpose(terrain_model_mat),
                            .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                        };
                        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                                &vs_params, sizeof(vs_params));

                        single_color_fs_params_t fs_params = {
                            .color = c,
                            .kd_scale = 0.0f,
                        };
                        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params,
                                &fs_params, sizeof(fs_params));

                        sg_draw(buf_idx, num_elements, 1);
                    }

                    vec3 points[4], mid_point;
                    if (face.num_points == 3) {
                        points[0] = terrain_model_get_point(terrain_model, face.x); 
                        points[1] = terrain_model_get_point(terrain_model, face.y); 
                        points[2] = terrain_model_get_point(terrain_model, face.z); 
                        points[0] = vec3_apply_mat4(points[0], 1.0f, terrain_model_mat);
                        points[1] = vec3_apply_mat4(points[1], 1.0f, terrain_model_mat);
                        points[2] = vec3_apply_mat4(points[2], 1.0f, terrain_model_mat);
                        mid_point = vec3_scale(vec3_add(points[0], 
                                    vec3_add(points[1], points[2])), 1.0f / 3.0f);
                    }
                    else if (face.num_points == 4) {
                        points[0] = terrain_model_get_point(terrain_model, face.x); 
                        points[1] = terrain_model_get_point(terrain_model, face.y); 
                        points[2] = terrain_model_get_point(terrain_model, face.z); 
                        points[3] = terrain_model_get_point(terrain_model, face.w); 
                        points[0] = vec3_apply_mat4(points[0], 1.0f, terrain_model_mat);
                        points[1] = vec3_apply_mat4(points[1], 1.0f, terrain_model_mat);
                        points[2] = vec3_apply_mat4(points[2], 1.0f, terrain_model_mat);
                        points[3] = vec3_apply_mat4(points[3], 1.0f, terrain_model_mat);
                        mid_point = vec3_scale(vec3_add(
                                    vec3_add(points[0], points[1]), 
                                    vec3_add(points[2], points[3])), 1.0f / 4.0f);
                    }
                    else {
                        assert(false);
                    }
                    float size = 0.001f * vec3_distance(renderer->cam_pos, mid_point);
                    c.w = 1.0f;

                    for (int i = 0; i < face.num_points; i++) {
                        vec3 p0 = points[i];
                        vec3 p1;
                        if (i + 1 < face.num_points) {
                            p1 = points[i + 1];
                        }
                        else {
                            p1 = points[0];
                        }

                        sg_bindings bindings = {
                            .vertex_buffers[0] = cube_model->positions_buf,
                            .vertex_buffers[1] = cube_model->normals_buf,
                        };
                        sg_apply_bindings(&bindings);

                        single_color_vs_params_t vs_params = {
                            .model_mat = mat4_transpose(mat4_box_to_line_transform(p0, p1, size)),
                            .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                        };
                        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                                &vs_params, sizeof(vs_params));

                        single_color_fs_params_t fs_params = {
                            .color = c,
                            .kd_scale = 0.0f,
                        };
                        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params,
                                &fs_params, sizeof(fs_params));

                        sg_draw(0, cube_model->num_points, 1);
                    }

                    buf_idx += num_elements;
                }
            }
            else {
                struct editor_entity_array *hovered_array = &ce->hovered_array;
                struct editor_entity_array *selected_array = &ce->selected_array;
                for (int i = 0; i < selected_array->length; i++) {
                    struct editor_entity selected = selected_array->data[i];
                    bool draw = false;
                    mat4 transform;
                    sg_buffer positions_buffer, normals_buffer;
                    int num_elements;
                    if (selected.type == EDITOR_ENTITY_TERRAIN) {
                        struct terrain_entity *terrain = &ce->hole->terrain_entities.data[selected.idx];
                        struct terrain_model *model = &terrain->terrain_model;
                        draw = true;
                        transform = terrain_entity_get_transform(terrain);
                        positions_buffer = model->positions_buf;
                        normals_buffer = model->normals_buf;
                        num_elements = model->num_elements;
                    }
                    else if (selected.type == EDITOR_ENTITY_MULTI_TERRAIN_MOVING) {
                        struct multi_terrain_entity *multi_terrain =
                            &ce->hole->multi_terrain_entities.data[selected.idx];
                        struct terrain_model *model = &multi_terrain->moving_terrain_model;
                        draw = true;
                        transform = multi_terrain_entity_get_moving_transform(multi_terrain, game->t);
                        positions_buffer = model->positions_buf;
                        normals_buffer = model->normals_buf;
                        num_elements = model->num_elements;
                    }
                    else if (selected.type == EDITOR_ENTITY_MULTI_TERRAIN_STATIC) {
                        struct multi_terrain_entity *multi_terrain =
                            &ce->hole->multi_terrain_entities.data[selected.idx];
                        struct terrain_model *model = &multi_terrain->static_terrain_model;
                        draw = true;
                        transform = multi_terrain_entity_get_static_transform(multi_terrain);
                        positions_buffer = model->positions_buf;
                        normals_buffer = model->normals_buf;
                        num_elements = model->num_elements;
                    }
                    else if (selected.type == EDITOR_ENTITY_ENVIRONMENT) {
                        struct environment_entity *environment = 
                            &ce->hole->environment_entities.data[selected.idx];
                        struct model *model = environment->model;
                        draw = true;
                        transform = environment_entity_get_transform(environment);
                        positions_buffer = model->positions_buf;
                        normals_buffer = model->normals_buf;
                        num_elements = model->num_points;
                    }

                    if (draw) {
                        sg_bindings bindings = {
                            .vertex_buffers[0] = positions_buffer,
                            .vertex_buffers[1] = normals_buffer,
                        };
                        sg_apply_bindings(&bindings);

                        single_color_vs_params_t vs_params = {
                            .model_mat = mat4_transpose(transform),
                            .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                        };
                        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                                &vs_params, sizeof(vs_params));

                        single_color_fs_params_t fs_params = {
                            .color = V4(1.0f, 1.0f, 1.0f, 0.3f),
                            .kd_scale = 0.0f,
                        };
                        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params,
                                &fs_params, sizeof(fs_params));

                        sg_draw(0, num_elements, 1);
                    }
                }
                for (int i = 0; i < hovered_array->length; i++) {
                    struct editor_entity hovered = hovered_array->data[i];
                    bool draw = false;
                    mat4 transform;
                    sg_buffer positions_buffer, normals_buffer;
                    int num_elements;
                    if (hovered.type == EDITOR_ENTITY_TERRAIN) {
                        struct terrain_entity *terrain = &ce->hole->terrain_entities.data[hovered.idx];
                        struct terrain_model *model = &terrain->terrain_model;
                        draw = true;
                        transform = terrain_entity_get_transform(terrain);
                        positions_buffer = model->positions_buf;
                        normals_buffer = model->normals_buf;
                        num_elements = model->num_elements;
                    }
                    else if (hovered.type == EDITOR_ENTITY_MULTI_TERRAIN_MOVING) {
                        struct multi_terrain_entity *multi_terrain =
                            &ce->hole->multi_terrain_entities.data[hovered.idx];
                        struct terrain_model *model = &multi_terrain->moving_terrain_model;
                        draw = true;
                        transform = multi_terrain_entity_get_moving_transform(multi_terrain, game->t);
                        positions_buffer = model->positions_buf;
                        normals_buffer = model->normals_buf;
                        num_elements = model->num_elements;
                    }
                    else if (hovered.type == EDITOR_ENTITY_MULTI_TERRAIN_STATIC) {
                        struct multi_terrain_entity *multi_terrain =
                            &ce->hole->multi_terrain_entities.data[hovered.idx];
                        struct terrain_model *model = &multi_terrain->static_terrain_model;
                        draw = true;
                        transform = multi_terrain_entity_get_static_transform(multi_terrain);
                        positions_buffer = model->positions_buf;
                        normals_buffer = model->normals_buf;
                        num_elements = model->num_elements;
                    }
                    else if (hovered.type == EDITOR_ENTITY_ENVIRONMENT) {
                        struct environment_entity *environment = 
                            &ce->hole->environment_entities.data[hovered.idx];
                        struct model *model = environment->model;
                        draw = true;
                        transform = environment_entity_get_transform(environment);
                        positions_buffer = model->positions_buf;
                        normals_buffer = model->normals_buf;
                        num_elements = model->num_points;
                    }

                    if (draw) {
                        sg_bindings bindings = {
                            .vertex_buffers[0] = positions_buffer,
                            .vertex_buffers[1] = normals_buffer,
                        };
                        sg_apply_bindings(&bindings);

                        single_color_vs_params_t vs_params = {
                            .model_mat = mat4_transpose(transform),
                            .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                        };
                        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                                &vs_params, sizeof(vs_params));

                        single_color_fs_params_t fs_params = {
                            .color = V4(1.0f, 1.0f, 1.0f, 0.3f),
                            .kd_scale = 0.0f,
                        };
                        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params,
                                &fs_params, sizeof(fs_params));

                        sg_draw(0, num_elements, 1);
                    }
                }

                {
                    struct ball_start_entity *ball_start = &ce->hole->ball_start_entity;
                    mat4 model_mat = ball_start_entity_get_transform(ball_start);
                    struct model *model = ball_start->model;

                    sg_bindings bindings = {
                        .vertex_buffers[0] = model->positions_buf,
                        .vertex_buffers[1] = model->normals_buf,
                    };
                    sg_apply_bindings(&bindings);
                    single_color_vs_params_t vs_params = {
                        .model_mat = mat4_transpose(model_mat),
                        .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                    };
                    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                            &vs_params, sizeof(vs_params));
                    single_color_fs_params_t fs_params = {
                        .color = V4(0.2f, 0.7f, 0.9f, 1.0f),
                        .kd_scale = 1.0f,
                    };
                    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params,
                            &fs_params, sizeof(fs_params));
                    sg_draw(0, model->num_points, 1);
                }

                {
                    struct beginning_camera_animation_entity *entity = 
                        &ce->hole->beginning_camera_animation_entity;
                    mat4 model_mat = beginning_camera_animation_entity_get_transform(entity);
                    struct model *model = asset_store_get_model("sphere");

                    sg_bindings bindings = {
                        .vertex_buffers[0] = model->positions_buf,
                        .vertex_buffers[1] = model->normals_buf,
                    };
                    sg_apply_bindings(&bindings);
                    single_color_vs_params_t vs_params = {
                        .model_mat = mat4_transpose(model_mat),
                        .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                    };
                    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                            &vs_params, sizeof(vs_params));
                    single_color_fs_params_t fs_params = {
                        .color = V4(1.0f, 1.0f, 0.0f, 1.0f),
                        .kd_scale = 1.0f,
                    };
                    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params,
                            &fs_params, sizeof(fs_params));
                    sg_draw(0, model->num_points, 1);
                }
            }
            if (ce->camera_zone_entities.can_modify) {
                for (int i = 0; i < ce->hole->camera_zone_entities.length; i++) {
                    struct camera_zone_entity *entity = &ce->hole->camera_zone_entities.data[i];

                    {
                        struct model *model = asset_store_get_model("cube");
                        mat4 model_mat = camera_zone_entity_get_transform(entity);

                        sg_bindings bindings = {
                            .vertex_buffers[0] = model->positions_buf,
                            .vertex_buffers[1] = model->normals_buf,
                        };
                        sg_apply_bindings(&bindings);

                        single_color_vs_params_t vs_params = {
                            .model_mat = mat4_transpose(model_mat),
                            .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                        };
                        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                                &vs_params, sizeof(vs_params));

                        single_color_fs_params_t fs_params = {
                            .color = V4(0.9f, 0.2f, 0.5f, 0.3f),
                            .kd_scale = 1.0f,
                        };
                        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params,
                                &fs_params, sizeof(fs_params));

                        sg_draw(0, model->num_points, 1);
                    }

                    {
                        struct model *model = asset_store_get_model("cube");
                        mat4 model_mat = mat4_multiply_n(3, 
                                mat4_translation(entity->position),
                                mat4_from_quat(entity->orientation),
                                mat4_scale(V3(1.0f, 0.1f, 0.1f)));

                        sg_bindings bindings = {
                            .vertex_buffers[0] = model->positions_buf,
                            .vertex_buffers[1] = model->normals_buf,
                        };
                        sg_apply_bindings(&bindings);

                        single_color_vs_params_t vs_params = {
                            .model_mat = mat4_transpose(model_mat),
                            .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                        };
                        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                                &vs_params, sizeof(vs_params));

                        single_color_fs_params_t fs_params = {
                            .color = V4(0.2f, 0.9f, 0.5f, 1.0f),
                            .kd_scale = 1.0f,
                        };
                        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params,
                                &fs_params, sizeof(fs_params));

                        sg_draw(0, model->num_points, 1);
                    }

                    {
                        struct model *model = asset_store_get_model("sphere");
                        mat4 model_mat = mat4_multiply_n(4, 
                                mat4_translation(entity->position),
                                mat4_from_quat(entity->orientation),
                                mat4_translation(V3(1.0f, 0.0f, 0.0f)),
                                mat4_scale(V3(0.2f, 0.2f, 0.2f)));

                        sg_bindings bindings = {
                            .vertex_buffers[0] = model->positions_buf,
                            .vertex_buffers[1] = model->normals_buf,
                        };
                        sg_apply_bindings(&bindings);

                        single_color_vs_params_t vs_params = {
                            .model_mat = mat4_transpose(model_mat),
                            .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                        };
                        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                                &vs_params, sizeof(vs_params));

                        single_color_fs_params_t fs_params = {
                            .color = V4(0.2f, 0.5f, 0.9f, 1.0f),
                            .kd_scale = 1.0f,
                        };
                        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params,
                                &fs_params, sizeof(fs_params));

                        sg_draw(0, model->num_points, 1);
                    }
                }
            }
            profiler_pop_section();
        }
        sg_end_pass();
    }

    {
        sg_pass_action hole_action0 = {
            .colors[0] = { .action = SG_ACTION_DONTCARE },
            .depth = {
                .action = SG_ACTION_DONTCARE,
            },
            .stencil = {
                .action = SG_ACTION_CLEAR,
            },
        };
        sg_begin_pass(renderer->sokol.game_pass, &hole_action0);

        profiler_push_section("draw_hole0");
        {
            struct cup_entity *hole = &ce->hole->cup_entity;
            struct model *model = asset_store_get_model("hole-cover");
            mat4 transform = cup_entity_get_transform(hole);

            sg_apply_pipeline(renderer->sokol.cup_pipeline[0]);

            sg_bindings bindings = {
                .vertex_buffers[0] = model->positions_buf,
            };
            sg_apply_bindings(&bindings);

            pass_through_vs_params_t vs_params = {
                .mvp_mat = mat4_transpose(mat4_multiply_n(2, renderer->proj_view_mat, transform)),
            };
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_pass_through_vs_params, &vs_params, sizeof(vs_params));

            sg_draw(0, model->num_points, 1);
        }
        profiler_pop_section();

        sg_end_pass();
    }

    {
        sg_pass_action hole_action1 = {
            .colors[0] = { .action = SG_ACTION_DONTCARE },
            .depth = {
                .action = SG_ACTION_CLEAR,
                .val = 1.0f,
            },
            .stencil = {
                .action = SG_ACTION_DONTCARE,
            },
        };
        sg_begin_pass(renderer->sokol.game_pass, &hole_action1);

        profiler_push_section("draw_hole1");
        {
            struct cup_entity *hole = &ce->hole->cup_entity;
            struct model *model = hole->model;
            struct lightmap *lightmap = &hole->lightmap;
            mat4 transform = cup_entity_get_transform(hole);

            sg_apply_pipeline(renderer->sokol.cup_pipeline[1]);

            sg_bindings bindings = {
                .vertex_buffers[0] = model->positions_buf,
                .vertex_buffers[1] = lightmap->uvs_buf,
                .fs_images[SLOT_lightmap_tex] = lightmap->images.data[0].sg_image,
            };
            sg_apply_bindings(&bindings);

            cup_vs_params_t vs_params = {
                .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                .model_mat = mat4_transpose(transform),
            };
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_cup_vs_params, &vs_params, sizeof(vs_params));

            sg_draw(0, model->num_points, 1);
        }
        profiler_pop_section();

        sg_end_pass();
    }

    {
        struct object_modifier *om = &ce->object_modifier;
        if (om->is_active) {
            profiler_push_section("draw_object_modifier");
            sg_pass_action pass_action = {
                .colors[0] = {
                    .action = SG_ACTION_DONTCARE,
                },
                .depth = {
                    .action = SG_ACTION_CLEAR,
                    .val = 1.0f,
                },
            };
            sg_begin_pass(renderer->sokol.game_pass, &pass_action);
            sg_apply_pipeline(renderer->sokol.objects_pipeline);

            if (om->mode == OBJECT_MODIFIER_MODE_TRANSLATE) {
                for (int i = 0; i < 3; i++) {
                    vec3 pos = om->pos;
                    float cone_scale = om->translate_mode.cone_scale;
                    float line_radius = om->translate_mode.line_radius;
                    vec3 axis = V3(0.0f, 0.0f, 0.0f);
                    if (i == 0) {
                        axis = V3(1.0f, 0.0f, 0.0f);
                    }
                    else if (i == 1) {
                        axis = V3(0.0f, 1.0f, 0.0f);
                    }
                    else if (i == 2) {
                        axis = V3(0.0f, 0.0f, 1.0f);
                    }
                    vec4 color = V4(axis.x, axis.y, axis.z, 1.0f);
                    axis = vec3_apply_mat4(axis, 0.0f, om->model_mat);
                    vec3 cone_pos = vec3_add(pos, vec3_scale(axis, om->translate_mode.cone_dist));
                    vec3 line_p0 = pos;
                    vec3 line_p1 = cone_pos;

                    {
                        struct model *cube_model = asset_store_get_model("cube");

                        sg_bindings bindings = {
                            .vertex_buffers[0] = cube_model->positions_buf,
                            .vertex_buffers[1] = cube_model->normals_buf,
                        };
                        sg_apply_bindings(&bindings);

                        single_color_vs_params_t vs_params = {
                            .model_mat = mat4_transpose(mat4_box_to_line_transform(line_p0, line_p1, line_radius)),
                            .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                        };
                        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                                &vs_params, sizeof(vs_params));

                        single_color_fs_params_t fs_params = {
                            .color = color,
                            .kd_scale = 0.5f,
                        };
                        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params,
                                &fs_params, sizeof(fs_params));

                        sg_draw(0, cube_model->num_points, 1);
                    }

                    {
                        struct model *cone_model = asset_store_get_model("cone");

                        sg_bindings bindings = {
                            .vertex_buffers[0] = cone_model->positions_buf,
                            .vertex_buffers[1] = cone_model->normals_buf,
                        };
                        sg_apply_bindings(&bindings);

                        mat4 model_mat = mat4_multiply_n(3,
                                mat4_translation(cone_pos),
                                mat4_scale(V3(cone_scale, cone_scale, cone_scale)),
                                mat4_from_quat(quat_between_vectors(V3(0.0f, 1.0f, 0.0f), axis)));
                        single_color_vs_params_t vs_params = {
                            .model_mat = mat4_transpose(model_mat),
                            .proj_view_mat =  mat4_transpose(renderer->proj_view_mat), 
                        };
                        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                                &vs_params, sizeof(vs_params));

                        float kd_scale = 0.5f;
                        if (om->translate_mode.cone_hovered[i] || 
                                (om->is_using && om->translate_mode.axis == i)) {
                            kd_scale = 0.2f;
                        }
                        single_color_fs_params_t fs_params = {
                            .color = color,
                            .kd_scale = kd_scale,
                        };
                        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params,
                                &fs_params, sizeof(fs_params));

                        sg_draw(0, cone_model->num_points, 1);
                    }
                }
            }
            else if (om->mode == OBJECT_MODIFIER_MODE_ROTATE) {
                float line_radius = om->rotate_mode.line_radius;
                struct model *cube_model = asset_store_get_model("cube");
                sg_bindings bindings = {
                    .vertex_buffers[0] = cube_model->positions_buf,
                    .vertex_buffers[1] = cube_model->normals_buf,
                };
                sg_apply_bindings(&bindings);

                for (int i = 0; i < 3; i++) {
                    vec3 axis = V3(0.0f, 0.0f, 0.0f);
                    if (i == 0) {
                        axis = V3(1.0f, 0.0f, 0.0f);
                    }
                    else if (i == 1) {
                        axis = V3(0.0f, 1.0f, 0.0f);
                    }
                    else if (i == 2) {
                        axis = V3(0.0f, 0.0f, 1.0f);
                    }
                    vec4 color = V4(axis.x, axis.y, axis.z, 1.0f);

                    for (int j = 0; j < OM_ROTATE_MODE_RES; j++) {
                        vec3 p0 = om->rotate_mode.line_segment_p0[OM_ROTATE_MODE_RES * i + j];
                        vec3 p1 = om->rotate_mode.line_segment_p1[OM_ROTATE_MODE_RES * i + j];
                        mat4 transform = mat4_box_to_line_transform(p0, p1, line_radius);

                        single_color_vs_params_t vs_params = {
                            .model_mat = mat4_transpose(transform),
                            .proj_view_mat = mat4_transpose(renderer->proj_view_mat),
                        };
                        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_single_color_vs_params,
                                &vs_params, sizeof(vs_params));

                        single_color_fs_params_t fs_params = {
                            .color = color,
                            .kd_scale = 0.5f,
                        };
                        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_single_color_fs_params,
                                &fs_params, sizeof(fs_params));

                        sg_draw(0, cube_model->num_points, 1);
                    }
                }
            }

            sg_end_pass();
            profiler_pop_section();
        }
    }

    /*
    if (ce->select_box.is_open) {
        profiler_push_section("draw_select_box");
        sg_pass_action ui_pass_action = {
            .colors[0] = { .action = SG_ACTION_DONTCARE },
            .depth = {
                .action = SG_ACTION_CLEAR,
            },
        };
        sg_begin_pass(renderer->sokol.game_pass, &ui_pass_action);

        vec2 p0 = V2(ce->select_box.p0.x, ce->select_box.p0.y);
        vec2 p1 = V2(ce->select_box.p1.x, ce->select_box.p0.y);
        vec2 p2 = V2(ce->select_box.p1.x, ce->select_box.p1.y);
        vec2 p3 = V2(ce->select_box.p0.x, ce->select_box.p1.y);
        float size = 1.0f;
        vec4 color = V4(1.0f, 1.0f, 1.0f, 1.0f);
        renderer_draw_ui_line(renderer, p0, p1, size, color);
        renderer_draw_ui_line(renderer, p1, p2, size, color);
        renderer_draw_ui_line(renderer, p2, p3, size, color);
        renderer_draw_ui_line(renderer, p3, p0, size, color);

        sg_end_pass();
        profiler_pop_section();
    }
    */

    {
        struct model *model = asset_store_get_model("square");
        sg_pass_action game_pass_action = {
            .colors[0] = { .action = SG_ACTION_CLEAR },
        };
        sg_begin_default_pass(&game_pass_action, renderer->window_width, renderer->window_height);
        sg_apply_pipeline(renderer->sokol.texture_pipeline);
        sg_bindings bindings = {
            .vertex_buffers[0] = model->positions_buf,
            .vertex_buffers[1] = model->texture_coords_buf,
            .fs_images[SLOT_texture_image] = renderer->sokol.game_color_image,
        };
        sg_apply_bindings(&bindings);
        texture_vs_params_t vs_params = {
            .mvp_mat = mat4_transpose(mat4_multiply_n(4,
                        renderer->ui_proj_mat,
                        mat4_scale(V3((float) renderer->game_fb_width, (float) renderer->game_fb_height, 1.0f)),
                        mat4_scale(V3(0.5f, 0.5f, 1.0f)),
                        mat4_translation(V3(1.0f, 1.0f, 0.0f))
                        )),
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_texture_vs_params, &vs_params, sizeof(vs_params));
        sg_draw(0, model->num_points, 1);
        sg_end_pass();
    }
}
