#include "golf/renderer.h"

#include <assert.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include "cimgui/cimgui.h"
#include "cimguizmo/cimguizmo.h"
#include "stb/stb_image.h"
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_imgui.h"
#include "golf/editor.h"
#include "golf/game.h"
#include "golf/log.h"
#include "golf/maths.h"
#include "golf/ui.h"

#include "golf/shaders/diffuse_color_material.glsl.h"
#include "golf/shaders/environment_material.glsl.h"
#include "golf/shaders/pass_through.glsl.h"
#include "golf/shaders/solid_color_material.glsl.h"
#include "golf/shaders/texture_material.glsl.h"
#include "golf/shaders/ui.glsl.h"
#include "golf/shaders/render_image.glsl.h"

static golf_renderer_t renderer;

static void _set_ui_proj_mat(vec2 pos_offset) {
    float fb_width = (float) 1280;
    float fb_height = (float) 720;
    float w_width = (float) sapp_width();
    float w_height = (float) sapp_height();
    float w_fb_width = w_width;
    float w_fb_height = (fb_height/fb_width)*w_fb_width;
    if (w_fb_height > w_height) {
        w_fb_height = w_height;
        w_fb_width = (fb_width/fb_height)*w_fb_height;
    }
    renderer.ui_proj_mat = mat4_multiply_n(4,
            mat4_orthographic_projection(0.0f, w_width, 0.0f, w_height, 0.0f, 1.0f),
            mat4_translation(V3(pos_offset.x, pos_offset.y, 0.0f)),
            mat4_translation(V3(0.5f*w_width - 0.5f*w_fb_width, 0.5f*w_height - 0.5f*w_fb_height, 0.0f)),
            mat4_scale(V3(w_fb_width/fb_width, w_fb_height/fb_height, 1.0f))
            );
}

golf_renderer_t *golf_renderer_get(void) {
    return &renderer;
}

void golf_renderer_init(void) {
    {
        renderer.window_size = V2(0, 0);
        renderer.viewport_pos = V2(0, 0);
        renderer.viewport_size = V2(0, 0);
        renderer.render_size = V2(0, 0);
        renderer.render_pass_size = V2(0, 0);
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
        renderer.render_image_pipeline = sg_make_pipeline(&pipeline_desc);
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
        renderer.diffuse_color_material_pipeline = sg_make_pipeline(&pipeline_desc);
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
        renderer.environment_material_pipeline = sg_make_pipeline(&pipeline_desc);
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
        renderer.solid_color_material_pipeline = sg_make_pipeline(&pipeline_desc);
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
        renderer.texture_material_pipeline = sg_make_pipeline(&pipeline_desc);
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
        renderer.ui_pipeline = sg_make_pipeline(&pipeline_desc);
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
        renderer.hole_pass1_pipeline = sg_make_pipeline(&pipeline_desc);
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
        renderer.hole_pass2_pipeline = sg_make_pipeline(&pipeline_desc);
    }

    renderer.cam_azimuth_angle = 0.5f*MF_PI;
    renderer.cam_inclination_angle = 0;
    renderer.cam_pos = V3(5, 5, 5);
    renderer.cam_dir = vec3_normalize(V3(-5, -5, -5));

    {
        float x = renderer.cam_dir.x;
        float y = renderer.cam_dir.y;
        float z = renderer.cam_dir.z;
        renderer.cam_inclination_angle = acosf(y);
        renderer.cam_azimuth_angle = atan2f(z, x);
        if (x < 0) {
            //renderer.cam_inclination_angle += MF_PI;
        }

        float theta = renderer.cam_inclination_angle;
        float phi = renderer.cam_azimuth_angle;
        renderer.cam_dir.x = sinf(theta) * cosf(phi);
        renderer.cam_dir.y = cosf(theta);
        renderer.cam_dir.z = sinf(theta) * sinf(phi);
    }

    renderer.cam_up = V3(0, 1, 0);
}

