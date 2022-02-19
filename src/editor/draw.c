#include "editor/draw.h"

#include "editor/editor.h"
#include "common/data.h"
#include "common/graphics.h"
#include "common/log.h"

#include "golf/shaders/diffuse_color_material.glsl.h"
#include "golf/shaders/environment_material.glsl.h"
#include "golf/shaders/pass_through.glsl.h"
#include "golf/shaders/solid_color_material.glsl.h"
#include "golf/shaders/texture_material.glsl.h"
#include "golf/shaders/ui.glsl.h"
#include "golf/shaders/render_image.glsl.h"

static golf_graphics_t *graphics = NULL;
static golf_editor_t *editor = NULL;

void golf_draw_init(void) {
    graphics = golf_graphics_get();
    editor = golf_editor_get();
}

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
    if (lightmap_image->time_length > 0 && lightmap_image->num_samples > 1) {
        float t = fmodf(editor->t, lightmap_image->time_length) / lightmap_image->time_length;
        if (lightmap_image->repeats) {
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

static void _golf_renderer_draw_solid_color_material(golf_model_t *model, int start, int count, mat4 model_mat, golf_material_t material) {
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
}

static void _draw_level(void) {
    golf_level_t *level = editor->level;

    for (int i = 0; i < level->entities.length; i++) {
        golf_entity_t *entity = &level->entities.data[i];
        if (!entity->active || entity->type == HOLE_ENTITY || entity->type == BALL_START_ENTITY) continue;

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
            golf_transform_t moved_transform = golf_transform_apply_movement(world_transform, *movement, editor->t);
            model_mat = golf_transform_get_model_mat(moved_transform);
        }
        else {
            model_mat = golf_transform_get_model_mat(world_transform);
        }

        for (int i = 0; i < model->groups.length; i++) {
            golf_model_group_t group = model->groups.data[i];
            golf_material_t material;
            if (!golf_level_get_material(level, group.material_name, &material)) {
                golf_log_warning("Could not find material %s", group.material_name);
                material = golf_material_texture("", 0, 0, 0, "data/textures/fallback.png");
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
                golf_material_t material = golf_material_texture("", 0, 0, 0, "data/textures/hole_lightmap.png");
                _golf_renderer_draw_with_material(model, 0, model->positions.length, model_mat, material, 1);
                break;
            }
        }
    }
}

void golf_editor_draw(void) {
    golf_config_t *editor_cfg = golf_data_get_config("data/config/editor.cfg");

    sg_pass_action action = {
        .colors[0] = {
            .action = SG_ACTION_CLEAR,
            .value = { 0, 0, 0, 1 },
        },
    };
    sg_begin_default_pass(&action, (int)graphics->viewport_size.x, (int)graphics->viewport_size.y);

    _draw_level();

    if (editor->in_edit_mode) {
        golf_geo_t *geo = editor->edit_mode.geo;
        mat4 model_mat = golf_transform_get_model_mat(editor->edit_mode.transform);

        sg_apply_pipeline(graphics->solid_color_material_pipeline);
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
            golf_material_t material = golf_material_color("", 0, 0, 0, V4(color.x, color.y, color.z, 1));
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
                golf_material_t material = golf_material_color("", 0, 0, 0, V4(color.x, color.y, color.z, 1));
                _golf_renderer_draw_solid_color_material(model, start, count, line_model_mat, material);
            }

            golf_edit_mode_entity_t entity = golf_edit_mode_entity_face(i);
            if (golf_editor_is_edit_entity_hovered(entity)) {
                golf_model_t *model = &geo->model;
                int start_vertex = face.start_vertex_in_model;
                int count = 3 * (n - 2);
                vec3 color = CFG_VEC3(editor_cfg, "edit_mode_hovered_color");
                float alpha = CFG_NUM(editor_cfg, "edit_mode_hovered_face_alpha");
                golf_material_t material = golf_material_color("", 0, 0, 0, V4(color.x, color.y, color.z, alpha));
                _golf_renderer_draw_solid_color_material(model, start_vertex, count, model_mat, material);
            }
            else if (golf_editor_is_edit_entity_selected(entity)) {
                golf_model_t *model = &geo->model;
                int start_vertex = face.start_vertex_in_model;
                int count = 3 * (n - 2);
                vec3 color = CFG_VEC3(editor_cfg, "edit_mode_selected_color");
                float alpha = CFG_NUM(editor_cfg, "edit_mode_selected_face_alpha");
                golf_material_t material = golf_material_color("", 0, 0, 0, V4(color.x, color.y, color.z, alpha));
                _golf_renderer_draw_solid_color_material(model, start_vertex, count, model_mat, material);
            }
        }
    }

    sg_end_pass();
}
