#include "editor/draw.h"

#include "editor/editor.h"
#include "common/data.h"
#include "common/graphics.h"
#include "common/log.h"

static golf_graphics_t *graphics = NULL;
static golf_editor_t *editor = NULL;

void golf_draw_init(void) {
    graphics = golf_graphics_get();
    editor = golf_editor_get();
}

static void _golf_renderer_draw_environment_material(golf_model_t *model, int start, int count, mat4 model_mat, golf_material_t material, golf_lightmap_image_t *lightmap_image, golf_lightmap_section_t *lightmap_section, float uv_scale, bool gi_on) {
    golf_shader_t *shader = golf_data_get_shader("data/shaders/environment_material.glsl");
    golf_shader_pipeline_t *pipeline = golf_shader_get_pipeline(shader, "environment_material");
    sg_apply_pipeline(pipeline->sg_pipeline);

    golf_shader_uniform_t *vs_uniform = golf_shader_get_vs_uniform(shader, "environment_material_vs_params");
    golf_shader_uniform_set_mat4(vs_uniform, "proj_view_mat", mat4_transpose(graphics->proj_view_mat));
    golf_shader_uniform_set_mat4(vs_uniform, "model_mat", mat4_transpose(model_mat));
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &(sg_range) { vs_uniform->data, vs_uniform->size });

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

    golf_shader_uniform_t *fs_uniform = golf_shader_get_fs_uniform(shader, "environment_material_fs_params");
    golf_shader_uniform_set_vec4(fs_uniform, "ball_position", V4(0, 0, 0, 0));
    golf_shader_uniform_set_float(fs_uniform, "lightmap_texture_a", lightmap_t);
    golf_shader_uniform_set_float(fs_uniform, "uv_scale", uv_scale);
    sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &(sg_range) { fs_uniform->data, fs_uniform->size });

    sg_bindings bindings = {
        .vertex_buffers[0] = model->sg_positions_buf,
        .vertex_buffers[1] = model->sg_texcoords_buf,
        .vertex_buffers[2] = model->sg_normals_buf,
        .vertex_buffers[3] = lightmap_section->sg_uvs_buf,
        .fs_images[0] = material.texture->sg_image,
        .fs_images[1] = lightmap_image->sg_image[sample0],
        .fs_images[2] = lightmap_image->sg_image[sample1],
    };
    if (!gi_on) {
        golf_texture_t *white = golf_data_get_texture("data/textures/colors/white.png");
        bindings.fs_images[1] = white->sg_image;
        bindings.fs_images[2] = white->sg_image;
    }
    sg_apply_bindings(&bindings);

    sg_draw(start, count, 1);
}