void golf_renderer_begin_frame(float dt) {
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

void golf_renderer_end_frame(void) {
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
    sg_end_pass();
    sg_commit();
}

void golf_renderer_set_viewport(vec2 pos, vec2 size) {
    renderer.viewport_pos = pos;
    renderer.viewport_size = size;
}

void golf_renderer_set_render_size(vec2 size) {
    renderer.render_size = size;
}

void golf_renderer_update(void) {
    renderer.window_size = V2(sapp_width(), sapp_height());

    if (!renderer.render_pass_inited || !vec2_equal(renderer.render_size, renderer.render_pass_size)) {
        renderer.render_pass_size = renderer.render_size;

        if (renderer.render_pass_inited) {
            sg_destroy_image(renderer.render_pass_image);
            sg_destroy_image(renderer.render_pass_depth_image);
            sg_destroy_pass(renderer.render_pass);
        }

        sg_image_desc render_pass_image_desc = {
            .render_target = true,
            .width = renderer.render_size.x,
            .height = renderer.render_size.y,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
        };
        renderer.render_pass_image = sg_make_image(&render_pass_image_desc);

        sg_image_desc render_pass_depth_image_desc = {
            .render_target = true,
            .width = renderer.render_size.x,
            .height = renderer.render_size.y,
            .pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL,
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
        };
        renderer.render_pass_depth_image = sg_make_image(&render_pass_depth_image_desc);

        sg_pass_desc render_pass_desc = {
            .color_attachments[0] = {
                .image = renderer.render_pass_image,
            },
            .depth_stencil_attachment = {
              .image = renderer.render_pass_depth_image  
            },
        };
        renderer.render_pass = sg_make_pass(&render_pass_desc);

        renderer.render_pass_inited = true;
    }

    {
        float theta = renderer.cam_inclination_angle;
        float phi = renderer.cam_azimuth_angle;
        renderer.cam_dir.x = sinf(theta) * cosf(phi);
        renderer.cam_dir.y = cosf(theta);
        renderer.cam_dir.z = sinf(theta) * sinf(phi);

        float near = 0.1f;
        float far = 150.0f;
        renderer.proj_mat = mat4_perspective_projection(66.0f,
                renderer.viewport_size.x / renderer.viewport_size.y, near, far);
        renderer.view_mat = mat4_look_at(renderer.cam_pos,
                vec3_add(renderer.cam_pos, renderer.cam_dir), renderer.cam_up);
        renderer.proj_view_mat = mat4_multiply(renderer.proj_mat, renderer.view_mat);
    }
}

static void _golf_renderer_draw_solid_color_material(golf_model_t *model, int start, int count, mat4 model_mat, golf_material_t material) {
    solid_color_material_vs_params_t vs_params = {
        .mvp_mat = mat4_transpose(mat4_multiply(renderer.proj_view_mat, model_mat)),
    };
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_solid_color_material_vs_params,
            &(sg_range) { &vs_params, sizeof(vs_params) });

    solid_color_material_fs_params_t fs_params = {
        .color = material.color,
    };
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_solid_color_material_fs_params,
            &(sg_range) { &fs_params, sizeof(fs_params) });

    sg_bindings bindings = {
        .vertex_buffers[0] = model->sg_positions_buf,
    };
    sg_apply_bindings(&bindings);

    sg_draw(start, count, 1);
}

