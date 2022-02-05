#include "golf/draw.h"

#include "common/data.h"
#include "common/graphics.h"
#include "common/level.h"
#include "common/log.h"
#include "common/maths.h"
#include "golf/game.h"
#include "golf/ui.h"

#include "golf/shaders/diffuse_color_material.glsl.h"
#include "golf/shaders/environment_material.glsl.h"
#include "golf/shaders/pass_through.glsl.h"
#include "golf/shaders/solid_color_material.glsl.h"
#include "golf/shaders/texture_material.glsl.h"
#include "golf/shaders/ui.glsl.h"
#include "golf/shaders/render_image.glsl.h"

typedef struct golf_draw {
    vec2 game_draw_pass_size;
    sg_image game_draw_pass_image, game_draw_pass_depth_image;
    sg_pass game_draw_pass;
} golf_draw_t;

static golf_draw_t draw;
static golf_game_t *game = NULL;
static golf_graphics_t *graphics = NULL;
static golf_ui_t *ui = NULL;

static void _golf_renderer_draw_environment_material(golf_model_t *model, int start, int count, mat4 model_mat, golf_material_t material, golf_lightmap_image_t *lightmap_image, golf_lightmap_section_t *lightmap_section, bool movement_repeats, float uv_scale, bool gi_on) {
    environment_material_vs_params_t vs_params = {
        .proj_view_mat = mat4_transpose(graphics->proj_view_mat),
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
                .proj_view_mat = mat4_transpose(graphics->proj_view_mat),
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
                .mvp_mat = mat4_transpose(mat4_multiply(graphics->proj_view_mat, model_mat)),
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



static void _draw_level(void) {
    golf_level_t *level = game->level;

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
                material = golf_material_texture("", 0, 0, "data/textures/colors/red.png");
            }
            else {
                if (!golf_level_get_material(level, group.material_name, &material)) {
                    golf_log_warning("Could not find material %s", group.material_name);
                    material = golf_material_texture("", 0, 0, "data/textures/fallback.png");
                }
            }

            switch (material.type) {
                case GOLF_MATERIAL_TEXTURE: {
                    sg_apply_pipeline(graphics->texture_material_pipeline);
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

                    sg_apply_pipeline(graphics->environment_material_pipeline);

                    bool movement_repeats = false;
                    if (movement) {
                        movement_repeats = movement->repeats;
                    }
                    _golf_renderer_draw_environment_material(model, group.start_vertex, group.vertex_count, model_mat, material, &lightmap_image, lightmap_section, movement_repeats, uv_scale, true);
                    break;
                }
            }
        }
    }

    sg_apply_pipeline(graphics->hole_pass1_pipeline);
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
                    .mvp_mat = mat4_transpose(mat4_multiply(graphics->proj_view_mat, model_mat)),
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

    sg_apply_pipeline(graphics->hole_pass2_pipeline);
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
                golf_material_t material = golf_material_texture("", 0, 0, "data/textures/hole_lightmap.png");
                _golf_renderer_draw_with_material(model, 0, model->positions.length, model_mat, material, 1);
                break;
            }
        }
    }
}

static void _draw_ui(void) {
    mat4 ui_proj_mat = mat4_orthographic_projection(0, graphics->viewport_size.x, graphics->viewport_size.y, 0, 0, 1); 

    sg_pass_action action = {
        .colors[0] = {
            .action = SG_ACTION_DONTCARE,
        },
        .depth = {
            .action = SG_ACTION_CLEAR,
            .value = 1.0f,
        }
    };
    sg_begin_default_pass(&action, (int)graphics->viewport_size.x, (int)graphics->viewport_size.y);
    sg_apply_viewportf(graphics->viewport_pos.x, graphics->viewport_pos.y, 
            graphics->viewport_size.x, graphics->viewport_size.y, true);
    sg_apply_pipeline(graphics->ui_pipeline);

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

static void _golf_draw_update_create_draw_pass(void) {
    sg_image_desc image_desc = {
        .render_target = true,
        .width = (int)draw.game_draw_pass_size.x,
        .height = (int)draw.game_draw_pass_size.y,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
    };
    draw.game_draw_pass_image = sg_make_image(&image_desc);

    sg_image_desc depth_image_desc = {
        .render_target = true,
        .width = (int)draw.game_draw_pass_size.x,
        .height = (int)draw.game_draw_pass_size.y,
        .pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
    };
    draw.game_draw_pass_depth_image = sg_make_image(&depth_image_desc);

    sg_pass_desc pass_desc = {
        .color_attachments[0] = {
            .image = draw.game_draw_pass_image,
        },
        .depth_stencil_attachment = {
            .image = draw.game_draw_pass_depth_image,  
        },
    };
    draw.game_draw_pass = sg_make_pass(&pass_desc);
}

void golf_draw_init(void) {
    game = golf_game_get();
    graphics = golf_graphics_get();
    ui = golf_ui_get();

    memset(&draw, 0, sizeof(draw));
    draw.game_draw_pass_size = graphics->viewport_size;
    _golf_draw_update_create_draw_pass();
}

void golf_draw(void) {
    if (!vec2_equal(draw.game_draw_pass_size, graphics->viewport_size)) {
        draw.game_draw_pass_size = graphics->viewport_size;
        sg_destroy_image(draw.game_draw_pass_image);
        sg_destroy_image(draw.game_draw_pass_depth_image);
        sg_destroy_pass(draw.game_draw_pass);
        _golf_draw_update_create_draw_pass();
    }

    /*
    sg_pass_action action = {
        .colors[0] = {
            .action = SG_ACTION_CLEAR,
            .value = { 0.529f, 0.808f, 0.922f, 1.0f },
        },
    };
    sg_begin_pass(draw.game_draw_pass, &action);
    _draw_level();
    sg_end_pass();
    */

    /*
    {
        sg_pass_action action = {
            .colors[0] = {
                .action = SG_ACTION_DONTCARE,
            },
        };

        sg_begin_default_pass(&action, (int)graphics->viewport_size.x, (int)graphics->viewport_size.y);
        sg_apply_viewportf(graphics->viewport_pos.x, graphics->viewport_pos.y, 
                graphics->viewport_size.x, graphics->viewport_size.y, true);
        sg_apply_pipeline(graphics->render_image_pipeline);
        golf_model_t *square = golf_data_get_model("data/models/render_image_square.obj");
        sg_bindings bindings = {
            .vertex_buffers[ATTR_render_image_vs_position] = square->sg_positions_buf,
            .vertex_buffers[ATTR_render_image_vs_texture_coord] = square->sg_texcoords_buf,
            .fs_images[SLOT_render_image_texture] = draw.game_draw_pass_image,
        };
        sg_apply_bindings(&bindings);
        sg_draw(0, square->positions.length, 1);
        sg_end_pass();
    }
    */

    _draw_ui();
}