static void _golf_renderer_draw_with_material(golf_model_t *model, int start, int count, mat4 model_mat, golf_material_t material) {
    switch (material.type) {
        case GOLF_MATERIAL_TEXTURE: {
            golf_shader_t *shader = golf_data_get_shader("data/shaders/texture_material.glsl");
            golf_shader_pipeline_t *pipeline = golf_shader_get_pipeline(shader, "texture_material");
            sg_apply_pipeline(pipeline->sg_pipeline);

            golf_shader_uniform_t *vs_uniforms = golf_shader_get_vs_uniform(shader, "texture_material_vs_params");
            golf_shader_uniform_set_mat4(vs_uniforms, "proj_view_mat", mat4_transpose(graphics->proj_view_mat));
            golf_shader_uniform_set_mat4(vs_uniforms, "model_mat", mat4_transpose(model_mat));
            sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &(sg_range) { vs_uniforms->data, vs_uniforms->size });

            sg_bindings bindings = {
                .vertex_buffers[0] = model->sg_positions_buf,
                .vertex_buffers[1] = model->sg_texcoords_buf,
                .vertex_buffers[2] = model->sg_normals_buf,
                .fs_images[0] = material.texture->sg_image,
            };
            sg_apply_bindings(&bindings);

            sg_draw(start, count, 1);
            break;
        }
        case GOLF_MATERIAL_COLOR: {
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
    golf_shader_t *shader = golf_data_get_shader("data/shaders/solid_color_material.glsl");
    golf_shader_pipeline_t *pipeline = golf_shader_get_pipeline(shader, "solid_color_material");
    sg_apply_pipeline(pipeline->sg_pipeline);

    golf_shader_uniform_t *vs_uniform = golf_shader_get_vs_uniform(shader, "solid_color_material_vs_params");
    golf_shader_uniform_set_mat4(vs_uniform, "mvp_mat", mat4_transpose(mat4_multiply(graphics->proj_view_mat, model_mat)));
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &(sg_range) { vs_uniform->data, vs_uniform->size });

    golf_shader_uniform_t *fs_uniform = golf_shader_get_fs_uniform(shader, "solid_color_material_fs_params");
    golf_shader_uniform_set_vec4(fs_uniform, "color", material.color);
    sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &(sg_range) { fs_uniform->data, fs_uniform->size });

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
        if (!entity->active || entity->type == HOLE_ENTITY || entity->type == BALL_START_ENTITY || entity->type == WATER_ENTITY) continue;

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
                    _golf_renderer_draw_with_material(model, group.start_vertex, group.vertex_count, model_mat, material);
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

                    _golf_renderer_draw_environment_material(model, group.start_vertex, group.vertex_count, model_mat, material, &lightmap_image, lightmap_section, uv_scale, true);
                    break;
                }
            }
        }
    }

    {
        golf_shader_t *shader = golf_data_get_shader("data/shaders/editor_water.glsl");
        golf_shader_pipeline_t *pipeline = golf_shader_get_pipeline(shader, "editor_water");
        sg_apply_pipeline(pipeline->sg_pipeline);
        for (int i = 0; i < level->entities.length; i++) {
            golf_entity_t *entity = &level->entities.data[i];
            if (entity->type != WATER_ENTITY) continue;

            golf_model_t *model = golf_entity_get_model(entity);
            golf_transform_t world_transform = golf_entity_get_world_transform(level, entity);
            mat4 model_mat = golf_transform_get_model_mat(world_transform);
            golf_texture_t *lightmap_tex = golf_data_get_texture("data/textures/colors/white.png");
            golf_texture_t *noise_tex0 = golf_data_get_texture("data/textures/water_noise_1.png");
            golf_texture_t *noise_tex1 = golf_data_get_texture("data/textures/water_noise_2.png");
            golf_lightmap_section_t *lightmap_section = golf_entity_get_lightmap_section(entity);

            sg_bindings bindings = {
                .vertex_buffers[0] = model->sg_positions_buf,
                .vertex_buffers[1] = model->sg_texcoords_buf,
                .vertex_buffers[2] = lightmap_section->sg_uvs_buf,
                .fs_images[0] = lightmap_tex->sg_image,
                .fs_images[1] = noise_tex0->sg_image,
                .fs_images[2] = noise_tex1->sg_image,
            };
            sg_apply_bindings(&bindings);

            golf_shader_uniform_t *vs_uniform = golf_shader_get_vs_uniform(shader, "editor_water_vs_params");
            golf_shader_uniform_set_mat4(vs_uniform, "model_mat", mat4_transpose(model_mat));
            golf_shader_uniform_set_mat4(vs_uniform, "proj_view_mat", mat4_transpose(graphics->proj_view_mat));
            sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &(sg_range) { vs_uniform->data, vs_uniform->size });

            golf_shader_uniform_t *fs_uniform = golf_shader_get_fs_uniform(shader, "editor_water_fs_params");
            golf_shader_uniform_set_float(fs_uniform, "draw_type", 0);
            golf_shader_uniform_set_float(fs_uniform, "t", editor->t);
            sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &(sg_range) { fs_uniform->data, fs_uniform->size });

            sg_draw(0, model->positions.length, 1);
        }
    }

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