static void _golf_renderer_draw_environment_material(golf_model_t *model, int start, int count, mat4 model_mat, golf_material_t material, golf_lightmap_image_t *lightmap_image, golf_lightmap_section_t *lightmap_section, bool movement_repeats, float uv_scale, bool gi_on) {
    environment_material_vs_params_t vs_params = {
        .proj_view_mat = mat4_transpose(renderer.proj_view_mat),
        .model_mat = mat4_transpose(model_mat),
    };
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_environment_material_vs_params,
            &(sg_range) { &vs_params, sizeof(vs_params) });

    int num_samples = lightmap_image->num_samples;
    int sample0 = 0;
    int sample1 = 0;
    float lightmap_t = 0;
    if (lightmap_image->time_length > 0) {
        float t = lightmap_image->cur_time / lightmap_image->time_length;
        if (movement_repeats) {
            t = 2 * t;
            if (t > 1) {
                t = 2 - t;
            }
        }

        for (int i = 1; i < num_samples; i++) {
            if (t < i / ((float) (num_samples - 1))) {
                sample0 = i - 1;
                sample1 = i;
                break;
            }

            float lightmap_t0 = sample0 / ((float) num_samples - 1);
            float lightmap_t1 = sample1 / ((float) num_samples - 1);
            lightmap_t = (t - lightmap_t0) / (lightmap_t1 - lightmap_t0);
            lightmap_t = golf_clampf(lightmap_t, 0, 1);
        }
    }

    environment_material_fs_params_t fs_params = {
        .lightmap_texture_a = lightmap_t,
        .uv_scale = uv_scale,
    };
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_solid_color_material_fs_params,
            &(sg_range) { &fs_params, sizeof(fs_params) });

    sg_bindings bindings = {
        .vertex_buffers[0] = model->sg_positions_buf,
        .vertex_buffers[1] = model->sg_texcoords_buf,
        .vertex_buffers[2] = model->sg_normals_buf,
        .vertex_buffers[3] = lightmap_section->sg_uvs_buf,
        .fs_images[SLOT_environment_material_texture] = material.texture->sg_image,
        .fs_images[SLOT_environment_material_lightmap_texture0] = lightmap_image->sg_image[sample0],
        .fs_images[SLOT_environment_material_lightmap_texture1] = lightmap_image->sg_image[sample1],
    };
    if (!gi_on) {
        golf_texture_t *white = golf_data_get_texture("data/textures/colors/white.png");
        bindings.fs_images[SLOT_environment_material_lightmap_texture0] = white->sg_image;
        bindings.fs_images[SLOT_environment_material_lightmap_texture1] = white->sg_image;
    }
    sg_apply_bindings(&bindings);

    sg_draw(start, count, 1);
}

static void _golf_renderer_draw_with_material(golf_model_t *model, int start, int count, mat4 model_mat, golf_material_t material, float uv_scale) {
    switch (material.type) {
        case GOLF_MATERIAL_TEXTURE: {
            texture_material_vs_params_t vs_params = {
                .proj_view_mat = mat4_transpose(renderer.proj_view_mat),
                .model_mat = mat4_transpose(model_mat),
            };
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_texture_material_vs_params, 
                    &(sg_range) { &vs_params, sizeof(vs_params) });

            sg_bindings bindings = {
                .vertex_buffers[0] = model->sg_positions_buf,
                .vertex_buffers[1] = model->sg_texcoords_buf,
                .vertex_buffers[2] = model->sg_normals_buf,
                .fs_images[SLOT_texture_material_tex] = material.texture->sg_image,
            };
            sg_apply_bindings(&bindings);

            sg_draw(start, count, 1);
            break;
        }
        case GOLF_MATERIAL_COLOR: {
            solid_color_material_vs_params_t vs_params = {
                .mvp_mat = mat4_transpose(mat4_multiply(renderer.proj_view_mat, model_mat)),
            };
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_solid_color_material_vs_params,
                    &(sg_range) { &vs_params, sizeof(vs_params) });

            solid_color_material_fs_params_t fs_params = {
                .color = material.color,
            };
            sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_solid_color_material_fs_params,
                    &(sg_range) { &fs_params, sizeof(fs_params) });

            sg_bindings bindings = {
                .vertex_buffers[0] = model->sg_positions_buf,
            };
            sg_apply_bindings(&bindings);

            sg_draw(start, count, 1);
            break;
        }
        case GOLF_MATERIAL_DIFFUSE_COLOR: {
            break;
        }
        case GOLF_MATERIAL_ENVIRONMENT: {
            break;
        }
    }
}


