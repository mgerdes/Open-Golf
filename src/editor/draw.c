#include "editor/draw.h"

#include "editor/editor.h"
#include "common/data.h"
#include "common/graphics.h"

#include "golf/shaders/diffuse_color_material.glsl.h"
#include "golf/shaders/environment_material.glsl.h"
#include "golf/shaders/pass_through.glsl.h"
#include "golf/shaders/solid_color_material.glsl.h"
#include "golf/shaders/texture_material.glsl.h"
#include "golf/shaders/ui.glsl.h"
#include "golf/shaders/render_image.glsl.h"

static void _golf_renderer_draw_solid_color_material(golf_model_t *model, int start, int count, mat4 model_mat, golf_material_t material) {
    golf_graphics_t *graphics = golf_graphics_get();

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

void golf_editor_draw(void) {
    /*
    golf_editor_t *editor = golf_editor_get();
    golf_graphics_t *graphics = golf_graphics_get();
    golf_config_t *editor_cfg = golf_data_get_config("data/config/editor.cfg");

    sg_pass_action action = {
        .colors[0] = {
            .action = SG_ACTION_CLEAR,
            .value = { 0.529f, 0.808f, 0.922f, 1.0f },
        },
    };
    sg_begin_pass(graphics->render_pass, &action);

    //_draw_level(editor->level, editor->graphics->gi_on);

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
    */
}
