#include "golf/draw.h"

#include <float.h>

#include "common/data.h"
#include "common/graphics.h"
#include "common/level.h"
#include "common/log.h"
#include "common/maths.h"
#include "golf/game.h"
#include "golf/golf.h"
#include "golf/ui.h"

typedef struct golf_draw {
    vec2 game_draw_pass_size;
    sg_image game_draw_pass_image, game_draw_pass_depth_image;
    sg_pass game_draw_pass;
} golf_draw_t;

static golf_draw_t draw;
static golf_game_t *game = NULL;
static golf_t *golf = NULL;
static golf_graphics_t *graphics = NULL;
static golf_ui_t *ui = NULL;

static void _draw_game(void) {
    golf_level_t *level = golf->level;

    {
        sg_pass_action action = {
            .colors[0] = {
                .action = SG_ACTION_CLEAR,
                .value = { 0.529f, 0.808f, 0.922f, 1.0f },
            },
        };
        sg_begin_pass(draw.game_draw_pass, &action);

        // Draw environment
        {
            golf_shader_t *shader = golf_data_get_shader("data/shaders/environment_material.glsl");
            golf_shader_pipeline_t *pipeline = golf_shader_get_pipeline(shader, "environment_material");
            sg_apply_pipeline(pipeline->sg_pipeline);

            for (int i = 0; i < level->entities.length; i++) {
                golf_entity_t *entity = &level->entities.data[i];
                if (entity->type != GEO_ENTITY && entity->type != MODEL_ENTITY) {
                    continue;
                }

                float uv_scale = 1;
                if (entity->type == MODEL_ENTITY) {
                    uv_scale = entity->model.uv_scale;
                }

                golf_model_t *model = golf_entity_get_model(entity);
                golf_transform_t world_transform = golf_entity_get_world_transform(level, entity);
                golf_movement_t *movement = golf_entity_get_movement(entity);
                if (movement) {
                    world_transform = golf_transform_apply_movement(world_transform, *movement, game->t);
                }
                mat4 model_mat = golf_transform_get_model_mat(world_transform);

                golf_lightmap_section_t *lightmap_section = golf_entity_get_lightmap_section(entity);
                golf_lightmap_image_t lightmap_image;
                if (!golf_level_get_lightmap_image(level, lightmap_section->lightmap_name, &lightmap_image)) {
                    golf_log_warning("Could not find lightmap %s", lightmap_section->lightmap_name);
                    continue;
                }

                for (int i = 0; i < model->groups.length; i++) {
                    golf_model_group_t group = model->groups.data[i];
                    golf_material_t material;
                    if (!golf_level_get_material(level, group.material_name, &material)) {
                        golf_log_warning("Could not find material %s", group.material_name);
                        material = golf_material_texture("", 0, 0, 0, "data/textures/fallback.png");
                    }
                    if (material.type != GOLF_MATERIAL_ENVIRONMENT) {
                        continue;
                    }

                    int num_samples = lightmap_image.num_samples;
                    int sample0 = 0;
                    int sample1 = 0;
                    float lightmap_t = 0;
                    if (lightmap_image.time_length > 0 && lightmap_image.num_samples > 1) {
                        float t = fmodf(game->t, lightmap_image.time_length) / lightmap_image.time_length;
                        if (lightmap_image.repeats) {
                            t = 2.0f * t;
                            if (t > 1.0f) {
                                t = 2.0f - t;
                            }
                        }
                        for (int i = 1; i < num_samples; i++) {
                            if (t < i / ((float) (num_samples - 1))) {
                                sample0 = i - 1;
                                sample1 = i;
                                break;
                            }
                        }

                        float lightmap_t0 = sample0 / ((float) num_samples - 1);
                        float lightmap_t1 = sample1 / ((float) num_samples - 1);
                        lightmap_t = (t - lightmap_t0) / (lightmap_t1 - lightmap_t0);
                        lightmap_t = golf_clampf(lightmap_t, 0, 1);
                    }

                    vec3 ball_pos = game->ball.draw_pos;

                    golf_shader_uniform_t *vs_uniform = golf_shader_vs_uniform_setup(shader, "environment_material_vs_params", 2,
                            UNIFORM_MAT4("proj_view_mat", mat4_transpose(graphics->proj_view_mat)),
                            UNIFORM_MAT4("model_mat", mat4_transpose(model_mat)));
                    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &(sg_range) { vs_uniform->data, vs_uniform->size });

                    golf_shader_uniform_t *fs_uniform = golf_shader_fs_uniform_setup(shader, "environment_material_fs_params", 3,
                            UNIFORM_VEC4("ball_position", V4(ball_pos.x, ball_pos.y, ball_pos.z, 0)),
                            UNIFORM_FLOAT("lightmap_texture_a", lightmap_t),
                            UNIFORM_FLOAT("uv_scale", uv_scale));
                    sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &(sg_range) { fs_uniform->data, fs_uniform->size });

                    sg_bindings bindings = {
                        .vertex_buffers[0] = model->sg_positions_buf,
                        .vertex_buffers[1] = model->sg_texcoords_buf,
                        .vertex_buffers[2] = model->sg_normals_buf,
                        .vertex_buffers[3] = lightmap_section->sg_uvs_buf,
                        .fs_images[0] = material.texture->sg_image,
                        .fs_images[1] = lightmap_image.sg_image[sample0],
                        .fs_images[2] = lightmap_image.sg_image[sample1],
                    };
                    sg_apply_bindings(&bindings);

                    sg_draw(group.start_vertex, group.vertex_count, 1);
                }
            }
        }

        // Draw the ball
        {
            golf_shader_t *shader = golf_data_get_shader("data/shaders/ball.glsl");
            golf_shader_pipeline_t *pipeline = golf_shader_get_pipeline(shader, "ball");
            sg_apply_pipeline(pipeline->sg_pipeline);

            vec3 ball_pos = game->ball.draw_pos;
            vec3 ball_scale = V3(game->ball.radius, game->ball.radius, game->ball.radius);
            golf_model_t *model = golf_data_get_model("data/models/golf_ball.obj");
            mat4 model_mat = mat4_multiply_n(3,
                    mat4_translation(ball_pos),
                    mat4_scale(ball_scale),
                    mat4_from_quat(game->ball.orientation));
            golf_texture_t *texture = golf_data_get_texture("data/textures/golf_ball_normal_map.jpg");
            vec4 color = V4(1, 1, 1, 1);

            sg_bindings bindings = {
                .vertex_buffers[0] = model->sg_positions_buf,
                .vertex_buffers[1] = model->sg_normals_buf,
                .vertex_buffers[2] = model->sg_texcoords_buf,
                .fs_images[0] = texture->sg_image,
            };
            sg_apply_bindings(&bindings);

            golf_shader_uniform_t *vs_uniform = golf_shader_vs_uniform_setup(shader, "ball_vs_params", 2,
                    UNIFORM_MAT4("proj_view_mat", mat4_transpose(graphics->proj_view_mat)),
                    UNIFORM_MAT4("model_mat", mat4_transpose(model_mat)));
            sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &(sg_range) { vs_uniform->data, vs_uniform->size });

            golf_shader_uniform_t *fs_uniform = golf_shader_fs_uniform_setup(shader, "ball_fs_params", 1,
                    UNIFORM_VEC4("color", color));
            sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &(sg_range) { fs_uniform->data, fs_uniform->size });

            sg_draw(0, model->positions.length, 1);
        }

        // Draw the ball hidden behind objects
        {
            golf_shader_t *shader = golf_data_get_shader("data/shaders/ball_hidden.glsl");
            golf_shader_pipeline_t *pipeline = golf_shader_get_pipeline(shader, "ball_hidden");
            sg_apply_pipeline(pipeline->sg_pipeline);

            vec3 cam_pos = graphics->cam_pos;
            vec3 ball_pos = game->ball.draw_pos;
            float ball_radius = game->ball.radius;
            mat4 model_mat = mat4_multiply_n(2,
                    mat4_translation(ball_pos),
                    mat4_scale(V3(ball_radius + 0.001f, ball_radius + 0.001f, ball_radius + 0.001f)));
            golf_model_t *model = golf_data_get_model("data/models/golf_ball.obj");

            sg_bindings bindings = {
                .vertex_buffers[0] = model->sg_positions_buf,
                .vertex_buffers[1] = model->sg_normals_buf,
            };
            sg_apply_bindings(&bindings);

            golf_shader_uniform_t *vs_uniform = golf_shader_vs_uniform_setup(shader, "vs_params", 2,
                    UNIFORM_MAT4("model_mat", mat4_transpose(model_mat)),
                    UNIFORM_MAT4("proj_view_mat", mat4_transpose(graphics->proj_view_mat)));
            sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &(sg_range) { vs_uniform->data, vs_uniform->size });

            golf_shader_uniform_t *fs_uniform = golf_shader_fs_uniform_setup(shader, "fs_params", 2,
                    UNIFORM_VEC4("ball_position", V4(ball_pos.x, ball_pos.y, ball_pos.z, 0)),
                    UNIFORM_VEC4("cam_position", V4(cam_pos.x, cam_pos.y, cam_pos.z, 0)));
            sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &(sg_range) { fs_uniform->data, fs_uniform->size });

            sg_draw(0, model->positions.length, 1);
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
        sg_begin_pass(draw.game_draw_pass, &water_pass_action);

        // Draw the water
        {
            golf_shader_t *shader = golf_data_get_shader("data/shaders/water.glsl");
            golf_shader_pipeline_t *pipeline = golf_shader_get_pipeline(shader, "water");
            sg_apply_pipeline(pipeline->sg_pipeline);
            for (int i = 0; i < level->entities.length; i++) {
                golf_entity_t *entity = &level->entities.data[i];
                if (entity->type != WATER_ENTITY) continue;

                golf_model_t *model = golf_entity_get_model(entity);
                golf_transform_t world_transform = golf_entity_get_world_transform(level, entity);
                mat4 model_mat = golf_transform_get_model_mat(world_transform);
                golf_texture_t *noise_tex0 = golf_data_get_texture("data/textures/water_noise_1.png");
                golf_texture_t *noise_tex1 = golf_data_get_texture("data/textures/water_noise_2.png");

                golf_lightmap_section_t *lightmap_section = golf_entity_get_lightmap_section(entity);
                golf_lightmap_image_t lightmap_image;
                if (!golf_level_get_lightmap_image(level, lightmap_section->lightmap_name, &lightmap_image)) {
                    golf_log_warning("Could not find lightmap %s", lightmap_section->lightmap_name);
                    continue;
                }

                sg_bindings bindings = {
                    .vertex_buffers[0] = model->sg_positions_buf,
                    .vertex_buffers[1] = model->sg_texcoords_buf,
                    .vertex_buffers[2] = lightmap_section->sg_uvs_buf,
                    .fs_images[0] = lightmap_image.sg_image[0],
                    .fs_images[1] = noise_tex0->sg_image,
                    .fs_images[2] = noise_tex1->sg_image,
                };
                sg_apply_bindings(&bindings);

                golf_shader_uniform_t *vs_uniform = golf_shader_get_vs_uniform(shader, "water_vs_params");
                golf_shader_uniform_set_mat4(vs_uniform, "model_mat", mat4_transpose(model_mat));
                golf_shader_uniform_set_mat4(vs_uniform, "proj_view_mat", mat4_transpose(graphics->proj_view_mat));
                sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &(sg_range) { vs_uniform->data, vs_uniform->size });

                golf_shader_uniform_t *fs_uniform = golf_shader_get_fs_uniform(shader, "water_fs_params");
                golf_shader_uniform_set_float(fs_uniform, "t", game->t);
                sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &(sg_range) { fs_uniform->data, fs_uniform->size });

                sg_draw(0, model->positions.length, 1);
            }
        }

        // Draw water around the ball
        if (game->ball.is_in_water) {
            golf_shader_t *shader = golf_data_get_shader("data/shaders/water_around_ball.glsl");
            golf_shader_pipeline_t *pipeline = golf_shader_get_pipeline(shader, "water_around_ball");
            sg_apply_pipeline(pipeline->sg_pipeline);

            golf_texture_t *noise_tex = golf_data_get_texture("data/textures/water_noise_3.png");
            golf_model_t *model = golf_data_get_model("data/models/ui_square.obj");
            sg_bindings bindings = {
                .vertex_buffers[0] = model->sg_positions_buf,
                .vertex_buffers[1] = model->sg_texcoords_buf,
                .fs_images[0] = noise_tex->sg_image,
            };
            sg_apply_bindings(&bindings);

            golf_shader_uniform_t *vs_params = golf_shader_get_vs_uniform(shader, "vs_params");
            golf_shader_uniform_set_mat4(vs_params, "mvp_mat", mat4_transpose(
                        mat4_multiply_n(4, 
                            graphics->proj_view_mat,
                            mat4_translation(vec3_sub(game->ball.draw_pos, V3(0.0f, 0.02f, 0.0f))),
                            mat4_scale(V3(0.4f, 0.4f, 0.4f)),
                            mat4_rotation_x(-0.5f * MF_PI))));
            sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &(sg_range) { vs_params->data, vs_params->size });

            golf_shader_uniform_t *fs_params = golf_shader_get_fs_uniform(shader, "fs_params");
            golf_shader_uniform_set_float(fs_params, "t", game->t);
            sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &(sg_range) { fs_params->data, fs_params->size });

            sg_draw(0, model->positions.length, 1);
        }

        // Draw the water ripples
        {
            golf_shader_t *shader = golf_data_get_shader("data/shaders/water_ripple.glsl");
            golf_shader_pipeline_t *pipeline = golf_shader_get_pipeline(shader, "water_ripple");
            sg_apply_pipeline(pipeline->sg_pipeline);
            for (int i = 0; i < MAX_NUM_WATER_RIPPLES; i++) {
                if (game->water_ripples[i].t0 == FLT_MAX) {
                    continue;
                }

                float dt = game->t - game->water_ripples[i].t0;
                vec4 color = game->water_ripples[i].color;
                vec3 pos = game->water_ripples[i].pos;

                golf_model_t *model = golf_data_get_model("data/models/ui_square.obj");
                golf_texture_t *texture = golf_data_get_texture("data/textures/water_noise_3.png");
                sg_bindings bindings = {
                    .vertex_buffers[0] = model->sg_positions_buf,
                    .vertex_buffers[1] = model->sg_texcoords_buf,
                    .fs_images[0] = texture->sg_image,
                };
                sg_apply_bindings(&bindings);

                mat4 mvp_mat = mat4_multiply_n(4, 
                        graphics->proj_view_mat,
                        mat4_translation(pos),
                        mat4_scale(V3(0.4f, 0.4f, 0.4f)),
                        mat4_rotation_x(-0.5f * MF_PI));
                golf_shader_uniform_t *vs_params = golf_shader_get_vs_uniform(shader, "vs_params");
                golf_shader_uniform_set_mat4(vs_params, "mvp_mat", mat4_transpose(mvp_mat));
                sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &(sg_range) { vs_params->data, vs_params->size });

                golf_shader_uniform_t *fs_params = golf_shader_get_fs_uniform(shader, "fs_params");
                golf_shader_uniform_set_float(fs_params, "t", dt);
                golf_shader_uniform_set_vec4(fs_params, "uniform_color", color);
                sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &(sg_range) { fs_params->data, fs_params->size });

                sg_draw(0, model->positions.length, 1);
            }
        }

        sg_end_pass();
    }

    {
        sg_pass_action aim_line_pass_action = {
            .colors[0] = { .action = SG_ACTION_DONTCARE },
            .depth = {
                .action = SG_ACTION_DONTCARE,
            },
            .stencil = {
                .action = SG_ACTION_DONTCARE,
            },
        };
        sg_begin_pass(draw.game_draw_pass, &aim_line_pass_action);

        // Draw the aim line
        if (game->state == GOLF_GAME_STATE_AIMING && game->aim_line.power > 0) {
            golf_shader_t *shader = golf_data_get_shader("data/shaders/aim_line.glsl");
            golf_shader_pipeline_t *pipeline = golf_shader_get_pipeline(shader, "aim_line");
            sg_apply_pipeline(pipeline->sg_pipeline);

            golf_model_t *square = golf_data_get_model("data/models/ui_square.obj");
            golf_texture_t *arrow_texture = golf_data_get_texture("data/textures/arrow.png");
            sg_bindings bindings = {
                .vertex_buffers[0] = square->sg_positions_buf,
                .vertex_buffers[1] = square->sg_texcoords_buf,
                .fs_images[0] = arrow_texture->sg_image,
            };
            sg_apply_bindings(&bindings);

            float total_length = 0;
            for (int i = 0; i < game->aim_line.num_points - 1; i++) {
                vec3 p0 = game->aim_line.points[i];
                vec3 p1 = game->aim_line.points[i + 1];
                total_length += vec3_distance(p1, p0);
            }

            vec2 offset = game->aim_line.offset;
            float len0 = 0;
            float len1 = 0;
            for (int i = 0; i < game->aim_line.num_points - 1; i++) {
                vec3 p0 = game->aim_line.points[i];
                vec3 p1 = game->aim_line.points[i + 1];
                vec3 dir = vec3_normalize(vec3_sub(p1, p0));
                float len = vec3_distance(p1, p0);
                vec2 dir2 = vec2_normalize(V2(dir.x, dir.z));
                len0 = len1;
                len1 += len;

                float z_rotation = asinf(dir.y);
                float y_rotation = acosf(dir2.x);
                if (dir2.y > 0) y_rotation *= -1;
                mat4 model_mat = mat4_multiply_n(6,
                        mat4_translation(p0),
                        mat4_rotation_y(y_rotation),
                        mat4_rotation_z(z_rotation),
                        mat4_scale(V3(0.5f * len, 1.0f, 0.1f)),
                        mat4_translation(V3(1, 0, 0)),
                        mat4_rotation_x(0.5f * MF_PI));

                golf_shader_uniform_t *vs_uniform = golf_shader_get_vs_uniform(shader, "aim_line_vs_params");
                golf_shader_uniform_set_mat4(vs_uniform, "mvp_mat", mat4_transpose(mat4_multiply(graphics->proj_view_mat, model_mat)));
                sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &(sg_range) { vs_uniform->data, vs_uniform->size });

                golf_shader_uniform_t *fs_uniform = golf_shader_get_fs_uniform(shader, "aim_line_fs_params");
                golf_shader_uniform_set_vec4(fs_uniform, "color", V4(1, 1, 1, 1));
                golf_shader_uniform_set_vec2(fs_uniform, "texture_coord_offset", offset);
                golf_shader_uniform_set_vec2(fs_uniform, "texture_coord_scale", V2(4 * len, 1));
                golf_shader_uniform_set_float(fs_uniform, "length0", len0);
                golf_shader_uniform_set_float(fs_uniform, "length1", len1);
                golf_shader_uniform_set_float(fs_uniform, "total_length", total_length);
                sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &(sg_range) { fs_uniform->data, fs_uniform->size });

                sg_draw(0, square->positions.length, 1);

                offset.x += 4.0f * len;
            }
        }

        sg_end_pass();
    }

    {
        sg_pass_action hole_action1 = {
            .colors[0] = { .action = SG_ACTION_DONTCARE },
            .depth = {
                .action = SG_ACTION_DONTCARE,
            },
            .stencil = {
                .action = SG_ACTION_CLEAR,
            },
        };
        sg_begin_pass(draw.game_draw_pass, &hole_action1);

        // Draw first pass for the hole 
        {
            golf_shader_t *shader = golf_data_get_shader("data/shaders/pass_through.glsl");
            golf_shader_pipeline_t *pipeline = golf_shader_get_pipeline(shader, "hole_pass_1");
            sg_apply_pipeline(pipeline->sg_pipeline);
            for (int i = 0; i < level->entities.length; i++) {
                golf_entity_t *entity = &level->entities.data[i];
                if (!entity->active) continue;

                switch (entity->type) {
                    case MODEL_ENTITY:
                    case BALL_START_ENTITY:
                    case GEO_ENTITY:
                    case GROUP_ENTITY:
                    case WATER_ENTITY:
                        break;
                    case HOLE_ENTITY: {
                        golf_transform_t transform = entity->hole.transform;
                        transform.position.y += 0.001f;
                        mat4 model_mat = golf_transform_get_model_mat(transform);
                        golf_model_t *model = golf_data_get_model("data/models/hole-cover.obj");

                        golf_shader_uniform_t *vs_uniform = golf_shader_get_vs_uniform(shader, "pass_through_vs_params");
                        golf_shader_uniform_set_mat4(vs_uniform, "mvp_mat", mat4_transpose(mat4_multiply(graphics->proj_view_mat, model_mat)));
                        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &(sg_range) { vs_uniform->data, vs_uniform->size });

                        sg_bindings bindings = {
                            .vertex_buffers[0] = model->sg_positions_buf,
                        };
                        sg_apply_bindings(&bindings);

                        sg_draw(0, model->positions.length, 1);

                        break;
                    }
                }
            }
        }

        sg_end_pass();
    }

    {
        sg_pass_action hole_action2 = {
            .colors[0] = { .action = SG_ACTION_DONTCARE },
            .depth = {
                .action = SG_ACTION_CLEAR,
                .value = 1.0f,
            },
            .stencil = {
                .action = SG_ACTION_DONTCARE,
            },
        };
        sg_begin_pass(draw.game_draw_pass, &hole_action2);

        // Draw second pass for the hole
        {
            golf_shader_t *shader = golf_data_get_shader("data/shaders/texture_material.glsl");
            golf_shader_pipeline_t *pipeline = golf_shader_get_pipeline(shader, "hole_pass_2");
            sg_apply_pipeline(pipeline->sg_pipeline);
            for (int i = 0; i < level->entities.length; i++) {
                golf_entity_t *entity = &level->entities.data[i];
                if (!entity->active) continue;

                switch (entity->type) {
                    case MODEL_ENTITY:
                    case BALL_START_ENTITY:
                    case GEO_ENTITY:
                    case GROUP_ENTITY:
                    case WATER_ENTITY:
                        break;
                    case HOLE_ENTITY: {
                        golf_transform_t transform = entity->hole.transform;
                        transform.position.y += 0.001f;
                        mat4 model_mat = golf_transform_get_model_mat(transform);
                        golf_model_t *model = golf_data_get_model("data/models/hole.obj");
                        golf_texture_t *texture = golf_data_get_texture("data/textures/hole_lightmap.png");

                        golf_shader_uniform_t *vs_uniform = golf_shader_get_vs_uniform(shader, "texture_material_vs_params");
                        golf_shader_uniform_set_mat4(vs_uniform, "proj_view_mat", mat4_transpose(graphics->proj_view_mat));
                        golf_shader_uniform_set_mat4(vs_uniform, "model_mat", mat4_transpose(model_mat));
                        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &(sg_range) { vs_uniform->data, vs_uniform->size });

                        sg_bindings bindings = {
                            .vertex_buffers[0] = model->sg_positions_buf,
                            .vertex_buffers[1] = model->sg_texcoords_buf,
                            .vertex_buffers[2] = model->sg_normals_buf,
                            .fs_images[0] = texture->sg_image,
                        };
                        sg_apply_bindings(&bindings);

                        sg_draw(0, model->positions.length, 1);
                        break;
                    }
                }
            }
        }

        sg_end_pass();
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

    {
        golf_shader_t *shader = golf_data_get_shader("data/shaders/ui.glsl");
        golf_shader_pipeline_t *pipeline = golf_shader_get_pipeline(shader, "ui");
        sg_apply_pipeline(pipeline->sg_pipeline);

        for (int i = 0; i < ui->draw_entities.length; i++) {
            golf_ui_draw_entity_t draw = ui->draw_entities.data[i];
            switch (draw.type) {
                case GOLF_UI_DRAW_TEXTURE: {
                    vec3 translate = V3(draw.pos.x, draw.pos.y, 0);
                    vec3 scale = V3(0.5f * draw.size.x, 0.5f * draw.size.y, 1);
                    float angle = draw.angle;

                    golf_model_t *square = golf_data_get_model("data/models/ui_square.obj");
                    sg_bindings bindings = {
                        .vertex_buffers[0] = square->sg_positions_buf,
                        .vertex_buffers[1] = square->sg_texcoords_buf,
                        .fs_images[0] = draw.image,
                    };
                    sg_apply_bindings(&bindings);

                    mat4 mvp_mat = mat4_transpose(mat4_multiply_n(4,
                                ui_proj_mat,
                                mat4_translation(translate),
                                mat4_rotation_z(angle),
                                mat4_scale(scale)));

                    golf_shader_uniform_t *vs_uniform = golf_shader_get_vs_uniform(shader, "ui_vs_params");
                    golf_shader_uniform_set_mat4(vs_uniform, "mvp_mat", mvp_mat);
                    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &(sg_range) { vs_uniform->data, vs_uniform->size } );

                    golf_shader_uniform_t *fs_uniform = golf_shader_get_fs_uniform(shader, "ui_fs_params");
                    golf_shader_uniform_set_float(fs_uniform, "tex_x", draw.uv0.x);
                    golf_shader_uniform_set_float(fs_uniform, "tex_y", draw.uv0.y);
                    golf_shader_uniform_set_float(fs_uniform, "tex_dx", draw.uv1.x - draw.uv0.x);
                    golf_shader_uniform_set_float(fs_uniform, "tex_dy", draw.uv1.y - draw.uv0.y);
                    golf_shader_uniform_set_float(fs_uniform, "is_font", draw.is_font);
                    golf_shader_uniform_set_vec4(fs_uniform, "color", draw.overlay_color);
                    golf_shader_uniform_set_float(fs_uniform, "alpha", draw.alpha);
                    sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &(sg_range) { fs_uniform->data, fs_uniform->size });

                    sg_draw(0, square->positions.length, 1);
                    break;
                }
                case GOLF_UI_DRAW_APPLY_VIEWPORT: {
                    vec2 pos = draw.pos;
                    vec2 size = draw.size;
                    pos.x -= 0.5f * size.x;
                    pos.y -= 0.5f * size.y;

                    sg_apply_scissor_rectf(pos.x, pos.y, size.x, size.y, true);
                    break;
                }
                case GOLF_UI_DRAW_UNDO_APPLY_VIEWPORT: {
                    sg_apply_scissor_rectf(graphics->viewport_pos.x, graphics->viewport_pos.y, 
                            graphics->viewport_size.x, graphics->viewport_size.y, true);
                    break;
                }
            }
        }
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
    golf = golf_get();
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

    if (golf->state == GOLF_STATE_MAIN_MENU || golf->state == GOLF_STATE_IN_GAME) {
        _draw_game();

        {
            sg_pass_action action = {
                .colors[0] = {
                    .action = SG_ACTION_DONTCARE,
                },
            };

            sg_begin_default_pass(&action, (int)graphics->viewport_size.x, (int)graphics->viewport_size.y);
            sg_apply_viewportf(graphics->viewport_pos.x, graphics->viewport_pos.y, 
                    graphics->viewport_size.x, graphics->viewport_size.y, true);

            golf_shader_t *shader = golf_data_get_shader("data/shaders/fxaa.glsl");
            golf_shader_pipeline_t *pipeline = golf_shader_get_pipeline(shader, "fxaa");

            sg_apply_pipeline(pipeline->sg_pipeline);
            golf_model_t *square = golf_data_get_model("data/models/render_image_square.obj");
            sg_bindings bindings = {
                .vertex_buffers[0] = square->sg_positions_buf,
                .vertex_buffers[1] = square->sg_texcoords_buf,
                .fs_images[0] = draw.game_draw_pass_image,
            };
            sg_apply_bindings(&bindings);
            sg_draw(0, square->positions.length, 1);
            sg_end_pass();
        }
    }


    _draw_ui();
}