static void _draw_level(golf_level_t *level, bool gi_on) {
    for (int i = 0; i < level->entities.length; i++) {
        golf_entity_t *entity = &level->entities.data[i];
        if (!entity->active || entity->type == HOLE_ENTITY) continue;

        golf_model_t *model = golf_entity_get_model(entity);
        if (!model) continue;

        float uv_scale = 1.0f;
        if (entity->type == MODEL_ENTITY) {
            uv_scale = entity->model.uv_scale;
        }

        golf_transform_t *transform = golf_entity_get_transform(entity);
        if (!transform) continue;
        golf_transform_t world_transform = golf_entity_get_world_transform(level, entity);

        mat4 model_mat;
        golf_movement_t *movement = golf_entity_get_movement(entity);
        if (movement) {
            golf_transform_t moved_transform = golf_transform_apply_movement(world_transform, *movement);
            model_mat = golf_transform_get_model_mat(moved_transform);
        }
        else {
            model_mat = golf_transform_get_model_mat(world_transform);
        }

        for (int i = 0; i < model->groups.length; i++) {
            golf_model_group_t group = model->groups.data[i];
            golf_material_t material;
            if (entity->type == BALL_START_ENTITY) {
                material = golf_material_texture("data/textures/colors/red.png");
            }
            else {
                if (!golf_level_get_material(level, group.material_name, &material)) {
                    golf_log_warning("Could not find material %s", group.material_name);
                    material = golf_material_texture("data/textures/fallback.png");
                }
            }

            switch (material.type) {
                case GOLF_MATERIAL_TEXTURE: {
                    sg_apply_pipeline(renderer.texture_material_pipeline);
                    _golf_renderer_draw_with_material(model, group.start_vertex, group.vertex_count, model_mat, material, 1);
                    break;
                }
                case GOLF_MATERIAL_COLOR: {
                    break;
                }
                case GOLF_MATERIAL_DIFFUSE_COLOR: {
                    break;
                }
                case GOLF_MATERIAL_ENVIRONMENT: {
                    golf_lightmap_section_t *lightmap_section = golf_entity_get_lightmap_section(entity);
                    if (!lightmap_section) {
                        golf_log_warning("Cannot use environment material on entity with no lightmap");
                        break;
                    }

                    golf_lightmap_image_t lightmap_image;
                    if (!golf_level_get_lightmap_image(level, lightmap_section->lightmap_name, &lightmap_image)) {
                        golf_log_warning("Could not find lightmap %s", lightmap_section->lightmap_name);
                        break;
                    }

                    sg_apply_pipeline(renderer.environment_material_pipeline);

                    bool movement_repeats = false;
                    if (movement) {
                        movement_repeats = movement->repeats;
                    }
                    _golf_renderer_draw_environment_material(model, group.start_vertex, group.vertex_count, model_mat, material, &lightmap_image, lightmap_section, movement_repeats, uv_scale, gi_on);
                    break;
                }
            }
        }
    }

    sg_apply_pipeline(renderer.hole_pass1_pipeline);
    for (int i = 0; i < level->entities.length; i++) {
        golf_entity_t *entity = &level->entities.data[i];
        if (!entity->active) continue;

        switch (entity->type) {
            case MODEL_ENTITY:
            case BALL_START_ENTITY:
            case GEO_ENTITY:
            case GROUP_ENTITY:
                break;
            case HOLE_ENTITY: {
                golf_transform_t transform = entity->hole.transform;
                transform.position.y += 0.001f;
                mat4 model_mat = golf_transform_get_model_mat(transform);
                golf_model_t *model = golf_data_get_model("data/models/hole-cover.obj");

                pass_through_vs_params_t vs_params = {
                    .mvp_mat = mat4_transpose(mat4_multiply(renderer.proj_view_mat, model_mat)),
                };
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_pass_through_vs_params,
                        &(sg_range) { &vs_params, sizeof(vs_params) });
                sg_bindings bindings = {
                    .vertex_buffers[0] = model->sg_positions_buf,
                };
                sg_apply_bindings(&bindings);
                sg_draw(0, model->positions.length, 1);

                break;
            }
        }
    }

    sg_apply_pipeline(renderer.hole_pass2_pipeline);
    for (int i = 0; i < level->entities.length; i++) {
        golf_entity_t *entity = &level->entities.data[i];
        if (!entity->active) continue;

        switch (entity->type) {
            case MODEL_ENTITY:
            case BALL_START_ENTITY:
            case GEO_ENTITY:
            case GROUP_ENTITY:
                break;
            case HOLE_ENTITY: {
                golf_transform_t transform = entity->hole.transform;
                transform.position.y += 0.001f;
                mat4 model_mat = golf_transform_get_model_mat(transform);
                golf_model_t *model = golf_data_get_model("data/models/hole.obj");
                golf_material_t material = golf_material_texture("data/textures/hole_lightmap.png");
                _golf_renderer_draw_with_material(model, 0, model->positions.length, model_mat, material, 1);
                break;
            }
        }
    }
}

static void _draw_game(void) {
    golf_game_t *game = golf_game_get();

    sg_pass_action action = {
        .colors[0] = {
            .action = SG_ACTION_CLEAR,
            .value = { 0.529f, 0.808f, 0.922f, 1.0f },
        },
    };
    sg_begin_pass(renderer.render_pass, &action);
    _draw_level(game->level, true);
    sg_end_pass();

    {
        sg_begin_default_pass(&action, sapp_width(), sapp_height());
        sg_apply_viewportf(renderer.viewport_pos.x, renderer.viewport_pos.y, 
                renderer.viewport_size.x, renderer.viewport_size.y, true);
        sg_apply_pipeline(renderer.render_image_pipeline);

        golf_model_t *square = golf_data_get_model("data/models/render_image_square.obj");
        sg_bindings bindings = {
            .vertex_buffers[ATTR_render_image_vs_position] = square->sg_positions_buf,
            .vertex_buffers[ATTR_render_image_vs_texture_coord] = square->sg_texcoords_buf,
            .fs_images[SLOT_render_image_texture] = renderer.render_pass_image,
        };
        sg_apply_bindings(&bindings);

        sg_draw(0, square->positions.length, 1);

        sg_end_pass();
    }
}

static void _draw_ui(void) {
    mat4 ui_proj_mat = mat4_orthographic_projection(0, renderer.viewport_size.x, 
            renderer.viewport_size.y, 0, 0, 1); 

    sg_pass_action action = {
        .colors[0] = {
            .action = SG_ACTION_DONTCARE,
        },
        .depth = {
            .action = SG_ACTION_CLEAR,
            .value = 1.0f,
        }
    };
    sg_begin_default_pass(&action, sapp_width(), sapp_height());
    sg_apply_viewportf(renderer.viewport_pos.x, renderer.viewport_pos.y, 
            renderer.viewport_size.x, renderer.viewport_size.y, true);
    sg_apply_pipeline(renderer.ui_pipeline);

    golf_ui_t *ui = golf_ui_get();
    for (int i = 0; i < ui->draw_entities.length; i++) {
        golf_ui_draw_entity_t draw = ui->draw_entities.data[i];
        vec3 translate = V3(draw.pos.x, draw.pos.y, 0);
        vec3 scale = V3(0.5f * draw.size.x, 0.5f * draw.size.y, 1);

        golf_model_t *square = golf_data_get_model("data/models/ui_square.obj");
        sg_bindings bindings = {
            .vertex_buffers[ATTR_ui_vs_position] = square->sg_positions_buf,
            .vertex_buffers[ATTR_ui_vs_texture_coord] = square->sg_texcoords_buf,
            .fs_images[SLOT_ui_texture] = draw.image,
        };
        sg_apply_bindings(&bindings);

        ui_vs_params_t vs_params = {
            .mvp_mat = mat4_transpose(mat4_multiply_n(3,
                        ui_proj_mat,
                        mat4_translation(translate),
                        mat4_scale(scale)))
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_ui_vs_params,
                &(sg_range) { &vs_params, sizeof(vs_params) } );

        ui_fs_params_t fs_params = {
            .tex_x = draw.uv0.x,
            .tex_y = draw.uv0.y, 
            .tex_dx = draw.uv1.x - draw.uv0.x, 
            .tex_dy = draw.uv1.y - draw.uv0.y,
            .is_font = draw.is_font,
            .color = draw.overlay_color,
        };
        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ui_fs_params,
                &(sg_range) { &fs_params, sizeof(fs_params) });

        sg_draw(0, square->positions.length, 1);
    }

    sg_end_pass();
}

void golf_renderer_draw(void) {
    _set_ui_proj_mat(V2(0.0f, 0.0f));

    _draw_game();
    _draw_ui();
}

/*
void golf_renderer_draw_editor(void) {
    golf_editor_t *editor = golf_editor_get();
    golf_config_t *editor_cfg = golf_data_get_config("data/config/editor.cfg");

    sg_pass_action action = {
        .colors[0] = {
            .action = SG_ACTION_CLEAR,
            .value = { 0.529f, 0.808f, 0.922f, 1.0f },
        },
    };
    sg_begin_pass(renderer.render_pass, &action);

    _draw_level(editor->level, editor->renderer.gi_on);

    if (editor->in_edit_mode) {
        golf_geo_t *geo = editor->edit_mode.geo;
        mat4 model_mat = golf_transform_get_model_mat(editor->edit_mode.transform);

        sg_apply_pipeline(renderer.solid_color_material_pipeline);
        for (int i = 0; i < geo->points.length; i++) {
            golf_geo_point_t p = geo->points.data[i];
            if (!p.active) continue;

            vec3 pos = vec3_apply_mat4(p.position, 1, model_mat);
            float sz = CFG_NUM(editor_cfg, "edit_mode_sphere_size");
            mat4 sphere_model_mat = mat4_multiply_n(2, mat4_translation(pos), mat4_scale(V3(sz, sz, sz)));
            golf_model_t *model = golf_data_get_model("data/models/sphere.obj");
            int start = 0;
            int count = model->positions.length;
            vec3 color = CFG_VEC3(editor_cfg, "edit_mode_selectable_color");
            golf_edit_mode_entity_t entity = golf_edit_mode_entity_point(i);
            if (golf_editor_is_edit_entity_hovered(entity)) {
                color = CFG_VEC3(editor_cfg, "edit_mode_hovered_color");
            }
            else if (golf_editor_is_edit_entity_selected(entity)) {  
                color = CFG_VEC3(editor_cfg, "edit_mode_selected_color");
            }
            golf_material_t material = golf_material_color(V4(color.x, color.y, color.z, 1));
            _golf_renderer_draw_solid_color_material(model, start, count, sphere_model_mat, material);
        }

        for (int i = 0; i < geo->faces.length; i++) {
            golf_geo_face_t face = geo->faces.data[i];
            if (!face.active) continue;

            int n = face.idx.length;
            for (int i = 0; i < n; i++) {
                int idx0 = face.idx.data[i];
                int idx1 = face.idx.data[(i + 1) % n];
                golf_geo_point_t p0 = geo->points.data[idx0];
                golf_geo_point_t p1 = geo->points.data[idx1];
                vec3 pos0 = vec3_apply_mat4(p0.position, 1, model_mat);
                vec3 pos1 = vec3_apply_mat4(p1.position, 1, model_mat);
                //vec3 pos_avg = vec3_scale(vec3_add(pos0, pos1), 0.5f);
                float sz = CFG_NUM(editor_cfg, "edit_mode_line_size");
                vec3 color = CFG_VEC3(editor_cfg, "edit_mode_selectable_color");
                golf_edit_mode_entity_t entity = golf_edit_mode_entity_line(idx0, idx1);
                if (golf_editor_is_edit_entity_hovered(entity)) {
                    color = CFG_VEC3(editor_cfg, "edit_mode_hovered_color");
                    sz += 0.001f;
                }
                else if (golf_editor_is_edit_entity_selected(entity)) {
                    color = CFG_VEC3(editor_cfg, "edit_mode_selected_color");
                    sz += 0.001f;
                }
                mat4 line_model_mat = mat4_box_to_line_transform(pos0, pos1, sz);
                golf_model_t *model = golf_data_get_model("data/models/cube.obj");
                int start = 0;
                int count = model->positions.length;
                golf_material_t material = golf_material_color(V4(color.x, color.y, color.z, 1));
                _golf_renderer_draw_solid_color_material(model, start, count, line_model_mat, material);
            }

            golf_edit_mode_entity_t entity = golf_edit_mode_entity_face(i);
            if (golf_editor_is_edit_entity_hovered(entity)) {
                golf_model_t *model = &geo->model;
                int start_vertex = face.start_vertex_in_model;
                int count = 3 * (n - 2);
                vec3 color = CFG_VEC3(editor_cfg, "edit_mode_hovered_color");
                float alpha = CFG_NUM(editor_cfg, "edit_mode_hovered_face_alpha");
                golf_material_t material = golf_material_color(V4(color.x, color.y, color.z, alpha));
                _golf_renderer_draw_solid_color_material(model, start_vertex, count, model_mat, material);
            }
            else if (golf_editor_is_edit_entity_selected(entity)) {
                golf_model_t *model = &geo->model;
                int start_vertex = face.start_vertex_in_model;
                int count = 3 * (n - 2);
                vec3 color = CFG_VEC3(editor_cfg, "edit_mode_selected_color");
                float alpha = CFG_NUM(editor_cfg, "edit_mode_selected_face_alpha");
                golf_material_t material = golf_material_color(V4(color.x, color.y, color.z, alpha));
                _golf_renderer_draw_solid_color_material(model, start_vertex, count, model_mat, material);
            }
        }
    }

    sg_end_pass();

    {
        sg_begin_default_pass(&action, sapp_width(), sapp_height());
        sg_apply_viewportf(renderer.viewport_pos.x, renderer.viewport_pos.y, 
                renderer.viewport_size.x, renderer.viewport_size.y, true);
        sg_apply_pipeline(renderer.render_image_pipeline);

        golf_model_t *square = golf_data_get_model("data/models/render_image_square.obj");
        sg_bindings bindings = {
            .vertex_buffers[ATTR_render_image_vs_position] = square->sg_positions_buf,
            .vertex_buffers[ATTR_render_image_vs_texture_coord] = square->sg_texcoords_buf,
            .fs_images[SLOT_render_image_texture] = renderer.render_pass_image,
        };
        sg_apply_bindings(&bindings);

        sg_draw(0, square->positions.length, 1);

        sg_end_pass();
    }
}
*/

vec2 golf_renderer_world_to_screen(vec3 pos) {
    vec4 t = V4(pos.x, pos.y, pos.z, 1);
    t = vec4_apply_mat(t, renderer.proj_view_mat);
    t = vec4_scale(t, 0.5f / t.w);
    t = vec4_add(t, V4(0.5f, 0.5f, 0, 0));
    t.y = 1 - t.y;
    t.x = t.x * renderer.viewport_size.x;
    t.y = t.y * renderer.viewport_size.y;
    t.x = t.x + renderer.viewport_pos.x;
    t.y = t.y + renderer.viewport_pos.y;
    return V2(t.x, t.y);
}

vec3 golf_renderer_screen_to_world(vec3 screen_point) {
    float near = 0.1f;
    float far = 150.0f;
    float c = (far + near) / (near - far);
    float d = (2.0f * far * near) / (near - far);
    screen_point.x = -1.0f + (2.0f * screen_point.x / renderer.viewport_size.x);
    screen_point.y = -1.0f + (2.0f * screen_point.y / renderer.viewport_size.y);
    float w = d / (screen_point.z + c);
    vec4 screen = V4(screen_point.x * w, screen_point.y * w, screen_point.z * w, w);
    vec4 world = vec4_apply_mat(screen, mat4_inverse(renderer.proj_view_mat));
    return V3(world.x, world.y, world.z);
}

void golf_renderer_debug_console_tab(void) {
    igText("Window size: <%.3f, %.3f>", renderer.window_size.x, renderer.window_size.y); 
    igText("Viewport Pos: <%.3f, %.3f>", renderer.viewport_pos.x, renderer.viewport_pos.y); 
    igText("Viewport Size: <%.3f, %.3f>", renderer.viewport_size.x, renderer.viewport_size.y); 
    igText("Render Size: <%.3f, %.3f>", renderer.render_size.x, renderer.render_size.y); 
    igText("Cam Pos: <%.3f, %.3f, %.3f>", renderer.cam_pos.x, renderer.cam_pos.y, renderer.cam_pos.z); 
    igText("Cam Dir: <%.3f, %.3f, %.3f>", renderer.cam_dir.x, renderer.cam_dir.y, renderer.cam_dir.z); 
    igText("Cam Up: <%.3f, %.3f, %.3f>", renderer.cam_up.x, renderer.cam_up.y, renderer.cam_up.z); 
    igText("Cam Inclination: %.3f", renderer.cam_inclination_angle); 
    igText("Cam Azimuth: %.3f", renderer.cam_azimuth_angle); 
}
