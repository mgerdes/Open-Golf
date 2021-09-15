#define _CRT_SECURE_NO_WARNINGS

#include "game_editor.h"

#include <float.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "profiler.h"
#ifdef HOLE_EDITOR
#include "s7.h"
#endif

#include "mlog.h"

static void entity_array_sort_idxs(struct editor_entity_array *entity_array) {
    for (int i = 0; i < entity_array->length; i++) {
        int smallest_idx = i;
        for (int j = i + 1; j < entity_array->length; j++) {
            if (entity_array->data[j].idx < entity_array->data[smallest_idx].idx) {
                smallest_idx = j;
            }
        }
        array_swap(entity_array, smallest_idx, i);
    }
}

struct editor_entity make_editor_entity(enum editor_entity_type type, int idx) {
    struct editor_entity editor_entity; 
    editor_entity.type = type;
    editor_entity.idx = idx;
    return editor_entity;
}

bool editor_entity_array_contains_entity(struct editor_entity_array *entity_array,
        struct editor_entity editor_entity) {
    struct editor_entity entity;
    int i;
    array_foreach(entity_array, entity, i) {
        if (entity.type == editor_entity.type && entity.idx == editor_entity.idx) {
            return true;
        }
    }
    return false;
}

static bool is_selectable_array_equal(struct editor_entity_array *selected_array,
        struct editor_entity_array *hovered_array) {
    for (int i = 0; i < hovered_array->length; i++) {
        if (!editor_entity_array_contains_entity(selected_array, hovered_array->data[i])) {
            return false;
        }
    }
    return true;
}

static void object_modifier_init(struct object_modifier *om) {
    memset(om, 0, sizeof(struct object_modifier));
    om->mode = OBJECT_MODIFIER_MODE_TRANSLATE;
    om->is_active = false;
    om->is_hovered = false;
    om->is_using = false;
    om->was_used = false;
    om->set_start_positions = true;
    om->model_mat = mat4_identity();

    om->translate_mode.cone_dist = 0.1f;
    om->translate_mode.line_radius = 0.01f;
    om->translate_mode.cone_scale = 0.02f;
    om->translate_mode.cone_hovered[0] = false;
    om->translate_mode.cone_hovered[1] = false;
    om->translate_mode.cone_hovered[2] = false;
    om->translate_mode.clamp = 0.0f;
    om->translate_mode.start_delta_set = false;
    om->translate_mode.start_delta = 0.0f;

    om->rotate_mode.line_radius = 0.01f;
    om->rotate_mode.circle_radius = 0.1f;
    om->rotate_mode.circle_color[0] = V3(1.0f, 0.0f, 0.0f);
    om->rotate_mode.circle_color[1] = V3(0.0f, 1.0f, 0.0f);
    om->rotate_mode.circle_color[2] = V3(0.0f, 0.0f, 1.0f);
    om->rotate_mode.clamp = 0.0f;
}

static void object_modifier_reset(struct object_modifier *om) {
    om->mode = OBJECT_MODIFIER_MODE_TRANSLATE;
    om->is_active = false;
    om->is_using = false;
    om->was_used = false;
    om->model_mat = mat4_identity();
}

static void object_modifier_activate(struct object_modifier *om, vec3 pos, mat4 model_mat) {
    om->is_active = true;
    om->was_used = false;
    om->model_mat = model_mat;
    om->pos = pos;
    if (om->mode == OBJECT_MODIFIER_MODE_TRANSLATE) {
        om->translate_mode.start_pos = pos;
        om->translate_mode.delta_pos = V3(0.0f, 0.0f, 0.0f);
    } 
    else if (om->mode == OBJECT_MODIFIER_MODE_ROTATE) {
        om->rotate_mode.delta_theta = 0.0f;
    }
}

static void object_modifier_update(struct object_modifier *om, float dt, struct button_inputs button_inputs) {
    om->is_hovered = false;
    if (!om->is_active) {
        return;
    }

    vec3 *pos = &om->pos;

    if (om->mode == OBJECT_MODIFIER_MODE_TRANSLATE) {
        vec3 axis[3] = { 
            V3(1.0f, 0.0f, 0.0f),
            V3(0.0f, 1.0f, 0.0f),
            V3(0.0f, 0.0f, 1.0f),
        };
        for (int i = 0; i < 3; i++) {
            axis[i] = vec3_apply_mat4(axis[i], 0.0f, om->model_mat);
        }
        vec3 *start_pos = &om->translate_mode.start_pos;
        vec3 *delta_pos = &om->translate_mode.delta_pos;
        float *cone_dist = &om->translate_mode.cone_dist;
        float *line_radius = &om->translate_mode.line_radius;
        float *cone_scale = &om->translate_mode.cone_scale;
        bool *cone_hovered = om->translate_mode.cone_hovered;
        cone_hovered[0] = false;
        cone_hovered[1] = false;
        cone_hovered[2] = false;

        struct model *cone_model = asset_store_get_model("cone");
        vec3 *cones_triangle_points = malloc(sizeof(vec3) * 3 * cone_model->num_points);
        for (int i = 0; i < 3; i++) {
            vec3 cone_pos = vec3_add(*pos, vec3_scale(axis[i], *cone_dist));
            mat4 transform = mat4_multiply_n(3,
                    mat4_translation(cone_pos),
                    mat4_scale(V3(*cone_scale, *cone_scale, *cone_scale)),
                    mat4_from_quat(quat_between_vectors(V3(0.0f, 1.0f, 0.0f), axis[i])));
            for (int j = 0; j < cone_model->num_points; j++) {
                vec3 p = cone_model->positions[j];
                p = vec3_apply_mat4(p, 1.0f, transform);
                cones_triangle_points[i * cone_model->num_points + j] = p;
            }
        }
        {
            float t;
            int idx;
            bool hovered_on_mouse_down = 
                ray_intersect_triangles(button_inputs.mouse_down_ray_orig, button_inputs.mouse_down_ray_dir,
                    cones_triangle_points, 3 * cone_model->num_points, mat4_identity(), &t, &idx);
            if (ray_intersect_triangles(button_inputs.mouse_ray_orig, button_inputs.mouse_ray_dir,
                    cones_triangle_points, 3 * cone_model->num_points, mat4_identity(), &t, &idx)) {
                idx = (3 * idx) / cone_model->num_points;
                om->is_hovered = true;
                om->translate_mode.cone_hovered[idx] = true;
                
                if (button_inputs.mouse_down[SAPP_MOUSEBUTTON_LEFT] && !om->is_using && hovered_on_mouse_down) {
                    om->translate_mode.start_delta_set = false;
                    om->is_using = true;
                    om->translate_mode.axis = idx;
                    *start_pos = *pos;
                    *delta_pos = V3(0.0f, 0.0f, 0.0f);
                }
            }
        }
        free(cones_triangle_points);

        if (!button_inputs.mouse_down[SAPP_MOUSEBUTTON_LEFT] && om->is_using) {
            om->was_used = true;
            om->is_using = false;
        }

        if (om->is_using) {
            int axis_idx = om->translate_mode.axis;
            assert(axis_idx >= 0 && axis_idx <= 2);
            vec3 p1 = button_inputs.mouse_ray_orig;
            vec3 q1 = vec3_add(button_inputs.mouse_ray_orig, button_inputs.mouse_ray_dir);
            vec3 p2 = *pos;
            vec3 q2 = vec3_add(*pos, axis[axis_idx]); 
            float s, t;
            vec3 c1, c2;
            closest_point_ray_ray(p1, q1, p2, q2, &s, &t, &c1, &c2);
            if (!om->translate_mode.start_delta_set) {
                om->translate_mode.start_delta_set = true;
                om->translate_mode.start_delta = t;
            }
            *pos = vec3_add(c2, vec3_scale(axis[axis_idx], -om->translate_mode.start_delta));
            *delta_pos = vec3_sub(*pos, *start_pos);
        }

        float cam_dist = vec3_distance(*pos, button_inputs.mouse_ray_orig);
        *cone_scale = 0.02f * cam_dist;
        *cone_dist = 0.1f * cam_dist;
        *line_radius = 0.01f * cam_dist;
    }
    else if (om->mode == OBJECT_MODIFIER_MODE_ROTATE) {
        vec3 axis[3] = { 
            V3(1.0f, 0.0f, 0.0f),
            V3(0.0f, 1.0f, 0.0f),
            V3(0.0f, 0.0f, 1.0f) 
        };
        float line_radius = om->rotate_mode.line_radius;
        float circle_radius = om->rotate_mode.circle_radius;

        for (int i = 0; i < 3; i++) {
            vec3 point = button_inputs.mouse_ray_orig;
            vec3 circle_center = *pos;
            vec3 circle_plane = axis[i];
            vec3 closest_point = closest_point_point_circle(point, circle_center, circle_plane, circle_radius);
            vec3 v = vec3_subtract(closest_point, *pos);

            for (int j = 0; j < OM_ROTATE_MODE_RES; j++) {
                float theta0 = -0.5f * MF_PI + ((float) j / OM_ROTATE_MODE_RES) * MF_PI;
                float theta1 = -0.5f * MF_PI + ((float) (j + 1) / OM_ROTATE_MODE_RES) * MF_PI;
                mat4 rotation0 = mat4_from_quat(quat_create_from_axis_angle(axis[i], theta0));
                mat4 rotation1 = mat4_from_quat(quat_create_from_axis_angle(axis[i], theta1));
                vec3 p0 = vec3_add(*pos, vec3_apply_mat4(v, 1.0f, rotation0));
                vec3 p1 = vec3_add(*pos, vec3_apply_mat4(v, 1.0f, rotation1));
                om->rotate_mode.line_segment_p0[OM_ROTATE_MODE_RES * i + j] = p0;
                om->rotate_mode.line_segment_p1[OM_ROTATE_MODE_RES * i + j] = p1;
            }

            float cam_dist = vec3_distance(*pos, point);
            om->rotate_mode.line_radius = 0.01f * cam_dist;
            om->rotate_mode.circle_radius = 0.1f * cam_dist;
        }

        {
            vec3 ro = button_inputs.mouse_down_ray_orig;
            vec3 rd = button_inputs.mouse_down_ray_dir;
            vec3 *p0 = om->rotate_mode.line_segment_p0;
            vec3 *p1 = om->rotate_mode.line_segment_p1;
            float t;
            int idx;
            bool hovered_on_mouse_down = 
                ray_intersect_segments(ro, rd, p0, p1, line_radius, 3 * OM_ROTATE_MODE_RES, &t, &idx);
            ro = button_inputs.mouse_ray_orig;
            rd = button_inputs.mouse_ray_dir;
            if (ray_intersect_segments(ro, rd, p0, p1, line_radius, 3 * OM_ROTATE_MODE_RES, &t, &idx)) {
                om->is_hovered = true;
                if (button_inputs.mouse_down[SAPP_MOUSEBUTTON_LEFT] && !om->is_using && hovered_on_mouse_down) {
                    om->is_using = true;
                    om->rotate_mode.start_theta_set = false;
                    om->rotate_mode.axis = (int) (idx / OM_ROTATE_MODE_RES);
                }
            }
        }

        if (om->is_using) {
            int axis_idx = om->rotate_mode.axis;
            vec3 ro = button_inputs.mouse_ray_orig;
            vec3 rd = button_inputs.mouse_ray_dir;
            vec3 closest_point = closest_point_ray_circle(ro, rd, *pos, axis[axis_idx], circle_radius); 
            vec3 v = vec3_normalize(vec3_subtract(closest_point, *pos));
            float theta = 0.0f;

            if (axis_idx == 0) {
                theta = acosf(v.z);
                if (v.y < 0.0f) {
                    theta *= -1.0f;
                }
            } else if (axis_idx == 1) {
                theta = acosf(v.x);
                if (v.z < 0.0f) {
                    theta *= -1.0f;
                }
            } else if (axis_idx == 2) {
                theta = acosf(v.y);
                if (v.x < 0.0f) {
                    theta *= -1.0f;
                }
            } else {
                assert(false);
            }

            if (!om->rotate_mode.start_theta_set) {
                om->rotate_mode.start_theta_set = true;
                om->rotate_mode.start_theta = theta;
            }
            om->rotate_mode.end_theta = theta;

            if (om->rotate_mode.end_theta - om->rotate_mode.start_theta >= MF_PI) {
                om->rotate_mode.start_theta += 2.0f * MF_PI;    
            }
            if (om->rotate_mode.end_theta - om->rotate_mode.start_theta <= -MF_PI) {
                om->rotate_mode.end_theta += 2.0f * MF_PI;    
            }
            om->rotate_mode.delta_theta = om->rotate_mode.end_theta - om->rotate_mode.start_theta;

            if (!button_inputs.mouse_down[SAPP_MOUSEBUTTON_LEFT]) {
                om->was_used = true;
                om->is_using = false;
            }
        }
    }
}

#ifdef HOLE_EDITOR
static void hole_editor_push_state(struct hole_editor *ce) {
    struct hole_editor_state state;
    data_stream_init(&state.serialization);
    array_init(&state.selected_array);

    hole_serialize(ce->hole, &state.serialization, false);
    for (int i = 0; i < ce->edit_terrain_model.selected_array.length; i++) {
        array_push(&state.selected_array, ce->edit_terrain_model.selected_array.data[i]);
    }

    if (ce->history.idx < ce->history.state_array.length) {
        int start = ce->history.idx;
        int count = ce->history.state_array.length - start;
        for (int i = start; i < start + count; i++) {
            data_stream_deinit(&ce->history.state_array.data[i].serialization);
            array_deinit(&ce->history.state_array.data[i].selected_array);
        }
        array_splice(&ce->history.state_array, start, count);
    }

    if (ce->history.idx >= 100) {
        data_stream_deinit(&ce->history.state_array.data[0].serialization);
        array_deinit(&ce->history.state_array.data[0].selected_array);
        array_splice(&ce->history.state_array, 0, 1);
        array_push(&ce->history.state_array, state);
    }
    else {
        array_push(&ce->history.state_array, state);
        ce->history.idx++;
    }
}

static void hole_editor_update_model_generator_scripts(struct hole_editor *ce, bool first_time) {
    struct array_model_generator_script *scripts = &ce->scripts;
    for (int i = 0; i < scripts->length; i++) {
        struct model_generator_script *script = &scripts->data[i];
        mfile_t file = script->file;
        mfiletime_t cur_time;
        mfile_get_time(&file, &cur_time);
        if (first_time || (mfiletime_cmp(script->load_time, cur_time) < 0.0f)) {
            if (mfile_load_data(&file)) {
                ce->sc_error[0] = 0;
                script->load_time = cur_time;
                script->params.length = 0;

                s7_scheme *sc = ce->sc_state;
                if (!first_time) {
                    s7_gc_unprotect_at(sc, script->sc_env_loc); 
                }
                s7_pointer sc_env = s7_inlet(sc, s7_nil(sc));
                script->sc_env = sc_env;
                script->sc_env_loc = s7_gc_protect(sc, sc_env);

                s7_eval_c_string_with_environment(sc, file.data, sc_env);
                s7_pointer gen_fn = s7_eval_c_string_with_environment(sc, "generate", sc_env);
                s7_pointer gen_args = s7_closure_args(sc, gen_fn);
                while (!s7_is_null(sc, gen_args)) {
                    s7_pointer arg = s7_car(gen_args);
                    if (s7_is_symbol(arg)) {
                        const char *symbol_name = s7_symbol_name(arg);
                        char *param = malloc(sizeof(char) * (strlen(symbol_name) + 1));
                        strcpy(param, symbol_name);
                        array_push(&script->params, param);
                    }

                    gen_args = s7_cdr(gen_args);
                }

                for (int i = script->values.length; i < script->params.length; i++) {
                    array_push(&script->values, 0.0f);
                }
                script->values.length = script->params.length;

                mfile_free_data(&file);
            }
        }
    }
}

static struct hole_editor *sc_hole_editor = NULL; 

static s7_pointer sc_error_handler(s7_scheme *sc, s7_pointer args) {
    snprintf(sc_hole_editor->sc_error, 1023, "error: %s", s7_string(s7_car(args)));
    sc_hole_editor->sc_error[1023] = 0;
    return s7_f(sc);
}

static s7_pointer sc_terrain_model_add_point(s7_scheme *sc, s7_pointer args) {
    struct terrain_model *model = sc_hole_editor->edit_terrain_model.model;
    vec3 point; 

    for (int i = 0; i < 3; i++) {
        s7_pointer arg = s7_car(args);
        double v = s7_number_to_real(sc, arg);
        if (i == 0)         point.x = (float) v;
        else if (i == 1)    point.y = (float) v;
        else if (i == 2)    point.z = (float) v;

        args = s7_cdr(args);
    }

    terrain_model_add_point(model, point, -1);
    return s7_nil(sc);
}

static s7_pointer sc_terrain_model_add_face(s7_scheme *sc, s7_pointer args) {
    struct terrain_model *model = sc_hole_editor->edit_terrain_model.model;

    int num_points, mat_idx, smooth_normal, x, y, z, w;
    vec2 tc0, tc1, tc2, tc3;
    float texture_coord_scale, cor, friction, vel_scale;
    enum terrain_model_auto_texture auto_texture;

    for (int i = 0; i < 20; i++) {
        s7_pointer arg = s7_car(args);
        double v = s7_number_to_real(sc, arg);
        if (i == 0)         num_points = (int) round(v);
        else if (i == 1)    mat_idx = (int) round(v);
        else if (i == 2)    smooth_normal = (int) round(v);
        else if (i == 3)    x = (int) round(v);
        else if (i == 4)    y = (int) round(v);
        else if (i == 5)    z = (int) round(v);
        else if (i == 6)    w = (int) round(v);
        else if (i == 7)    tc0.x = (float) v;
        else if (i == 8)    tc0.y = (float) v;
        else if (i == 9)    tc1.x = (float) v;
        else if (i == 10)   tc1.y = (float) v;
        else if (i == 11)   tc2.x = (float) v;
        else if (i == 12)   tc2.y = (float) v;
        else if (i == 13)   tc3.x = (float) v;
        else if (i == 14)   tc3.y = (float) v;
        else if (i == 15)   texture_coord_scale = (float) v;
        else if (i == 16)   cor = (float) v;
        else if (i == 17)   friction = (float) v;
        else if (i == 18)   vel_scale = (float) v;
        else if (i == 19) {
            int vi = (int)v;
            if (vi == 1) {
                auto_texture = AUTO_TEXTURE_WOOD_OUT;
            }
            else if (vi == 2) {
                auto_texture = AUTO_TEXTURE_WOOD_IN;
            }
            else if (vi == 3) {
                auto_texture = AUTO_TEXTURE_WOOD_TOP;
            }
            else if (vi == 4) {
                auto_texture = AUTO_TEXTURE_GRASS;
            }
            else {
                auto_texture = AUTO_TEXTURE_NONE;
            }
        } 
        args = s7_cdr(args);
    }

    struct terrain_model_face face = create_terrain_model_face(num_points, mat_idx, smooth_normal,
            x, y, z, w, tc0, tc1, tc2, tc3, texture_coord_scale, cor, friction, vel_scale, auto_texture);
    if (terrain_model_add_face(model, face, -1) == -1) {
        return s7_out_of_range_error(sc, "terrain_model_add_face", 0, s7_nil(sc), "point idx out of bounds");
    }
    return s7_nil(sc);
}

static s7_pointer sc_perlin_noise(s7_scheme *sc, s7_pointer args) {
    struct texture *perlin_noise = asset_store_get_texture("perlin_noise.png");

    double px = s7_number_to_real(sc, s7_car(args));
    double py = s7_number_to_real(sc, s7_cadr(args));

    int x = (int) (px * perlin_noise->width);
    int y = (int) (py * perlin_noise->height);
    if (x < 0) x *= -1;
    if (y < 0) x *= -1;
    x = x % perlin_noise->width;
    y = y % perlin_noise->height;

    unsigned char c = perlin_noise->data[4 * (perlin_noise->width * x + y) + 1];
    float v = (float) c / 255.0f;
    return s7_make_real(sc, v);
}

static void sc_begin_hook(s7_scheme *sc, bool *all_done) {
    double seconds = stm_sec(stm_since(sc_hole_editor->sc_start_time));
    printf("%f\n", (float) seconds);
    if (seconds > 5.0) {
        *all_done = true;
    }
}

static void hole_editor_init(struct hole_editor *ce, struct renderer *renderer) {
    profiler_push_section("hole_editor_init");

    ce->open_save_as_dialog = false;
    ce->open_load_dialog = false;
    {
        ce->history.idx = 0;
        array_init(&ce->history.state_array);
    }
    {
        ce->cam.pos = V3(0.0f, 2.0f, 0.0f);
        ce->cam.azimuth_angle = 0.0f;
        ce->cam.inclination_angle = 0.5f * MF_PI;
    }
    {
        object_modifier_init(&ce->object_modifier);
        array_init(&ce->object_modifier_entities);
        array_init(&ce->object_modifier_start_positions);
        array_init(&ce->object_modifier_start_orientations);
        ce->select_box.is_open = false;
        ce->select_box.p0 = V2(0.0f, 0.0f);
        ce->select_box.p1 = V2(0.0f, 0.0f);
        array_init(&ce->selected_array);
        array_init(&ce->hovered_array);
    }
    {
        ce->file.path[0] = 0;
        mdir_init(&ce->file.holes_directory, "assets/holes", false);
        ce->hole = NULL;
    }
    {
        ce->drawing.helper_lines.active = false;
        ce->drawing.helper_lines.line_dist = 1.0f;
        ce->drawing.terrain_entities.draw_type = 1;
    }
    {
        ce->selected_entity.terrain.lightmap_width = 0;
        ce->selected_entity.terrain.lightmap_height = 0;
        mdir_init(&ce->selected_entity.environment.model_directory, "assets/models", false);
        //directory_sort_files_alphabetically(&ce->selected_entity.environment.model_directory);
    }
    {
        ce->multi_terrain_entities.is_moving = false;
    }
    {
        ce->camera_zone_entities.can_modify = false;
    }
    {
        ce->lightmap_update.do_it = false;
        ce->lightmap_update.new_num_images = 1;
        ce->lightmap_update.new_width = 256;
        ce->lightmap_update.new_height = 256;
    }
    {
        ce->edit_terrain_model.active = false;
        ce->edit_terrain_model.model_mat = mat4_identity();
        ce->edit_terrain_model.model = NULL;
        array_init(&ce->edit_terrain_model.selected_array);
        array_init(&ce->edit_terrain_model.hovered_array);
        ce->edit_terrain_model.selected_face_cor = 1.0f;
        ce->edit_terrain_model.selected_face_friction = 0.0f;
        ce->edit_terrain_model.selected_face_vel_scale = 1.0f;
        ce->edit_terrain_model.points_selectable = true;
        ce->edit_terrain_model.faces_selectable = true;
        ce->edit_terrain_model.model_filename[0] = 0;
        array_init(&ce->edit_terrain_model.copied_points);
        array_init(&ce->edit_terrain_model.copied_faces);
    }
    {
        ce->global_illumination.do_it = false;
        ce->global_illumination.num_dilates = 0;
        ce->global_illumination.num_smooths = 1;
        ce->global_illumination.num_iterations = 1;
        ce->global_illumination.reset_lightmaps = true;
        ce->global_illumination.create_uvs = true;
        ce->global_illumination.gamma = 1.0f;
    }

    {
        sc_hole_editor = ce;
        s7_scheme *sc = s7_init();
        ce->sc_state = sc;
        s7_define_function(sc, "error-handler", sc_error_handler, 1, 0, false, "our error handler");
        s7_eval_c_string(sc, "(set! (hook-functions *error-hook*)                    \n\
            (list (lambda (hook)                                 \n\
                   (error-handler                               \n\
                    (apply format #f (hook 'data)))            \n\
                   (set! (hook 'result) 'our-error))))");
        s7_define_function(sc, "terrain_model_add_point", sc_terrain_model_add_point, 3, 0, false, "");
        s7_define_function(sc, "terrain_model_add_face", sc_terrain_model_add_face, 20, 0, false, "");
        s7_define_function(sc, "perlin_noise", sc_perlin_noise, 2, 0, false, "");

        struct array_model_generator_script *scripts = &ce->scripts;
        array_init(scripts);

		mdir_t dir;
		mdir_init(&dir, "model_generator_scripts", false);
        for (int i = 0; i < dir.num_files; i++) {
            mfile_t file = dir.files[i];
            if (strcmp(file.ext, ".scm") != 0) {
                continue;
            }

            struct model_generator_script script;
            array_init(&script.params);
            array_init(&script.values);
            script.file = file;
            array_push(scripts, script);
        }
        mdir_deinit(&dir);
        hole_editor_update_model_generator_scripts(ce, true);
    }
#if defined(_WIN32)
    lightmap_generator_init();
#endif

    profiler_pop_section();
}

static void add_unique_point(struct array_int *added_points_idxs, struct array_int *new_points_idxs,
        struct array_vec3 *points, int point_idx, vec3 point, int *new_point_idx) {
    for (int i = 0; i < added_points_idxs->length; i++) {
        if (added_points_idxs->data[i] == point_idx) {
            *new_point_idx = new_points_idxs->data[i];
            return;
        }
    }

    *new_point_idx = points->length;
    array_push(added_points_idxs, point_idx);
    array_push(new_points_idxs, points->length);
    array_push(points, point);
}

static void hole_editor_update(struct game_editor *ed, float dt,
        struct button_inputs button_inputs, struct renderer *renderer) {
    profiler_push_section("hole_editor_update");
    struct hole_editor *ce = &ed->hole_editor;

#if (_WIN32)
    if (ce->global_illumination.do_it) {
        if (!lightmap_generator_is_running()) {
            ce->global_illumination.do_it = false;
            struct lightmap_generator_data *data = ce->global_illumination.data;

            for (int i = 0; i < data->entities.length; i++) {
                struct lightmap_entity *lm_entity = &data->entities.data[i];

                struct lightmap *lightmap = lm_entity->lightmap;
                float *lightmap_data = lm_entity->lightmap_data;
                int lightmap_width = lm_entity->lightmap_width;
                int lightmap_height = lm_entity->lightmap_height;
                struct array_vec2 lightmap_uvs = lm_entity->lightmap_uvs;

                lightmap->uvs.length = 0;
                for (int i = 0; i < lightmap_uvs.length; i++) {
                    array_push(&lightmap->uvs, lightmap_uvs.data[i]);
                }

                assert((lightmap_width == lightmap->width) && (lightmap_height == lightmap->height));
                for (int i = 0; i < lightmap_width * lightmap_height; i++) {
                    int val = (int) (255.0f * lightmap_data[i]);
                    if (val > 255) val = 255;
                    if (val < 0) val = 0;
                    unsigned char *data = lightmap->images.data[0].data;
                    data[i] = (unsigned char) val;
                }
            }

            for (int i = 0; i < data->multi_entities.length; i++) {
                struct lightmap_multi_entity *lm_entity = &data->multi_entities.data[i];

                {
                    struct lightmap *lightmap = lm_entity->static_lightmap;
                    int lightmap_width = lm_entity->static_lightmap_width;
                    int lightmap_height = lm_entity->static_lightmap_height;
                    struct array_vec2 lightmap_uvs = lm_entity->static_lightmap_uvs;
                    assert((lightmap_width == lightmap->width) && (lightmap_height == lightmap->height));

                    lightmap->uvs.length = 0;
                    for (int i = 0; i < lightmap_uvs.length; i++) {
                        array_push(&lightmap->uvs, lightmap_uvs.data[i]);
                    }

                    assert(lightmap->images.length == lm_entity->static_lightmap_data.length);
                    for (int ti = 0; ti < lightmap->images.length; ti++) {
                        float *lightmap_data = lm_entity->static_lightmap_data.data[ti];
                        for (int i = 0; i < lightmap_width * lightmap_height; i++) {
                            int val = (int) (255.0f * lightmap_data[i]);
                            if (val > 255) val = 255;
                            if (val < 0) val = 0;
                            unsigned char *data = lightmap->images.data[ti].data;
                            data[i] = (unsigned char) val;
                        }
                    }
                }

                {
                    struct lightmap *lightmap = lm_entity->moving_lightmap;
                    int lightmap_width = lm_entity->moving_lightmap_width;
                    int lightmap_height = lm_entity->moving_lightmap_height;
                    struct array_vec2 lightmap_uvs = lm_entity->moving_lightmap_uvs;
                    assert((lightmap_width == lightmap->width) && (lightmap_height == lightmap->height));

                    lightmap->uvs.length = 0;
                    for (int i = 0; i < lightmap_uvs.length; i++) {
                        array_push(&lightmap->uvs, lightmap_uvs.data[i]);
                    }

                    assert(lightmap->images.length == lm_entity->moving_lightmap_data.length);
                    for (int ti = 0; ti < lightmap->images.length; ti++) {
                        float *lightmap_data = lm_entity->moving_lightmap_data.data[ti];
                        for (int i = 0; i < lightmap_width * lightmap_height; i++) {
                            int val = (int) (255.0f * lightmap_data[i]);
                            if (val > 255) val = 255;
                            if (val < 0) val = 0;
                            unsigned char *data = lightmap->images.data[ti].data;
                            data[i] = (unsigned char) val;
                        }
                    }
                }
            }

            lightmap_generator_data_deinit(data);
            free(data);
        }
    }
#endif
    if (ce->lightmap_update.do_it) {
        ce->lightmap_update.do_it = false;
        struct lightmap *lightmap = ce->lightmap_update.lightmap;
        int new_num_images = ce->lightmap_update.new_num_images;
        int new_width = ce->lightmap_update.new_width;
        int new_height = ce->lightmap_update.new_height;
        lightmap_resize(lightmap, new_width, new_height, new_num_images);
    }

    {
        if (button_inputs.button_down[SAPP_KEYCODE_W]) {
            ce->cam.pos.x += 8.0f * dt * cosf(ce->cam.azimuth_angle);
            ce->cam.pos.z += 8.0f * dt * sinf(ce->cam.azimuth_angle);
        }

        if (button_inputs.button_down[SAPP_KEYCODE_S]) {
            ce->cam.pos.x -= 8.0f * dt * cosf(ce->cam.azimuth_angle);
            ce->cam.pos.z -= 8.0f * dt * sinf(ce->cam.azimuth_angle);
        }

        if (button_inputs.button_down[SAPP_KEYCODE_D]) {
            ce->cam.pos.x += 8.0f * dt * cosf(ce->cam.azimuth_angle + 0.5f * MF_PI);
            ce->cam.pos.z += 8.0f * dt * sinf(ce->cam.azimuth_angle + 0.5f * MF_PI);
        }

        if (button_inputs.button_down[SAPP_KEYCODE_A]) {
            ce->cam.pos.x -= 8.0f * dt * cosf(ce->cam.azimuth_angle + 0.5f * MF_PI);
            ce->cam.pos.z -= 8.0f * dt * sinf(ce->cam.azimuth_angle + 0.5f * MF_PI);
        }

        if (button_inputs.button_down[SAPP_KEYCODE_E] && !button_inputs.button_down[SAPP_KEYCODE_LEFT_CONTROL]) {
            ce->cam.pos.y += 8.0f * dt;
        }

        if (button_inputs.button_down[SAPP_KEYCODE_Q] && !button_inputs.button_down[SAPP_KEYCODE_LEFT_CONTROL]) {
            ce->cam.pos.y -= 8.0f * dt;
        }

        if (button_inputs.button_down[SAPP_KEYCODE_LEFT]) {
            ce->cam.azimuth_angle -= 1.0f * dt;
        }

        if (button_inputs.button_down[SAPP_KEYCODE_RIGHT]) {
            ce->cam.azimuth_angle += 1.0f * dt;
        }

        if (button_inputs.button_down[SAPP_KEYCODE_DOWN]) {
            ce->cam.inclination_angle += 1.0f * dt;
        }

        if (button_inputs.button_down[SAPP_KEYCODE_UP]) {
            ce->cam.inclination_angle -= 1.0f * dt;
        }

        if (button_inputs.mouse_down[SAPP_MOUSEBUTTON_MIDDLE]) {
            ce->cam.azimuth_angle += 0.2f * dt * button_inputs.mouse_delta.x;
            ce->cam.inclination_angle -= 0.2f * dt * button_inputs.mouse_delta.y;
        }

        renderer->cam_pos = ce->cam.pos;

        float theta = ce->cam.inclination_angle;
        float phi = ce->cam.azimuth_angle;
        renderer->cam_dir.x = sinf(theta) * cosf(phi);
        renderer->cam_dir.y = cosf(theta);
        renderer->cam_dir.z = sinf(theta) * sinf(phi);

        renderer->cam_up = V3(0.0f, 1.0f, 0.0f);
    }

    if (ce->multi_terrain_entities.is_moving) {
        bool should_move = true;
        for (int i = 0; i < ce->hole->multi_terrain_entities.length; i++) {
            struct editor_entity editor_entity = make_editor_entity(EDITOR_ENTITY_MULTI_TERRAIN_MOVING, i);
            if (editor_entity_array_contains_entity(&ce->selected_array, editor_entity) ||
                    editor_entity_array_contains_entity(&ce->hovered_array, editor_entity)) {
                should_move = false;
                break;
            }
        }
        if (should_move) {
            ed->game->t += dt;
        }
    }
    else {
        ed->game->t = 0.0f;
    }

    struct object_modifier *om = &ce->object_modifier;
    if (ce->edit_terrain_model.active) {
        struct editor_entity_array *selected_array = &ce->edit_terrain_model.selected_array;
        struct editor_entity_array *hovered_array = &ce->edit_terrain_model.hovered_array;
        mat4 model_mat = ce->edit_terrain_model.model_mat;
        struct terrain_model *model = ce->edit_terrain_model.model;

        if (!om->is_using && !ce->select_box.is_open && 
                button_inputs.mouse_down[SAPP_MOUSEBUTTON_LEFT] && 
                vec2_length(button_inputs.mouse_down_delta) >= 10.0f) {
            ce->select_box.is_open = true;
        }

        profiler_push_section("get_hovered_entity");
        if (ce->select_box.is_open) {
            vec2 p0 = button_inputs.mouse_down_pos;
            vec2 p1 = button_inputs.mouse_pos;
            if (p1.x < p0.x) {
                float temp = p0.x;
                p0.x = p1.x;
                p1.x = temp;
            }
            if (p1.y < p0.y) {
                float temp = p0.y;
                p0.y = p1.y;
                p1.y = temp;
            }

            float h_width = 0.5f * (p1.x - p0.x);
            float h_height = 0.5f * (p1.y - p0.y);
            vec3 box_center = V3(p0.x + h_width, p0.y + h_height, 0.0f);
            vec3 box_half_lengths = V3(h_width, h_height, 1.0f);
            ce->select_box.p0 = p0;
            ce->select_box.p1 = p1;

            hovered_array->length = 0;
            if (ce->edit_terrain_model.points_selectable) {
                for (int i = 0; i < model->points.length; i++) {
                    vec3 p = model->points.data[i];
                    p = vec3_apply_mat4(p, 1.0f, model_mat);
                    vec3 point = renderer_world_to_screen(renderer, p);
                    if (point_inside_box(point, box_center, box_half_lengths)) {
                        struct editor_entity hovered = make_editor_entity(EDITOR_ENTITY_POINT, i);
                        array_push(hovered_array, hovered);
                    }
                }
            }

            if (ce->edit_terrain_model.faces_selectable) {
                struct array_vec3 triangle_points;
                struct array_int triangle_idxs;
                struct array_bool is_inside;
                array_init(&triangle_points);
                array_init(&triangle_idxs);
                array_init(&is_inside);

                for (int i = 0; i < model->faces.length; i++) {
                    struct terrain_model_face face = model->faces.data[i];
                    if (face.num_points == 3) {
                        vec3 p0 = terrain_model_get_point(model, face.x);
                        vec3 p1 = terrain_model_get_point(model, face.y);
                        vec3 p2 = terrain_model_get_point(model, face.z);
                        p0 = vec3_apply_mat4(p0, 1.0f, model_mat);
                        p1 = vec3_apply_mat4(p1, 1.0f, model_mat);
                        p2 = vec3_apply_mat4(p2, 1.0f, model_mat);
                        array_push(&triangle_points, p0);
                        array_push(&triangle_points, p1);
                        array_push(&triangle_points, p2);
                        array_push(&triangle_idxs, i);
                        array_push(&is_inside, false);
                    }
                    else if (face.num_points == 4) {
                        vec3 p0 = terrain_model_get_point(model, face.x);
                        vec3 p1 = terrain_model_get_point(model, face.y);
                        vec3 p2 = terrain_model_get_point(model, face.z);
                        vec3 p3 = terrain_model_get_point(model, face.w);
                        p0 = vec3_apply_mat4(p0, 1.0f, model_mat);
                        p1 = vec3_apply_mat4(p1, 1.0f, model_mat);
                        p2 = vec3_apply_mat4(p2, 1.0f, model_mat);
                        p3 = vec3_apply_mat4(p3, 1.0f, model_mat);
                        array_push(&triangle_points, p0);
                        array_push(&triangle_points, p1);
                        array_push(&triangle_points, p2);
                        array_push(&triangle_idxs, i);
                        array_push(&is_inside, false);

                        array_push(&triangle_points, p2);
                        array_push(&triangle_points, p3);
                        array_push(&triangle_points, p0);
                        array_push(&triangle_idxs, i);
                        array_push(&is_inside, false);
                    }
                    else {
                        assert(false);
                    }
                }

                vec3 frustum_corners[8];
                frustum_corners[0] = V3(
                        box_center.x - box_half_lengths.x,
                        box_center.y - box_half_lengths.y, -1.0f);
                frustum_corners[1] = V3(
                        box_center.x + box_half_lengths.x,
                        box_center.y - box_half_lengths.y, -1.0f);
                frustum_corners[2] = V3(
                        box_center.x + box_half_lengths.x,
                        box_center.y + box_half_lengths.y, -1.0f);
                frustum_corners[3] = V3(
                        box_center.x - box_half_lengths.x,
                        box_center.y + box_half_lengths.y, -1.0f);
                frustum_corners[4] = V3(
                        box_center.x - box_half_lengths.x,
                        box_center.y - box_half_lengths.y, 1.0f);
                frustum_corners[5] = V3(
                        box_center.x + box_half_lengths.x,
                        box_center.y - box_half_lengths.y, 1.0f);
                frustum_corners[6] = V3(
                        box_center.x + box_half_lengths.x,
                        box_center.y + box_half_lengths.y, 1.0f);
                frustum_corners[7] = V3(
                        box_center.x - box_half_lengths.x,
                        box_center.y + box_half_lengths.y, 1.0f);
                for (int i = 0; i < 8; i++) {
                    frustum_corners[i] = renderer_screen_to_world(renderer, frustum_corners[i]);
                }
                triangles_inside_frustum(triangle_points.data, triangle_points.length / 3,
                        frustum_corners, is_inside.data);

                for (int i = 0; i < is_inside.length; i++) {
                    if (is_inside.data[i]) {
                        int face_idx = triangle_idxs.data[i];

                        struct editor_entity hovered = make_editor_entity(EDITOR_ENTITY_FACE, face_idx);
                        if (!editor_entity_array_contains_entity(hovered_array, hovered)) {
                            array_push(hovered_array, hovered);

                            struct terrain_model_face face = terrain_model_get_face(model, face_idx);
                            vec3 p0 = terrain_model_get_point(model, face.x);
                            vec3 p1 = terrain_model_get_point(model, face.y);
                            vec3 p2 = terrain_model_get_point(model, face.z);
                            vec3 p3 = terrain_model_get_point(model, face.w);
                            p0 = vec3_apply_mat4(p0, 1.0f, model_mat);
                            p1 = vec3_apply_mat4(p1, 1.0f, model_mat);
                            p2 = vec3_apply_mat4(p2, 1.0f, model_mat);
                            p3 = vec3_apply_mat4(p3, 1.0f, model_mat);
                            p0 = renderer_world_to_screen(renderer, p0);
                            p1 = renderer_world_to_screen(renderer, p1);
                            p2 = renderer_world_to_screen(renderer, p2);
                            p3 = renderer_world_to_screen(renderer, p3);
                        }
                    }
                }

                array_deinit(&triangle_points);
                array_deinit(&triangle_idxs);
                array_deinit(&is_inside);
            }

            if (!om->is_using && !button_inputs.mouse_down[SAPP_MOUSEBUTTON_LEFT]) {
                ce->select_box.is_open = false;
            }
        } else {
            float t = FLT_MAX;
            struct editor_entity hovered;

            if (ce->edit_terrain_model.points_selectable) {
                struct array_vec3 sphere_center;
                struct array_float sphere_radius;
                array_init(&sphere_center);
                array_init(&sphere_radius);

                for (int i = 0; i < model->points.length; i++) {
                    vec3 point = model->points.data[i];
                    point = vec3_apply_mat4(point, 1.0f, model_mat);
                    array_push(&sphere_center, point);
                    array_push(&sphere_radius, 0.04f);
                }

                float t1;
                int idx;
                if (ray_intersect_spheres(button_inputs.mouse_ray_orig, button_inputs.mouse_ray_dir, 
                            sphere_center.data, sphere_radius.data, 
                            sphere_center.length, &t1, &idx)) {
                    if (t1 < t) {
                        t = t1;
                        hovered = make_editor_entity(EDITOR_ENTITY_POINT, idx);
                    }
                }

                array_deinit(&sphere_center);
                array_deinit(&sphere_radius);
            }

            if (ce->edit_terrain_model.faces_selectable) {
                struct array_vec3 triangle_points;
                struct array_int triangle_idxs;
                array_init(&triangle_points);
                array_init(&triangle_idxs);

                for (int i = 0; i < model->faces.length; i++) {
                    struct terrain_model_face face = model->faces.data[i];
                    if (face.num_points == 3) {
                        vec3 p0 = terrain_model_get_point(model, face.x);
                        vec3 p1 = terrain_model_get_point(model, face.y);
                        vec3 p2 = terrain_model_get_point(model, face.z);
                        p0 = vec3_apply_mat4(p0, 1.0f, model_mat);
                        p1 = vec3_apply_mat4(p1, 1.0f, model_mat);
                        p2 = vec3_apply_mat4(p2, 1.0f, model_mat);
                        array_push(&triangle_points, p0);
                        array_push(&triangle_points, p1);
                        array_push(&triangle_points, p2);
                        array_push(&triangle_idxs, i);
                    }
                    else if (face.num_points == 4) {
                        vec3 p0 = terrain_model_get_point(model, face.x);
                        vec3 p1 = terrain_model_get_point(model, face.y);
                        vec3 p2 = terrain_model_get_point(model, face.z);
                        vec3 p3 = terrain_model_get_point(model, face.w);
                        p0 = vec3_apply_mat4(p0, 1.0f, model_mat);
                        p1 = vec3_apply_mat4(p1, 1.0f, model_mat);
                        p2 = vec3_apply_mat4(p2, 1.0f, model_mat);
                        p3 = vec3_apply_mat4(p3, 1.0f, model_mat);
                        array_push(&triangle_points, p0);
                        array_push(&triangle_points, p1);
                        array_push(&triangle_points, p2);
                        array_push(&triangle_idxs, i);

                        array_push(&triangle_points, p2);
                        array_push(&triangle_points, p3);
                        array_push(&triangle_points, p0);
                        array_push(&triangle_idxs, i);
                    }
                    else {
                        assert(false);
                    }
                }

                float t1;
                int idx;
                if (ray_intersect_triangles(button_inputs.mouse_ray_orig, button_inputs.mouse_ray_dir,
                            triangle_points.data, triangle_points.length,  
                            mat4_identity(), &t1, &idx)) {
                    int face_idx = triangle_idxs.data[idx];
                    if (t1 < t) {
                        t = t1;
                        hovered = make_editor_entity(EDITOR_ENTITY_FACE, face_idx);
                    }
                }

                array_deinit(&triangle_points);
                array_deinit(&triangle_idxs);
            }

            hovered_array->length = 0;
            if (t < FLT_MAX) {
                array_push(hovered_array, hovered);
            }
        }
        profiler_pop_section();

        if (!om->is_using) {
            if (button_inputs.mouse_clicked[SAPP_MOUSEBUTTON_LEFT]) {
                om->is_active = false;

                bool shift_down = button_inputs.button_down[SAPP_KEYCODE_LEFT_SHIFT];
                if (!shift_down) {
                    selected_array->length = 0;
                }
                for (int i = 0; i < hovered_array->length; i++) {
                    if (!editor_entity_array_contains_entity(selected_array, hovered_array->data[i])) {
                        array_push(selected_array, hovered_array->data[i]);
                    }
                    else if (shift_down) {
                        for (int j = 0; j < selected_array->length; j++) {
                            if ((selected_array->data[j].type == hovered_array->data[i].type) &&
                                    (selected_array->data[j].idx == hovered_array->data[i].idx)) {
                                array_splice(selected_array, j, 1);
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (selected_array->length > 0) {
            vec3 avg_pos = V3(0.0f, 0.0f, 0.0f);
            for (int i = 0; i < selected_array->length; i++) {
                struct editor_entity entity = selected_array->data[i];
                if (entity.type == EDITOR_ENTITY_POINT) {
                    vec3 pos = vec3_apply_mat4(model->points.data[entity.idx], 1.0f, model_mat);
                    avg_pos = vec3_add(avg_pos, pos);
                }
                else if (entity.type == EDITOR_ENTITY_FACE) {
                    struct terrain_model_face face = model->faces.data[entity.idx];
                    vec3 pos;
                    if (face.num_points == 3) {
                        vec3 p0 = vec3_apply_mat4(model->points.data[face.x], 1.0f, model_mat);
                        vec3 p1 = vec3_apply_mat4(model->points.data[face.y], 1.0f, model_mat);
                        vec3 p2 = vec3_apply_mat4(model->points.data[face.z], 1.0f, model_mat);
                        pos = vec3_scale(vec3_add(p0, vec3_add(p1, p2)), 1.0f / 3.0f);
                    }
                    else if (face.num_points == 4) {
                        vec3 p0 = vec3_apply_mat4(model->points.data[face.x], 1.0f, model_mat);
                        vec3 p1 = vec3_apply_mat4(model->points.data[face.y], 1.0f, model_mat);
                        vec3 p2 = vec3_apply_mat4(model->points.data[face.z], 1.0f, model_mat);
                        vec3 p3 = vec3_apply_mat4(model->points.data[face.w], 1.0f, model_mat);
                        pos = vec3_scale(vec3_add(vec3_add(p0, p1), vec3_add(p2, p3)), 1.0f / 4.0f);
                    }
                    else {
                        assert(false);
                    }
                    avg_pos = vec3_add(avg_pos, pos);
                }
                else {
                    assert(false);
                }
            }
            avg_pos = vec3_scale(avg_pos, 1.0f / selected_array->length);

            if (!om->is_active) {
                object_modifier_activate(om, avg_pos, model_mat);
            }
            if (!om->is_using && !om->was_used) {
                om->pos = avg_pos;
            }

            bool was_using = om->is_using;
            object_modifier_update(om, dt, button_inputs);
            if (om->is_using && !was_using) {
                om->set_start_positions = true;
                hole_editor_push_state(ce);
            }
            if (om->set_start_positions) {
                om->set_start_positions = false;
                ce->object_modifier_entities.length = 0;
                ce->object_modifier_start_positions.length = 0;
                ce->object_modifier_start_orientations.length = 0;

                for (int i = 0; i < selected_array->length; i++) {
                    struct editor_entity selected = selected_array->data[i];
                    if (selected.type == EDITOR_ENTITY_POINT) {
                        if (!editor_entity_array_contains_entity(&ce->object_modifier_entities, selected)) {
                            array_push(&ce->object_modifier_entities, selected);
                            array_push(&ce->object_modifier_start_positions, model->points.data[selected.idx]);
                            array_push(&ce->object_modifier_start_orientations, QUAT(1.0f, 0.0f, 0.0f, 0.0f));
                        }
                    }
                    else if (selected.type == EDITOR_ENTITY_FACE) {
                        struct terrain_model_face face = model->faces.data[selected.idx];
                        if (face.num_points == 3) {
                            struct editor_entity point0 = make_editor_entity(EDITOR_ENTITY_POINT, face.x);
                            struct editor_entity point1 = make_editor_entity(EDITOR_ENTITY_POINT, face.y);
                            struct editor_entity point2 = make_editor_entity(EDITOR_ENTITY_POINT, face.z);
                            if (!editor_entity_array_contains_entity(&ce->object_modifier_entities, point0)) {
                                array_push(&ce->object_modifier_entities, point0);
                                array_push(&ce->object_modifier_start_positions, model->points.data[point0.idx]);
                                array_push(&ce->object_modifier_start_orientations, QUAT(1.0f, 0.0f, 0.0f, 0.0f));
                            }
                            if (!editor_entity_array_contains_entity(&ce->object_modifier_entities, point1)) {
                                array_push(&ce->object_modifier_entities, point1);
                                array_push(&ce->object_modifier_start_positions, model->points.data[point1.idx]);
                                array_push(&ce->object_modifier_start_orientations, QUAT(1.0f, 0.0f, 0.0f, 0.0f));
                            }
                            if (!editor_entity_array_contains_entity(&ce->object_modifier_entities, point2)) {
                                array_push(&ce->object_modifier_entities, point2);
                                array_push(&ce->object_modifier_start_positions, model->points.data[point2.idx]);
                                array_push(&ce->object_modifier_start_orientations, QUAT(1.0f, 0.0f, 0.0f, 0.0f));
                            }
                        }
                        else if (face.num_points == 4) {
                            struct editor_entity point0 = make_editor_entity(EDITOR_ENTITY_POINT, face.x);
                            struct editor_entity point1 = make_editor_entity(EDITOR_ENTITY_POINT, face.y);
                            struct editor_entity point2 = make_editor_entity(EDITOR_ENTITY_POINT, face.z);
                            struct editor_entity point3 = make_editor_entity(EDITOR_ENTITY_POINT, face.w);
                            if (!editor_entity_array_contains_entity(&ce->object_modifier_entities, point0)) {
                                array_push(&ce->object_modifier_entities, point0);
                                array_push(&ce->object_modifier_start_positions, model->points.data[point0.idx]);
                                array_push(&ce->object_modifier_start_orientations, QUAT(1.0f, 0.0f, 0.0f, 0.0f));
                            }
                            if (!editor_entity_array_contains_entity(&ce->object_modifier_entities, point1)) {
                                array_push(&ce->object_modifier_entities, point1);
                                array_push(&ce->object_modifier_start_positions, model->points.data[point1.idx]);
                                array_push(&ce->object_modifier_start_orientations, QUAT(1.0f, 0.0f, 0.0f, 0.0f));
                            }
                            if (!editor_entity_array_contains_entity(&ce->object_modifier_entities, point2)) {
                                array_push(&ce->object_modifier_entities, point2);
                                array_push(&ce->object_modifier_start_positions, model->points.data[point2.idx]);
                                array_push(&ce->object_modifier_start_orientations, QUAT(1.0f, 0.0f, 0.0f, 0.0f));
                            }
                            if (!editor_entity_array_contains_entity(&ce->object_modifier_entities, point3)) {
                                array_push(&ce->object_modifier_entities, point3);
                                array_push(&ce->object_modifier_start_positions, model->points.data[point3.idx]);
                                array_push(&ce->object_modifier_start_orientations, QUAT(1.0f, 0.0f, 0.0f, 0.0f));
                            }
                        }
                        else {
                            assert(false);
                        }
                    }
                    else {
                        assert(false);
                    }
                }
            }

            if (om->is_using || om->was_used) {
                if (om->mode == OBJECT_MODIFIER_MODE_TRANSLATE) {
                    vec3 delta_pos = vec3_apply_mat4(om->translate_mode.delta_pos, 0.0f, mat4_inverse(model_mat));
                    for (int i = 0; i < ce->object_modifier_entities.length; i++) {
                        struct editor_entity entity = ce->object_modifier_entities.data[i];
                        if (entity.type == EDITOR_ENTITY_POINT) {
                            vec3 *point = &model->points.data[entity.idx];
                            *point = vec3_add(ce->object_modifier_start_positions.data[i], delta_pos);

                            float clamp = om->translate_mode.clamp;
                            if (clamp > 0.0f) {
                                point->x = roundf(point->x / clamp) * clamp;
                                point->y = roundf(point->y / clamp) * clamp;
                                point->z = roundf(point->z / clamp) * clamp;
                            }
                        }
                        else {
                            assert(false);
                        }
                    }
                }
                else if (om->mode == OBJECT_MODIFIER_MODE_ROTATE) {
                    vec3 rotate_point = vec3_apply_mat4(om->pos, 1.0f, mat4_inverse(model_mat));
                    float delta_theta = om->rotate_mode.delta_theta; 
                    for (int i = 0; i < ce->object_modifier_entities.length; i++) {
                        struct editor_entity entity = ce->object_modifier_entities.data[i];
                        if (entity.type == EDITOR_ENTITY_POINT) {
                            vec3 *point = &model->points.data[entity.idx];
                            vec3 start_point = ce->object_modifier_start_positions.data[i];
                            vec3 v = vec3_sub(start_point, rotate_point);
                            v = vec3_rotate_y(v, -delta_theta);
                            *point = vec3_add(rotate_point, v);

                            float clamp = om->translate_mode.clamp;
                            if (clamp > 0.0f) {
                                point->x = roundf(point->x / clamp) * clamp;
                                point->y = roundf(point->y / clamp) * clamp;
                                point->z = roundf(point->z / clamp) * clamp;
                            }
                        }
                        else {
                            assert(false);
                        }
                    }
                }
            }
        }
        else {
            om->is_active = false;
            om->is_using = false;
        }
    }
    else {
        struct hole *hole = ce->hole;
        struct editor_entity_array *hovered_array = &ce->hovered_array;
        struct editor_entity_array *selected_array = &ce->selected_array;

        profiler_push_section("get_hovered_entity");
        {
            struct array_vec3 triangle_points;
            struct editor_entity_array entities;
            array_init(&triangle_points);
            array_init(&entities);

            profiler_push_section("push_environment_entities_triangles");
            for (int i = 0; i < hole->environment_entities.length; i++) {
                struct environment_entity *environment = &hole->environment_entities.data[i];
                struct model *model = environment->model;
                mat4 model_mat = environment_entity_get_transform(environment);
                struct editor_entity entity = make_editor_entity(EDITOR_ENTITY_ENVIRONMENT, i);
                for (int i = 0; i < model->num_points; i += 3) {
                    vec3 p0 = vec3_apply_mat4(model->positions[i + 0], 1.0f, model_mat);
                    vec3 p1 = vec3_apply_mat4(model->positions[i + 1], 1.0f, model_mat);
                    vec3 p2 = vec3_apply_mat4(model->positions[i + 2], 1.0f, model_mat);
                    array_push(&triangle_points, p0);
                    array_push(&triangle_points, p1);
                    array_push(&triangle_points, p2);
                    array_push(&entities, entity);
                }
            }
            profiler_pop_section();

            profiler_push_section("push_water_entities_triangles");
            for (int i = 0; i < hole->water_entities.length; i++) {
                struct water_entity *water = &hole->water_entities.data[i];
                struct terrain_model *model = &water->model;
                mat4 model_mat = water_entity_get_transform(water);
                struct editor_entity entity = make_editor_entity(EDITOR_ENTITY_WATER, i);
                for (int j = 0; j < model->faces.length; j++) {
                    struct terrain_model_face face = model->faces.data[j];
                    assert(face.num_points == 3 || face.num_points == 4);

                    vec3 p0 = vec3_apply_mat4(model->points.data[face.x], 1.0f, model_mat);
                    vec3 p1 = vec3_apply_mat4(model->points.data[face.y], 1.0f, model_mat);
                    vec3 p2 = vec3_apply_mat4(model->points.data[face.z], 1.0f, model_mat);
                    array_push(&triangle_points, p0);
                    array_push(&triangle_points, p1);
                    array_push(&triangle_points, p2);
                    array_push(&entities, entity);
                    if (face.num_points == 4) {
                        vec3 p3 = vec3_apply_mat4(model->points.data[face.w], 1.0f, model_mat);
                        array_push(&triangle_points, p2);
                        array_push(&triangle_points, p3);
                        array_push(&triangle_points, p0);
                        array_push(&entities, entity);
                    }
                }
            }
            profiler_pop_section();

            profiler_push_section("push_terrain_entities_triangles");
            for (int i = 0; i < hole->terrain_entities.length; i++) {
                struct terrain_entity *terrain = &hole->terrain_entities.data[i];
                struct terrain_model *model = &terrain->terrain_model;
                mat4 model_mat = terrain_entity_get_transform(terrain);
                for (int j = 0; j < model->faces.length; j++) {
                    struct terrain_model_face face = model->faces.data[j];
                    assert(face.num_points == 3 || face.num_points == 4);

                    struct editor_entity entity = make_editor_entity(EDITOR_ENTITY_TERRAIN, i);
                    vec3 p0 = vec3_apply_mat4(model->points.data[face.x], 1.0f, model_mat);
                    vec3 p1 = vec3_apply_mat4(model->points.data[face.y], 1.0f, model_mat);
                    vec3 p2 = vec3_apply_mat4(model->points.data[face.z], 1.0f, model_mat);
                    array_push(&triangle_points, p0);
                    array_push(&triangle_points, p1);
                    array_push(&triangle_points, p2);
                    array_push(&entities, entity);
                    if (face.num_points == 4) {
                        vec3 p3 = vec3_apply_mat4(model->points.data[face.w], 1.0f, model_mat);
                        array_push(&triangle_points, p2);
                        array_push(&triangle_points, p3);
                        array_push(&triangle_points, p0);
                        array_push(&entities, entity);
                    }
                }
            }
            profiler_pop_section();

            profiler_push_section("push_multi_terrain_entities_triangles");
            for (int i = 0; i < hole->multi_terrain_entities.length; i++) {
                struct multi_terrain_entity *multi_terrain = &hole->multi_terrain_entities.data[i];
                struct terrain_model *moving_model = &multi_terrain->moving_terrain_model;
                struct terrain_model *static_model = &multi_terrain->static_terrain_model;
                mat4 moving_model_mat = multi_terrain_entity_get_moving_transform(multi_terrain, ed->game->t);
                mat4 static_model_mat = multi_terrain_entity_get_static_transform(multi_terrain);
                for (int j = 0; j < moving_model->faces.length; j++) {
                    struct terrain_model_face face = moving_model->faces.data[j];
                    assert(face.num_points == 3 || face.num_points == 4);

                    struct editor_entity entity = make_editor_entity(EDITOR_ENTITY_MULTI_TERRAIN_MOVING, i);
                    vec3 p0 = vec3_apply_mat4(moving_model->points.data[face.x], 1.0f, moving_model_mat);
                    vec3 p1 = vec3_apply_mat4(moving_model->points.data[face.y], 1.0f, moving_model_mat);
                    vec3 p2 = vec3_apply_mat4(moving_model->points.data[face.z], 1.0f, moving_model_mat);
                    array_push(&triangle_points, p0);
                    array_push(&triangle_points, p1);
                    array_push(&triangle_points, p2);
                    array_push(&entities, entity);
                    if (face.num_points == 4) {
                        vec3 p3 = vec3_apply_mat4(moving_model->points.data[face.w], 1.0f, moving_model_mat);
                        array_push(&triangle_points, p2);
                        array_push(&triangle_points, p3);
                        array_push(&triangle_points, p0);
                        array_push(&entities, entity);
                    }
                }
                for (int j = 0; j < static_model->faces.length; j++) {
                    struct terrain_model_face face = static_model->faces.data[j];
                    assert(face.num_points == 3 || face.num_points == 4);

                    struct editor_entity entity = make_editor_entity(EDITOR_ENTITY_MULTI_TERRAIN_STATIC, i);
                    vec3 p0 = vec3_apply_mat4(static_model->points.data[face.x], 1.0f, static_model_mat);
                    vec3 p1 = vec3_apply_mat4(static_model->points.data[face.y], 1.0f, static_model_mat);
                    vec3 p2 = vec3_apply_mat4(static_model->points.data[face.z], 1.0f, static_model_mat);
                    array_push(&triangle_points, p0);
                    array_push(&triangle_points, p1);
                    array_push(&triangle_points, p2);
                    array_push(&entities, entity);
                    if (face.num_points == 4) {
                        vec3 p3 = vec3_apply_mat4(static_model->points.data[face.w], 1.0f, static_model_mat);
                        array_push(&triangle_points, p2);
                        array_push(&triangle_points, p3);
                        array_push(&triangle_points, p0);
                        array_push(&entities, entity);
                    }
                }
            }
            profiler_pop_section();

            profiler_push_section("push_ball_start_triangles");
            {
                struct ball_start_entity *ball_start = &hole->ball_start_entity;
                mat4 model_mat = ball_start_entity_get_transform(ball_start);
                struct model *model = ball_start->model;
                struct editor_entity entity = make_editor_entity(EDITOR_ENTITY_BALL_START, 0);
                for (int i = 0; i < model->num_points; i += 3) {
                    vec3 p0 = vec3_apply_mat4(model->positions[i + 0], 1.0f, model_mat);
                    vec3 p1 = vec3_apply_mat4(model->positions[i + 1], 1.0f, model_mat);
                    vec3 p2 = vec3_apply_mat4(model->positions[i + 2], 1.0f, model_mat);
                    array_push(&triangle_points, p0);
                    array_push(&triangle_points, p1);
                    array_push(&triangle_points, p2);
                    array_push(&entities, entity);
                }
            }
            profiler_pop_section();

            profiler_push_section("push_cup_triangles");
            {
                struct cup_entity *cup = &hole->cup_entity;
                mat4 model_mat = cup_entity_get_transform(cup);
                struct model *model = cup->model;
                struct editor_entity entity = make_editor_entity(EDITOR_ENTITY_CUP, 0);
                for (int i = 0; i < model->num_points; i += 3) {
                    vec3 p0 = vec3_apply_mat4(model->positions[i + 0], 1.0f, model_mat);
                    vec3 p1 = vec3_apply_mat4(model->positions[i + 1], 1.0f, model_mat);
                    vec3 p2 = vec3_apply_mat4(model->positions[i + 2], 1.0f, model_mat);
                    array_push(&triangle_points, p0);
                    array_push(&triangle_points, p1);
                    array_push(&triangle_points, p2);
                    array_push(&entities, entity);
                }
            }
            profiler_pop_section();

            profiler_push_section("push_beginning_camera_animation_triangles");
            {
                struct beginning_camera_animation_entity *anim = &hole->beginning_camera_animation_entity;
                mat4 model_mat = beginning_camera_animation_entity_get_transform(anim);
                struct model *model = asset_store_get_model("sphere");
                struct editor_entity entity = make_editor_entity(EDITOR_ENTITY_BEGINNING_CAMERA_ANIMATION, 0);
                for (int i = 0; i < model->num_points; i += 3) {
                    vec3 p0 = vec3_apply_mat4(model->positions[i + 0], 1.0f, model_mat);
                    vec3 p1 = vec3_apply_mat4(model->positions[i + 1], 1.0f, model_mat);
                    vec3 p2 = vec3_apply_mat4(model->positions[i + 2], 1.0f, model_mat);
                    array_push(&triangle_points, p0);
                    array_push(&triangle_points, p1);
                    array_push(&triangle_points, p2);
                    array_push(&entities, entity);
                }
            }
            profiler_pop_section();

            if (ce->camera_zone_entities.can_modify) {
                profiler_push_section("push_camera_zone_entity_triangles");
                for (int i = 0; i < hole->camera_zone_entities.length; i++) {
                    struct camera_zone_entity *camera_zone = &hole->camera_zone_entities.data[i];
                    struct model *model = asset_store_get_model("cube");
                    mat4 model_mat = camera_zone_entity_get_transform(camera_zone);
                    struct editor_entity entity = make_editor_entity(EDITOR_ENTITY_CAMERA_ZONE, i);
                    for (int i = 0; i < model->num_points; i += 3) {
                        vec3 p0 = vec3_apply_mat4(model->positions[i + 0], 1.0f, model_mat);
                        vec3 p1 = vec3_apply_mat4(model->positions[i + 1], 1.0f, model_mat);
                        vec3 p2 = vec3_apply_mat4(model->positions[i + 2], 1.0f, model_mat);
                        array_push(&triangle_points, p0);
                        array_push(&triangle_points, p1);
                        array_push(&triangle_points, p2);
                        array_push(&entities, entity);
                    }
                }
                profiler_pop_section();
            }

            float *t = malloc(sizeof(float) * (triangle_points.length / 3));
            profiler_push_section("ray_intersect_triangles");
            ray_intersect_triangles_all(button_inputs.mouse_ray_orig, button_inputs.mouse_ray_dir, 
                    triangle_points.data, triangle_points.length, mat4_identity(), t);
            profiler_pop_section();
            float min_t = FLT_MAX;
            int min_idx = 0;
            for (int i = 0; i < triangle_points.length / 3; i++) {
                if (t[i] < min_t) {
                    min_t = t[i];
                    min_idx = i;
                }
                else if (entities.data[i].type == EDITOR_ENTITY_CUP && t[i] < FLT_MAX) {
                    min_t = 0.0f;
                    min_idx = i;
                }
            }
            hovered_array->length = 0;
            if (min_t < FLT_MAX && !om->is_hovered && !om->is_using) {
                array_push(hovered_array, entities.data[min_idx]);
            }

            profiler_section_add_var("triangle_points_length", triangle_points.length);
            profiler_section_add_var("entities_length", entities.length);

            free(t);
            array_deinit(&triangle_points);
            array_deinit(&entities);
        }
        profiler_pop_section();

        if (!om->is_hovered && !om->is_using) {
            if (button_inputs.mouse_clicked[SAPP_MOUSEBUTTON_LEFT]) {
                om->is_active = false;
                if (!button_inputs.button_down[SAPP_KEYCODE_LEFT_SHIFT]) {
                    selected_array->length = 0;
                }
                for (int i = 0; i < hovered_array->length; i++) {
                    if (!editor_entity_array_contains_entity(selected_array, hovered_array->data[i])) {
                        array_push(selected_array, hovered_array->data[i]);
                    }
                }
            }
        }

        if (selected_array->length > 0) {
            vec3 avg_pos = V3(0.0f, 0.0f, 0.0f);
            for (int i = 0; i < selected_array->length; i++) {
                struct editor_entity entity = selected_array->data[i];
                if (entity.type == EDITOR_ENTITY_TERRAIN) {
                    struct terrain_entity *terrain = &hole->terrain_entities.data[entity.idx];
                    avg_pos = vec3_add(avg_pos, terrain->position);
                }
                else if (entity.type == EDITOR_ENTITY_BALL_START) {
                    struct ball_start_entity *ball_start = &hole->ball_start_entity;
                    avg_pos = vec3_add(avg_pos, ball_start->position);
                }
                else if (entity.type == EDITOR_ENTITY_CUP) {
                    struct cup_entity *cup = &hole->cup_entity;
                    avg_pos = vec3_add(avg_pos, cup->position);
                }
                else if (entity.type == EDITOR_ENTITY_BEGINNING_CAMERA_ANIMATION) {
                    struct beginning_camera_animation_entity *cam_anim =
                        &hole->beginning_camera_animation_entity;
                    avg_pos = vec3_add(avg_pos, cam_anim->start_position);
                }
                else if (entity.type == EDITOR_ENTITY_CAMERA_ZONE) {
                    struct camera_zone_entity *camera_zone = &hole->camera_zone_entities.data[entity.idx];
                    avg_pos = vec3_add(avg_pos, camera_zone->position);
                }
                else if (entity.type == EDITOR_ENTITY_MULTI_TERRAIN_MOVING) {
                    struct multi_terrain_entity *multi_terrain = 
                        &hole->multi_terrain_entities.data[entity.idx];
                    avg_pos = vec3_add(avg_pos, multi_terrain->moving_position);
                }
                else if (entity.type == EDITOR_ENTITY_MULTI_TERRAIN_STATIC) {
                    struct multi_terrain_entity *multi_terrain = 
                        &hole->multi_terrain_entities.data[entity.idx];
                    avg_pos = vec3_add(avg_pos, multi_terrain->static_position);
                }
                else if (entity.type == EDITOR_ENTITY_ENVIRONMENT) {
                    struct environment_entity *environment = 
                        &hole->environment_entities.data[entity.idx];
                    avg_pos = vec3_add(avg_pos, environment->position);
                }
                else if (entity.type == EDITOR_ENTITY_WATER) {
                    struct water_entity *water = 
                        &hole->water_entities.data[entity.idx];
                    avg_pos = vec3_add(avg_pos, water->position);
                }
                else {
                    assert(false);
                }
            }
            avg_pos = vec3_scale(avg_pos, 1.0f / selected_array->length);

            if (!om->is_active) {
                object_modifier_activate(om, avg_pos, mat4_identity());
            }
            if (!om->is_using && !om->was_used) {
                om->pos = avg_pos;
            }

            bool was_using = om->is_using;
            object_modifier_update(om, dt, button_inputs);
            if (om->is_using && !was_using) {
                om->set_start_positions = true;
                hole_editor_push_state(ce);
            }
            if (om->set_start_positions) {
                om->set_start_positions = false;
                ce->object_modifier_entities.length = 0;
                ce->object_modifier_start_positions.length = 0;
                ce->object_modifier_start_orientations.length = 0;

                for (int i = 0; i < selected_array->length; i++) {
                    struct editor_entity selected = selected_array->data[i];
                    if (selected.type == EDITOR_ENTITY_TERRAIN) {
                        struct terrain_entity *terrain = &hole->terrain_entities.data[selected.idx];
                        array_push(&ce->object_modifier_entities, selected);
                        array_push(&ce->object_modifier_start_positions, terrain->position);
                        array_push(&ce->object_modifier_start_orientations, terrain->orientation);
                    }
                    else if (selected.type == EDITOR_ENTITY_BALL_START) {
                        struct ball_start_entity *ball_start = &hole->ball_start_entity;
                        array_push(&ce->object_modifier_entities, selected);
                        array_push(&ce->object_modifier_start_positions, ball_start->position);
                        array_push(&ce->object_modifier_start_orientations, QUAT(1.0f, 0.0f, 0.0f, 0.0f));
                    }
                    else if (selected.type == EDITOR_ENTITY_CUP) {
                        struct cup_entity *cup = &hole->cup_entity;
                        array_push(&ce->object_modifier_entities, selected);
                        array_push(&ce->object_modifier_start_positions, cup->position);
                        array_push(&ce->object_modifier_start_orientations, QUAT(1.0f, 0.0f, 0.0f, 0.0f));
                    }
                    else if (selected.type == EDITOR_ENTITY_BEGINNING_CAMERA_ANIMATION) {
                        struct beginning_camera_animation_entity *cam_anim = 
                            &hole->beginning_camera_animation_entity;
                        array_push(&ce->object_modifier_entities, selected);
                        array_push(&ce->object_modifier_start_positions, cam_anim->start_position);
                        array_push(&ce->object_modifier_start_orientations, QUAT(1.0f, 0.0f, 0.0f, 0.0f));
                    }
                    else if (selected.type == EDITOR_ENTITY_CAMERA_ZONE) {
                        struct camera_zone_entity *camera_zone = &hole->camera_zone_entities.data[selected.idx];
                        array_push(&ce->object_modifier_entities, selected);
                        array_push(&ce->object_modifier_start_positions, camera_zone->position);
                        array_push(&ce->object_modifier_start_orientations, camera_zone->orientation);
                    }
                    else if (selected.type == EDITOR_ENTITY_MULTI_TERRAIN_MOVING) {
                        struct multi_terrain_entity *entity = 
                            &hole->multi_terrain_entities.data[selected.idx];
                        array_push(&ce->object_modifier_entities, selected);
                        array_push(&ce->object_modifier_start_positions, entity->moving_position);
                        array_push(&ce->object_modifier_start_orientations, entity->moving_orientation);
                    }
                    else if (selected.type == EDITOR_ENTITY_MULTI_TERRAIN_STATIC) {
                        struct multi_terrain_entity *entity = 
                            &hole->multi_terrain_entities.data[selected.idx];
                        array_push(&ce->object_modifier_entities, selected);
                        array_push(&ce->object_modifier_start_positions, entity->static_position);
                        array_push(&ce->object_modifier_start_orientations, entity->static_orientation);
                    }
                    else if (selected.type == EDITOR_ENTITY_ENVIRONMENT) {
                        struct environment_entity *entity = 
                            &hole->environment_entities.data[selected.idx];
                        array_push(&ce->object_modifier_entities, selected);
                        array_push(&ce->object_modifier_start_positions, entity->position);
                        array_push(&ce->object_modifier_start_orientations, entity->orientation);
                    }
                    else if (selected.type == EDITOR_ENTITY_WATER) {
                        struct water_entity *entity = 
                            &hole->water_entities.data[selected.idx];
                        array_push(&ce->object_modifier_entities, selected);
                        array_push(&ce->object_modifier_start_positions, entity->position);
                        array_push(&ce->object_modifier_start_orientations, entity->orientation);
                    }
                    else {
                        assert(false);
                    }
                }
            }

            if (om->is_using || om->was_used) {
                if (om->mode == OBJECT_MODIFIER_MODE_TRANSLATE) {
                    vec3 delta_pos = om->translate_mode.delta_pos;
                    for (int i = 0; i < ce->object_modifier_entities.length; i++) {
                        struct editor_entity entity = ce->object_modifier_entities.data[i];
                        vec3 start_position = ce->object_modifier_start_positions.data[i];
                        vec3 *position = NULL;

                        if (entity.type == EDITOR_ENTITY_TERRAIN) {
                            struct terrain_entity *terrain = &hole->terrain_entities.data[entity.idx];
                            position = &terrain->position;
                        }
                        else if (entity.type == EDITOR_ENTITY_BALL_START) {
                            struct ball_start_entity *ball_start = &hole->ball_start_entity;
                            position = &ball_start->position;
                        }
                        else if (entity.type == EDITOR_ENTITY_CUP) {
                            struct cup_entity *cup = &hole->cup_entity;
                            position = &cup->position;
                        }
                        else if (entity.type == EDITOR_ENTITY_BEGINNING_CAMERA_ANIMATION) {
                            struct beginning_camera_animation_entity *cam_anim = 
                                &hole->beginning_camera_animation_entity;
                            position = &cam_anim->start_position;
                        }
                        else if (entity.type == EDITOR_ENTITY_CAMERA_ZONE) {
                            struct camera_zone_entity *camera_zone = 
                                &hole->camera_zone_entities.data[entity.idx];
                            position = &camera_zone->position;
                        }
                        else if (entity.type == EDITOR_ENTITY_MULTI_TERRAIN_MOVING) {
                            struct multi_terrain_entity *multi_terrain = 
                                &hole->multi_terrain_entities.data[entity.idx];
                            position = &multi_terrain->moving_position;
                        }
                        else if (entity.type == EDITOR_ENTITY_MULTI_TERRAIN_STATIC) {
                            struct multi_terrain_entity *multi_terrain = 
                                &hole->multi_terrain_entities.data[entity.idx];
                            position = &multi_terrain->static_position;
                        }
                        else if (entity.type == EDITOR_ENTITY_ENVIRONMENT) {
                            struct environment_entity *environment = 
                                &hole->environment_entities.data[entity.idx];
                            position = &environment->position;
                        }
                        else if (entity.type == EDITOR_ENTITY_WATER) {
                            struct water_entity *water = 
                                &hole->water_entities.data[entity.idx];
                            position = &water->position;
                        }
                        else {
                            assert(false);
                        }

                        if (position) {
                            *position = vec3_add(start_position, delta_pos);
                            float clamp = om->translate_mode.clamp;
                            if (clamp > 0.0f) {
                                position->x = roundf(position->x / clamp) * clamp;
                                position->y = roundf(position->y / clamp) * clamp;
                                position->z = roundf(position->z / clamp) * clamp;
                            }
                        }
                    }
                }
                else if (om->mode == OBJECT_MODIFIER_MODE_ROTATE) {
                    float delta_theta = om->rotate_mode.delta_theta;
                    for (int i = 0; i < ce->object_modifier_entities.length; i++) {
                        struct editor_entity entity = ce->object_modifier_entities.data[i];
                        vec3 start_position = ce->object_modifier_start_positions.data[i];
                        quat start_orientation = ce->object_modifier_start_orientations.data[i];
                        vec3 *position = NULL;
                        quat *orientation = NULL;

                        if (entity.type == EDITOR_ENTITY_TERRAIN) {
                            struct terrain_entity *terrain = &hole->terrain_entities.data[entity.idx];
                            position = &terrain->position;
                            orientation = &terrain->orientation;
                        }
                        else if (entity.type == EDITOR_ENTITY_BALL_START) {
                            struct ball_start_entity *ball_start = &hole->ball_start_entity;
                            position = &ball_start->position;
                            orientation = NULL;
                        }
                        else if (entity.type == EDITOR_ENTITY_CUP) {
                            struct cup_entity *cup = &hole->cup_entity;
                            position = &cup->position;
                            orientation = NULL;
                        }
                        else if (entity.type == EDITOR_ENTITY_BEGINNING_CAMERA_ANIMATION) {
                            struct beginning_camera_animation_entity *cam_anim = 
                                &hole->beginning_camera_animation_entity;
                            position = &cam_anim->start_position;
                            orientation = NULL;
                        }
                        else if (entity.type == EDITOR_ENTITY_CAMERA_ZONE) {
                            struct camera_zone_entity *camera_zone = 
                                &hole->camera_zone_entities.data[entity.idx];
                            position = &camera_zone->position;
                            orientation = &camera_zone->orientation;
                        }
                        else if (entity.type == EDITOR_ENTITY_MULTI_TERRAIN_MOVING) {
                            struct multi_terrain_entity *multi_terrain = 
                                &hole->multi_terrain_entities.data[entity.idx];
                            position = &multi_terrain->moving_position;
                            orientation = &multi_terrain->moving_orientation;
                        }
                        else if (entity.type == EDITOR_ENTITY_MULTI_TERRAIN_STATIC) {
                            struct multi_terrain_entity *multi_terrain = 
                                &hole->multi_terrain_entities.data[entity.idx];
                            position = &multi_terrain->static_position;
                            orientation = &multi_terrain->static_orientation;
                        }
                        else if (entity.type == EDITOR_ENTITY_ENVIRONMENT) {
                            struct environment_entity *environment = 
                                &hole->environment_entities.data[entity.idx];
                            position = &environment->position;
                            orientation = &environment->orientation;
                        }
                        else if (entity.type == EDITOR_ENTITY_WATER) {
                            struct water_entity *water = 
                                &hole->water_entities.data[entity.idx];
                            position = &water->position;
                            orientation = &water->orientation;
                        }
                        else {
                            assert(false);
                        }

                        if (position && orientation) {
                            if (om->rotate_mode.axis == 0) {
                                vec3 axis = vec3_normalize(vec3_apply_quat(V3(1.0f, 0.0f, 0.0f), 0.0f, start_orientation));
                                *orientation = quat_multiply(start_orientation, 
                                        quat_create_from_axis_angle(axis, -delta_theta));

                                vec3 v = vec3_sub(start_position, om->pos);
                                v = vec3_rotate_x(v, -delta_theta);
                                *position = vec3_add(om->pos, v);
                            }
                            else if (om->rotate_mode.axis == 1) {
                                vec3 axis = vec3_normalize(vec3_apply_quat(V3(0.0f, 1.0f, 0.0f), 0.0f, start_orientation));
                                *orientation = quat_multiply(start_orientation, 
                                        quat_create_from_axis_angle(axis, -delta_theta));

                                vec3 v = vec3_sub(start_position, om->pos);
                                v = vec3_rotate_y(v, -delta_theta);
                                *position = vec3_add(om->pos, v);
                            }
                            else if (om->rotate_mode.axis == 2) {
                                vec3 axis = vec3_normalize(vec3_apply_quat(V3(0.0f, 0.0f, 1.0f), 0.0f, start_orientation));
                                *orientation = quat_multiply(start_orientation, 
                                        quat_create_from_axis_angle(axis, -delta_theta));

                                vec3 v = vec3_sub(start_position, om->pos);
                                v = vec3_rotate_z(v, -delta_theta);
                                *position = vec3_add(om->pos, v);
                            }
                        }
                    }
                }
                else {
                    assert(false);
                }
            }
        }
        else {
            om->is_active = false;
            om->is_using = false;
        }
    }
    hole_update_buffers(ce->hole);

    {
#if defined(_WIN32)
        {
            if (ce->global_illumination.open_in_progress_dialog) {
                igSetNextWindowSize((ImVec2){400, 200}, ImGuiCond_FirstUseEver);
                igOpenPopup("Lightmap Generator In Progress");
                ce->global_illumination.open_in_progress_dialog = false;
            }

            igSetNextWindowSize((ImVec2){400, 200}, ImGuiCond_FirstUseEver);
            if (igBeginPopupModal("Lightmap Generator In Progress", NULL, 0)) {
                char progress_string[2048];
                lightmap_generator_get_progress_string(progress_string, 2048);
                igTextWrapped(progress_string);

                if (!ce->global_illumination.do_it) {
                    igCloseCurrentPopup();
                }
                igEndPopup();
            }
        }
#endif

        {
            if (ce->open_save_as_dialog) {
                igSetNextWindowSize((ImVec2){400, 200}, ImGuiCond_FirstUseEver);
                igOpenPopup("Save As");
                ce->open_save_as_dialog = false;
            }

            igSetNextWindowSize((ImVec2){400, 200}, ImGuiCond_FirstUseEver);
            if (igBeginPopupModal("Save As", NULL, 0)) {
                igInputText("Path", ce->file.path, MFILE_MAX_PATH, 0, NULL, NULL);
                if (igButton("Save", (ImVec2){0, 0})) {
                    mfile_t file = mfile(ce->file.path);
                    hole_save(ce->hole, &file);
                    hole_load(ce->hole, &file);
                    igCloseCurrentPopup();
                }
                igEndPopup();
            }
        }

        {
            if (ce->open_load_dialog) {
                igSetNextWindowSize((ImVec2){400, 600}, ImGuiCond_FirstUseEver);
                igOpenPopup("Load");
                mdir_deinit(&ce->file.holes_directory);
                mdir_init(&ce->file.holes_directory, "assets/holes", false);
                ce->open_load_dialog = false;
            }

            igSetNextWindowSize((ImVec2){400, 600}, ImGuiCond_FirstUseEver);
            if (igBeginPopupModal("Load", NULL, 0)) {
                for (int i =- 0; i < ce->file.holes_directory.num_files; i++) {
                    mfile_t f = ce->file.holes_directory.files[i];
                    if (igSelectableBool(f.path, false, ImGuiSelectableFlags_AllowDoubleClick, 
                                (ImVec2){0, 0})) {
                        strncpy(ce->file.path, f.path, MFILE_MAX_PATH);
                        ce->file.path[MFILE_MAX_PATH - 1] = 0;
                        ce->selected_array.length = 0;
                        ce->hovered_array.length = 0;
                        ce->edit_terrain_model.selected_array.length = 0;
                        ce->edit_terrain_model.hovered_array.length = 0;

                        mfile_t file = mfile(ce->file.path);
                        hole_reset(ce->hole);
                        hole_load(ce->hole, &file);
                        igCloseCurrentPopup();
                    }
                }
                igEndPopup();
            }
        }

        {
            static bool hole_editor_window_open = false;
            igSetNextWindowSize((ImVec2){300, 300}, ImGuiCond_FirstUseEver);
            igSetNextWindowPos((ImVec2){5, 5}, ImGuiCond_FirstUseEver, (ImVec2){0, 0});
            igBegin("Course Editor", &hole_editor_window_open, ImGuiWindowFlags_MenuBar);

            if (igBeginMenuBar()) {
                if (igBeginMenu("File", true)) {
                    if (igMenuItemBool("New", NULL, false, true)) {
                        hole_reset(ce->hole);
                        ce->file.path[0] = 0;
                        ce->selected_array.length = 0;
                        ce->hovered_array.length = 0;
                        ce->edit_terrain_model.selected_array.length = 0;
                        ce->edit_terrain_model.hovered_array.length = 0;
                    }
                    if (igMenuItemBool("Save", NULL, false, true)) {
                        if (!ce->file.path[0]) {
                            ce->open_save_as_dialog = true;
                        }
                        else {
                            mfile_t file = mfile(ce->file.path);
                            hole_save(ce->hole, &file);
                            hole_load(ce->hole, &file);
                        }
                    }
                    if (igMenuItemBool("Save As", NULL, false, true)) {
                        ce->open_save_as_dialog = true;
                    }
                    if (igMenuItemBool("Open", NULL, false, true)) {
                        ce->open_load_dialog = true;
                    }
                    if (igMenuItemBool("Quit", NULL, false, true)) {
                        ed->editing_hole = false;
                        game_physics_load_triangles(ed->game);
                    }
                    igEndMenu();
                }
                if (igBeginMenu("Edit", true)) {
                    if (igMenuItemBool("Undo", "Ctrl+Z", false, ce->history.idx > 0)) {
                        ce->object_modifier.is_active = false;
                        if (ce->history.idx == ce->history.state_array.length) {
                            hole_editor_push_state(ce);
                            ce->history.idx--;
                        }

                        ce->selected_array.length = 0;
                        ce->hovered_array.length = 0;
                        ce->edit_terrain_model.selected_array.length = 0;
                        ce->edit_terrain_model.hovered_array.length = 0;

                        struct data_stream *stream =
                            &ce->history.state_array.data[ce->history.idx - 1].serialization;
                        hole_deserialize(ce->hole, stream, false);

                        ce->history.idx--;
                    }
                    if (igMenuItemBool("Redo", NULL, false, ce->history.idx + 1 < ce->history.state_array.length)) {
                        ce->object_modifier.is_active = false;
                        ce->selected_array.length = 0;
                        ce->hovered_array.length = 0;
                        ce->edit_terrain_model.selected_array.length = 0;
                        ce->edit_terrain_model.hovered_array.length = 0;

                        struct data_stream *stream =
                            &ce->history.state_array.data[ce->history.idx + 1].serialization;
                        hole_deserialize(ce->hole, stream, false);

                        ce->history.idx++;
                    }
                    if (igMenuItemBool("Copy", "Ctrl+C", false, true)) {
                        if (ce->edit_terrain_model.active) {
                            struct terrain_model *model = ce->edit_terrain_model.model;
                            struct editor_entity_array *selected_array = &ce->edit_terrain_model.selected_array;
                            struct array_vec3 *copied_points = &ce->edit_terrain_model.copied_points;
                            struct array_terrain_model_face *copied_faces = &ce->edit_terrain_model.copied_faces;
                            struct array_int added_points_idxs, new_points_idxs;
                            array_init(&added_points_idxs);
                            array_init(&new_points_idxs);
                            copied_points->length = 0;
                            copied_faces->length = 0;

                            for (int i = 0; i <  selected_array->length; i++) {
                                struct editor_entity ed_entity = selected_array->data[i];
                                if (ed_entity.type == EDITOR_ENTITY_POINT) {
                                    vec3 point = model->points.data[ed_entity.idx];
                                    int new_point_idx;
                                    add_unique_point(&added_points_idxs, &new_points_idxs,
                                            copied_points, ed_entity.idx, point, &new_point_idx);
                                }
                                else if (ed_entity.type == EDITOR_ENTITY_FACE) {
                                    struct terrain_model_face face = model->faces.data[ed_entity.idx];

                                    int new_idx_x;
                                    vec3 px = model->points.data[face.x];
                                    add_unique_point(&added_points_idxs, &new_points_idxs,
                                            copied_points, face.x, px, &new_idx_x);
                                    face.x = new_idx_x;

                                    int new_idx_y;
                                    vec3 py = model->points.data[face.y];
                                    add_unique_point(&added_points_idxs, &new_points_idxs,
                                            copied_points, face.y, py, &new_idx_y);
                                    face.y = new_idx_y;

                                    int new_idx_z;
                                    vec3 pz = model->points.data[face.z];
                                    add_unique_point(&added_points_idxs, &new_points_idxs,
                                            copied_points, face.z, pz, &new_idx_z);
                                    face.z = new_idx_z;

                                    if (face.num_points == 4) {
                                        vec3 pw = model->points.data[face.w];
                                        int new_idx_w;
                                        add_unique_point(&added_points_idxs, &new_points_idxs,
                                                copied_points, face.w, pw, &new_idx_w);
                                        face.w = new_idx_w;
                                    }

                                    array_push(copied_faces, face);
                                }
                            }

                            array_deinit(&added_points_idxs);
                            array_deinit(&new_points_idxs);
                        }
                    }
                    if (igMenuItemBool("Paste", "Ctrl+P", false, true)) {
                        if (ce->edit_terrain_model.active) {
                            hole_editor_push_state(ce);

                            struct terrain_model *model = ce->edit_terrain_model.model;
                            struct editor_entity_array *selected_array = &ce->edit_terrain_model.selected_array;
                            struct array_vec3 *copied_points = &ce->edit_terrain_model.copied_points;
                            struct array_terrain_model_face *copied_faces = &ce->edit_terrain_model.copied_faces;
                            selected_array->length = 0;

                            int start_point_idx = model->points.length;
                            for (int i = 0; i < copied_points->length; i++) {
                                int idx = terrain_model_add_point(model, copied_points->data[i], -1);
                                struct editor_entity entity = make_editor_entity(EDITOR_ENTITY_POINT, idx);
                                if (!editor_entity_array_contains_entity(selected_array, entity)) {
                                    array_push(selected_array, entity);
                                }
                            }

                            for (int i = 0; i < copied_faces->length; i++) {
                                struct terrain_model_face face = copied_faces->data[i];
                                face.x += start_point_idx;
                                face.y += start_point_idx;
                                face.z += start_point_idx;
                                if (face.num_points == 4) {
                                    face.w += start_point_idx;
                                }
                                int idx = terrain_model_add_face(model, face, -1);
                                struct editor_entity entity = make_editor_entity(EDITOR_ENTITY_FACE, idx);
                                if (!editor_entity_array_contains_entity(selected_array, entity)) {
                                    array_push(selected_array, entity);
                                }
                            }
                        }
                    }
                    if (igMenuItemBool("Duplicate", "Ctrl+D", false, true) ||
                            (button_inputs.button_down[SAPP_KEYCODE_LEFT_CONTROL] &&
                             button_inputs.button_clicked[SAPP_KEYCODE_D])) {
                        if (ce->edit_terrain_model.active) {

                        }
                        else {
                            hole_editor_push_state(ce);

                            struct editor_entity_array new_editor_entities;
                            array_init(&new_editor_entities);
                            struct editor_entity_array *selected_array = &ce->selected_array;

                            for (int i = 0; i < selected_array->length; i++) {
                                struct editor_entity editor_entity = selected_array->data[i];
                                if (editor_entity.type == EDITOR_ENTITY_TERRAIN) {
                                    struct terrain_entity *old_entity = 
                                        &ce->hole->terrain_entities.data[editor_entity.idx];
                                    struct terrain_entity new_entity;
                                    terrain_entity_init(&new_entity, old_entity->terrain_model.num_elements,
                                            old_entity->lightmap.width, old_entity->lightmap.height);
                                    terrain_entity_copy(&new_entity, old_entity);
                                    array_push(&ce->hole->terrain_entities, new_entity);

                                    struct editor_entity new_ed_entity =
                                        make_editor_entity(EDITOR_ENTITY_TERRAIN,
                                                ce->hole->terrain_entities.length - 1);
                                    array_push(&new_editor_entities, new_ed_entity);
                                }
                                else if (editor_entity.type == EDITOR_ENTITY_MULTI_TERRAIN_STATIC) {
                                    struct multi_terrain_entity *old_entity = 
                                        &ce->hole->multi_terrain_entities.data[editor_entity.idx];
                                    struct multi_terrain_entity new_entity;
                                    multi_terrain_entity_init(&new_entity, 
                                            old_entity->static_terrain_model.num_elements,
                                            old_entity->moving_terrain_model.num_elements,
                                            old_entity->static_lightmap.width,
                                            old_entity->static_lightmap.height);
                                    multi_terrain_entity_copy(&new_entity, old_entity);
                                    array_push(&ce->hole->multi_terrain_entities, new_entity);

                                    struct editor_entity new_ed_static_entity =
                                        make_editor_entity(EDITOR_ENTITY_MULTI_TERRAIN_STATIC,
                                                ce->hole->multi_terrain_entities.length - 1);
                                    array_push(&new_editor_entities, new_ed_static_entity);
                                    struct editor_entity new_ed_moving_entity =
                                        make_editor_entity(EDITOR_ENTITY_MULTI_TERRAIN_MOVING,
                                                ce->hole->multi_terrain_entities.length - 1);
                                    array_push(&new_editor_entities, new_ed_moving_entity);
                                }
                                else if (editor_entity.type == EDITOR_ENTITY_ENVIRONMENT) {
                                    struct environment_entity *old_entity = 
                                        &ce->hole->environment_entities.data[editor_entity.idx];
                                    struct environment_entity new_entity;
                                    environment_entity_init(&new_entity, old_entity->model->name);
                                    new_entity.is_tiled = old_entity->is_tiled;
                                    new_entity.position = old_entity->position;
                                    new_entity.scale = old_entity->scale;
                                    new_entity.orientation = old_entity->orientation;
                                    array_push(&ce->hole->environment_entities, new_entity);

                                    struct editor_entity new_ed_entity =
                                        make_editor_entity(EDITOR_ENTITY_ENVIRONMENT,
                                                ce->hole->environment_entities.length - 1);
                                    array_push(&new_editor_entities, new_ed_entity);
                                }
                            }

                            selected_array->length = 0;
                            for (int i = 0; i < new_editor_entities.length; i++) {
                                array_push(selected_array, new_editor_entities.data[i]);
                            }

                            array_deinit(&new_editor_entities);
                        }

                    }
                    igEndMenu();
                }
                if (igBeginMenu("Drawing", true)) {
                    if (igBeginMenu("Terrain Entities", true)) {
                        int *draw_type = &ce->drawing.terrain_entities.draw_type;
                        if (igMenuItemBool("Default", NULL, *draw_type == 0, true)) {
                            *draw_type = 0;
                        }
                        if (igMenuItemBool("No AO", NULL, *draw_type == 1, true)) {
                            *draw_type = 1;
                        }
                        if (igMenuItemBool("Only AO", NULL, *draw_type == 2, true)) {
                            *draw_type = 2;
                        }
                        if (igMenuItemBool("Lightmap UVs", NULL, *draw_type == 3, true)) {
                            *draw_type = 3;
                        }
                        if (igMenuItemBool("COR", NULL, *draw_type == 4, true)) {
                            *draw_type = 4;
                        }
                        if (igMenuItemBool("Friction", NULL, *draw_type == 5, true)) {
                            *draw_type = 5;
                        }
                        if (igMenuItemBool("Vel Scale", NULL, *draw_type == 6, true)) {
                            *draw_type = 6;
                        }
                        igEndMenu();
                    }
                    igMenuItemBoolPtr("Helper Lines", NULL, &ce->drawing.helper_lines.active, true);
                    igEndMenu();
                }
                igEndMenuBar();
            }

#if defined(_WIN32)
            if (igTreeNodeStr("Global Illumination")) {
                igCheckbox("Reset Lightmaps", &ce->global_illumination.reset_lightmaps);
                igCheckbox("Create UVs", &ce->global_illumination.create_uvs);
                igInputFloat("Gamma", &ce->global_illumination.gamma, 0.0f, 0.0f, "%0.3f", 0);
                igInputInt("Num Iterations", &ce->global_illumination.num_iterations, 0, 0, 0);
                igInputInt("Num Dilates", &ce->global_illumination.num_dilates, 0, 0, 0);
                igInputInt("Num Smooths", &ce->global_illumination.num_smooths, 0, 0, 0);

                if (igButton("Create Lightmaps", (ImVec2){0, 0})) {
                    if (!lightmap_generator_is_running()) {
                        struct lightmap_generator_data *data = malloc(sizeof(struct lightmap_generator_data));
                        bool reset_lightmaps = ce->global_illumination.reset_lightmaps;
                        bool create_uvs = ce->global_illumination.create_uvs;
                        float gamma = ce->global_illumination.gamma;
                        int num_dilates = ce->global_illumination.num_dilates;
                        int num_smooths = ce->global_illumination.num_smooths;
                        int num_iterations = ce->global_illumination.num_iterations;
                        lightmap_generator_data_init(ce->hole, data, reset_lightmaps, create_uvs, gamma,
                                num_iterations, num_dilates, num_smooths);
                        ce->global_illumination.open_in_progress_dialog = true;
                        ce->global_illumination.do_it = true;
                        ce->global_illumination.data = data;
                        lightmap_generator_start(data);
                    }
                }
                igTreePop(); 
            }
#endif

            if (igTreeNodeStr("Entities")) {
                if (igButton("Create Terrain Entity", (ImVec2){0, 0})) {
                    struct terrain_entity entity;
                    terrain_entity_init(&entity, 24, 256, 256);
                    terrain_model_make_square(&entity.terrain_model);
                    array_push(&ce->hole->terrain_entities, entity);
                }

                if (igButton("Create Multi Terrain Entity", (ImVec2){0, 0})) {
                    struct multi_terrain_entity entity;
                    multi_terrain_entity_init(&entity, 24, 24, 256, 256);
                    terrain_model_make_square(&entity.moving_terrain_model);
                    terrain_model_make_square(&entity.static_terrain_model);
                    array_push(&ce->hole->multi_terrain_entities, entity);
                }

                if (igButton("Create Environment Entity", (ImVec2){0, 0})) {
                    struct environment_entity entity;
                    environment_entity_init(&entity, "sand.terrain_model");
                    array_push(&ce->hole->environment_entities, entity);
                }

                if (igButton("Create Water Entity", (ImVec2){0, 0})) {
                    struct water_entity entity;
                    water_entity_init(&entity, 256, 128, 128);
                    array_push(&ce->hole->water_entities, entity);
                }
                
                if (igButton("Create Camera Zone Entity", (ImVec2){0, 0})) {
                    struct camera_zone_entity entity;
                    entity.position = V3(0.0f, 0.0f, 0.0f);
                    entity.size = V2(1.0f, 1.0f);
                    entity.orientation = QUAT(0.0f, 0.0f, 0.0f, 1.0f);
                    entity.look_towards_cup = false;
                    array_push(&ce->hole->camera_zone_entities, entity);
                }

                igCheckbox("Multi Terrain Entities Moving", &ce->multi_terrain_entities.is_moving);
                igCheckbox("Modify Camera Zone Entities", &ce->camera_zone_entities.can_modify);

                for (int i = 0; i < ce->hole->terrain_entities.length; i++) {
                    char label[512];
                    sprintf(label, "Terrain: %d", i);

                    struct editor_entity entity = make_editor_entity(EDITOR_ENTITY_TERRAIN, i);
                    bool selected = editor_entity_array_contains_entity(&ce->selected_array, entity);
                    if (igSelectableBool(label, selected, 0, (ImVec2){0, 0})) {
                        ce->object_modifier.is_active = false;
                        ce->selected_array.length = 0;
                        array_push(&ce->selected_array, entity);
                    }
                }

                for (int i = 0; i < ce->hole->multi_terrain_entities.length; i++) {
                    char label[512];
                    sprintf(label, "Moving Terrain (Moving Part): %d", i);

                    struct editor_entity entity = make_editor_entity(EDITOR_ENTITY_MULTI_TERRAIN_MOVING, i);
                    bool selected = editor_entity_array_contains_entity(&ce->selected_array, entity);
                    if (igSelectableBool(label, selected, 0, (ImVec2){0, 0})) {
                        ce->object_modifier.is_active = false;
                        ce->selected_array.length = 0;
                        array_push(&ce->selected_array, entity);
                    }
                }

                for (int i = 0; i < ce->hole->multi_terrain_entities.length; i++) {
                    char label[512];
                    sprintf(label, "Moving Terrain (Static Part): %d", i);

                    struct editor_entity entity = make_editor_entity(EDITOR_ENTITY_MULTI_TERRAIN_STATIC, i);
                    bool selected = editor_entity_array_contains_entity(&ce->selected_array, entity);
                    if (igSelectableBool(label, selected, 0, (ImVec2){0, 0})) {
                        ce->object_modifier.is_active = false;
                        ce->selected_array.length = 0;
                        array_push(&ce->selected_array, entity);
                    }
                }

                {
                    struct cup_entity *cup = &ce->hole->cup_entity;
                    char label[512];
                    sprintf(label, "Hole: <%.5f, %.5f, %.5f>", 
                            cup->position.x, cup->position.y, cup->position.z);

                    struct editor_entity entity = make_editor_entity(EDITOR_ENTITY_CUP, 0);
                    bool selected = editor_entity_array_contains_entity(&ce->selected_array, entity);
                    if (igSelectableBool(label, selected, 0, (ImVec2){0, 0})) {
                        ce->object_modifier.is_active = false;
                        ce->selected_array.length = 0;
                        array_push(&ce->selected_array, entity);
                    }
                }

                {
                    struct beginning_camera_animation_entity *cam_anim =
                        &ce->hole->beginning_camera_animation_entity;
                    char label[512];
                    sprintf(label, "Beginning Camera Animation: <%.5f, %.5f, %.5f>", 
                            cam_anim->start_position.x, cam_anim->start_position.y, cam_anim->start_position.z);

                    struct editor_entity entity = make_editor_entity(EDITOR_ENTITY_BEGINNING_CAMERA_ANIMATION, 0);
                    bool selected = editor_entity_array_contains_entity(&ce->selected_array, entity);
                    if (igSelectableBool(label, selected, 0, (ImVec2){0, 0})) {
                        ce->object_modifier.is_active = false;
                        ce->selected_array.length = 0;
                        array_push(&ce->selected_array, entity);
                    }
                }

                {
                    struct ball_start_entity *ball_start = &ce->hole->ball_start_entity;
                    char label[512];
                    sprintf(label, "Ball Start: <%.5f, %.5f, %.5f>", 
                            ball_start->position.x, ball_start->position.y, ball_start->position.z);

                    struct editor_entity entity = make_editor_entity(EDITOR_ENTITY_BALL_START, 0);
                    bool selected = editor_entity_array_contains_entity(&ce->selected_array, entity);
                    if (igSelectableBool(label, selected, 0, (ImVec2){0, 0})) {
                        ce->object_modifier.is_active = false;
                        ce->selected_array.length = 0;
                        array_push(&ce->selected_array, entity);
                    }
                }

                for (int i = 0; i < ce->hole->camera_zone_entities.length; i++) {
                    char label[512];
                    sprintf(label, "Camera Zone: %d", i);

                    struct editor_entity entity = make_editor_entity(EDITOR_ENTITY_CAMERA_ZONE, i);
                    bool selected = editor_entity_array_contains_entity(&ce->selected_array, entity);
                    if (igSelectableBool(label, selected, 0, (ImVec2){0, 0})) {
                        ce->object_modifier.is_active = false;
                        ce->selected_array.length = 0;
                        array_push(&ce->selected_array, entity);
                    }
                }

                for (int i = 0; i < ce->hole->environment_entities.length; i++) {
                    char label[512];
                    sprintf(label, "Environment: %d", i);

                    struct editor_entity entity = make_editor_entity(EDITOR_ENTITY_ENVIRONMENT, i);
                    bool selected = editor_entity_array_contains_entity(&ce->selected_array, entity);
                    if (igSelectableBool(label, selected, 0, (ImVec2){0, 0})) {
                        ce->object_modifier.is_active = false;
                        ce->selected_array.length = 0;
                        array_push(&ce->selected_array, entity);
                    }
                }

                for (int i = 0; i < ce->hole->water_entities.length; i++) {
                    char label[512];
                    sprintf(label, "Water: %d", i);

                    struct editor_entity entity = make_editor_entity(EDITOR_ENTITY_WATER, i);
                    bool selected = editor_entity_array_contains_entity(&ce->selected_array, entity);
                    if (igSelectableBool(label, selected, 0, (ImVec2){0, 0})) {
                        ce->object_modifier.is_active = false;
                        ce->selected_array.length = 0;
                        array_push(&ce->selected_array, entity);
                    }
                }

                igTreePop();
            }

            igEnd();
        }

        if (ce->edit_terrain_model.active) {
            struct terrain_model *model = ce->edit_terrain_model.model;
            struct editor_entity_array *selected_array = &ce->edit_terrain_model.selected_array;
            struct editor_entity_array *hovered_array = &ce->edit_terrain_model.hovered_array;

            static bool edit_terrain_model_window_open = true;
            igSetNextWindowSize((ImVec2){300, 500}, ImGuiCond_FirstUseEver);
            igSetNextWindowPos((ImVec2){5, 310}, ImGuiCond_FirstUseEver, (ImVec2){0, 0});
            igBegin("Terrain Model Editor", &edit_terrain_model_window_open, 0);
            if (igButton("Close Model Editor", (ImVec2){0, 0})) {
                ce->edit_terrain_model.active = false;
                ce->object_modifier.is_active = false;
            }
            if (igButton("Generate Point", (ImVec2){0, 0})) {
                hole_editor_push_state(ce);
                ce->object_modifier.is_active = false;
                selected_array->length = 0;
                int idx = terrain_model_add_point(model, V3(0.0f, 0.0f, 0.0f), -1);
                array_push(selected_array, make_editor_entity(EDITOR_ENTITY_POINT, idx));
            }
            igCheckbox("Select Points", &ce->edit_terrain_model.points_selectable);
            igCheckbox("Select Faces", &ce->edit_terrain_model.faces_selectable);
            if (igTreeNodeStr("Materials")) {
                for (int i = 0; i < MAX_NUM_TERRAIN_MODEL_MATERIALS; i++) {
                    vec3 *c0 = &model->materials[i].color0;
                    vec3 *c1 = &model->materials[i].color1;
                    igPushIDInt(i);
                    igColorEdit3("Color0", (float*) c0, ImGuiColorEditFlags_None);
                    igColorEdit3("Color1", (float*) c1, ImGuiColorEditFlags_None);
                    igPopID();
                }
                igTreePop();
            }
            if (igTreeNodeStr("Generator Scripts")) {
                if (ce->sc_error[0]) {
                    igTextWrapped(ce->sc_error); 
                }
                hole_editor_update_model_generator_scripts(ce, false);
                struct array_model_generator_script *scripts = &ce->scripts;
                for (int i = 0; i < scripts->length; i++) {
                    struct model_generator_script *script = &scripts->data[i];
                    bool is_model_generator = strcmp(script->file.name, model->generator_name) == 0;

                    if (is_model_generator) {
                        igPushStyleColorU32(ImGuiCol_Text, 0xFF0000FF);
                    }
                    if (igTreeNodeStr(script->file.name)) {
                        igPushItemWidth(100.0f);
                        for (int i = 0; i < script->params.length; i++) {
                            char *param = script->params.data[i];
                            float *value = &script->values.data[i];
                            if (is_model_generator) {
                                value = map_get(&model->generator_params, param);
                                if (!value) {
                                    map_set(&model->generator_params, param, 0.0f);
                                    value = map_get(&model->generator_params, param);
                                }
                            }
                            igInputFloat(param, value, 0.0f, 0.0f, "%0.2f", 0);
                            if (is_model_generator) {
                                script->values.data[i] = *value;
                            }
                        }
                        igPopItemWidth();

                        if (igButton("Run", (ImVec2){0, 0})) {
                            hole_editor_push_state(ce);

                            selected_array->length = 0;
                            hovered_array->length = 0;
                            model->points.length = 0;
                            model->faces.length = 0;
                            map_deinit(&model->generator_params);
                            map_init(&model->generator_params);
                            strncpy(model->generator_name, script->file.name, MFILE_MAX_NAME + 1);
                            model->generator_name[MFILE_MAX_NAME] = 0;
                            ce->sc_error[0] = 0;

                            s7_scheme *sc = ce->sc_state;
                            s7_pointer sc_env = script->sc_env;
                            s7_pointer sc_args = s7_nil(sc);
                            for (int i = script->params.length - 1; i >= 0; i--) {
                                float value = script->values.data[i];
                                sc_args = s7_cons(sc, s7_make_real(sc, value), sc_args);
                                map_set(&model->generator_params, script->params.data[i], value);
                            }
                            s7_pointer sc_code = s7_cons(sc, s7_make_symbol(sc, "generate"), sc_args);
                            s7_int loc = s7_gc_protect(sc, sc_code);
                            ce->sc_start_time = stm_now();
                            s7_set_begin_hook(sc, sc_begin_hook);
                            s7_eval(sc, sc_code, sc_env);
                            s7_set_begin_hook(sc, NULL);
                            s7_gc_unprotect_at(sc, loc);
                        }

                        igTreePop();
                    }
                    if (is_model_generator) {
                        igPopStyleColor(1);
                    }
                }
                igTreePop();
            }
            igEnd();

            if (selected_array->length > 0) {
                static bool selected_terrain_model_entity_window = true;
                igSetNextWindowSize((ImVec2){300, 500}, ImGuiCond_FirstUseEver);
                igSetNextWindowPos((ImVec2){310, 5}, ImGuiCond_FirstUseEver, (ImVec2){0, 0});
                igBegin("Selected Terrain Model Entity", &selected_terrain_model_entity_window, 0);

                bool only_points_selected = true;
                for (int i = 0; i < selected_array->length; i++) {
                    struct editor_entity entity = selected_array->data[i];
                    if (entity.type != EDITOR_ENTITY_POINT) {
                        only_points_selected = false;
                        break;
                    }
                }

                if (only_points_selected && selected_array->length == 3) {
                    if (igButton("Generate Face", (ImVec2){0, 0})) {
                        hole_editor_push_state(ce);

                        int num_points = 3;
                        int mat_idx = 0;
                        int smooth_normal = 0;
                        int x = selected_array->data[0].idx;
                        int y = selected_array->data[1].idx;
                        int z = selected_array->data[2].idx;
                        int w = 0;
                        vec2 tc0 = V2(0.0f, 0.0f);
                        vec2 tc1 = V2(0.0f, 0.0f);
                        vec2 tc2 = V2(0.0f, 0.0f);
                        vec2 tc3 = V2(0.0f, 0.0f);
                        float cor = 1.0f;
                        float friction = 0.0f;
                        float vel_scale = 1.0f;
                        struct terrain_model_face face = 
                            create_terrain_model_face(num_points, mat_idx, smooth_normal, 
                                    x, y, z, w, tc0, tc1, tc2, tc3, 1.0f, cor, friction, vel_scale, 
                                    AUTO_TEXTURE_NONE);
                        vec3 p0 = terrain_model_get_point(model, face.x);
                        vec3 p1 = terrain_model_get_point(model, face.y);
                        vec3 p2 = terrain_model_get_point(model, face.z);
                        vec3 n = vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0));
                        if (vec3_dot(n, button_inputs.mouse_ray_dir) > 0) {
                            int temp = face.x;
                            face.x = face.y;
                            face.y = temp;
                        }
                        int idx = terrain_model_add_face(model, face, -1);
                        selected_array->length = 0;
                        hovered_array->length = 0;
                        array_push(selected_array, make_editor_entity(EDITOR_ENTITY_FACE, idx));
                    }
                }

                if (only_points_selected && selected_array->length == 4) {
                    if (igButton("Generate Face", (ImVec2){0, 0})) {
                        hole_editor_push_state(ce);

                        int num_points = 4;
                        int mat_idx = 0;
                        int smooth_normal = 0;
                        int x = selected_array->data[0].idx;
                        int y = selected_array->data[1].idx;
                        int z = selected_array->data[2].idx;
                        int w = selected_array->data[3].idx;
                        vec2 tc0 = V2(0.0f, 0.0f);
                        vec2 tc1 = V2(0.0f, 0.0f);
                        vec2 tc2 = V2(0.0f, 0.0f);
                        vec2 tc3 = V2(0.0f, 0.0f);
                        float cor = 1.0f;
                        float friction = 0.0f;
                        float vel_scale = 1.0f;
                        struct terrain_model_face face = 
                            create_terrain_model_face(num_points, mat_idx, smooth_normal, 
                                    x, y, z, w, tc0, tc1, tc2, tc3, 1.0f, cor, friction, vel_scale, 
                                    AUTO_TEXTURE_NONE);
                        vec3 p0 = terrain_model_get_point(model, face.x);
                        vec3 p1 = terrain_model_get_point(model, face.y);
                        vec3 p2 = terrain_model_get_point(model, face.z);
                        vec3 n = vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0));
                        if (vec3_dot(n, button_inputs.mouse_ray_dir) > 0) {
                            int temp_x = face.x;
                            int temp_y = face.y;
                            int temp_z = face.z;
                            int temp_w = face.w;
                            face.x = temp_w;
                            face.y = temp_z;
                            face.z = temp_y;
                            face.w = temp_x;
                        }
                        int idx = terrain_model_add_face(model, face, -1);
                        selected_array->length = 0;
                        hovered_array->length = 0;
                        array_push(selected_array, make_editor_entity(EDITOR_ENTITY_FACE, idx));
                    }
                }

                if (only_points_selected && selected_array->length >= 3) {
                    if (igButton("Generate Border", (ImVec2){0, 0})) {
                        hole_editor_push_state(ce);

                        struct array_vec2 polygon_points;
                        array_init(&polygon_points);

                        for (int i = 0; i < selected_array->length; i++) {
                            vec3 point = model->points.data[selected_array->data[i].idx];
                            array_push(&polygon_points, V2(point.z, point.x));
                        }

                        struct array_int point_idxs;
                        array_init(&point_idxs);

                        float y_bottom = vec3_apply_mat4(V3(0.0f, -1.0f, 0.0f), 1.0f,
                                mat4_inverse(ce->edit_terrain_model.model_mat)).y;

                        for (int i = 0; i < polygon_points.length; i++) {
                            float y = model->points.data[selected_array->data[i].idx].y;

                            vec2 p0;
                            if (i == 0) p0 = polygon_points.data[polygon_points.length - 1];
                            else p0 = polygon_points.data[i - 1];

                            vec2 p1 = polygon_points.data[i];

                            vec2 p2;
                            if (i == polygon_points.length - 1) p2 = polygon_points.data[0];
                            else p2 = polygon_points.data[i + 1];

                            vec2 dir0 = vec2_normalize(vec2_sub(p0, p1));
                            vec2 dir1 = vec2_normalize(vec2_sub(p2, p1));
                            vec2 dir = vec2_normalize(vec2_add(dir0, dir1));
                            if (vec2_length(dir) < 0.0001f) {
                                dir = vec2_rotate(dir0, 0.5f * MF_PI);
                            }

                            vec2 new_point = vec2_add(p1, vec2_scale(dir, 0.01f)); 
                            if (vec2_point_in_polygon(new_point, polygon_points.length, polygon_points.data)) {
                                dir = vec2_scale(dir, -1.0f);
                            }
                            float l = 0.5f / sinf(acosf(vec2_dot(dir, dir0)));
                            new_point = vec2_add(p1, vec2_scale(dir, l));

                            int i0 = selected_array->data[i].idx;
                            int i1 = terrain_model_add_point(model, V3(p1.y, y + 0.25f, p1.x), -1);
                            int i2 = terrain_model_add_point(model, V3(new_point.y, y + 0.25f, new_point.x), -1);
                            int i3 = terrain_model_add_point(model, V3(new_point.y, y_bottom, new_point.x), -1);

                            array_push(&point_idxs, i0);
                            array_push(&point_idxs, i1);
                            array_push(&point_idxs, i2);
                            array_push(&point_idxs, i3);
                        }

                        float l0_start = 0.0f;
                        for (int i = 0; i < polygon_points.length; i++) {
                            int i1 = i + 1;
                            if (i == polygon_points.length - 1) {
                                i1 = 0;
                            }

                            struct terrain_model_face face;
                            int num_points = 4;
                            int mat_idx = 1;
                            int smooth_normal = 0;
                            int x = point_idxs.data[4*i + 0];
                            int y = point_idxs.data[4*i + 1];
                            int z = point_idxs.data[4*i1 + 1];
                            int w = point_idxs.data[4*i1 + 0];
                            vec3 px = model->points.data[x];
                            vec3 py = model->points.data[y];
                            vec3 pz = model->points.data[z];
                            vec3 pw = model->points.data[w];
                            vec3 dir = vec3_normalize(vec3_sub(px, pw));
                            float l0 = vec3_distance(px, pw);
                            vec2 tc0 = V2(0.0f, l0_start);
                            vec2 tc1 = V2(1.0f, l0_start);
                            vec2 tc2 = V2(1.0f, l0_start + l0);
                            vec2 tc3 = V2(0.0f, l0_start + l0);
                            float texture_coord_scale = 0.25f;
                            float cor = 1.0f;
                            float friction = 1.0f;
                            float vel_scale = 1.0f;

                            face = create_terrain_model_face(num_points, mat_idx, smooth_normal,
                                    x, y, z, w, tc0, tc1, tc2, tc3, texture_coord_scale,
                                    cor, friction, vel_scale, AUTO_TEXTURE_NONE);
                            terrain_model_add_face(model, face, -1);

                            x = point_idxs.data[4*i+1];
                            y = point_idxs.data[4*i+2];
                            z = point_idxs.data[4*i1+2];
                            w = point_idxs.data[4*i1+1];
                            px = model->points.data[x];
                            py = model->points.data[y];
                            pz = model->points.data[z];
                            pw = model->points.data[w];
                            float l1_start = l0_start + vec3_dot(dir, vec3_sub(px, py));
                            float l1 = vec3_distance(py, pz);
                            tc0 = V2(0.0f, l0_start);
                            tc1 = V2(1.0f, l1_start);
                            tc2 = V2(1.0f, l1_start + l1);
                            tc3 = V2(0.0f, l0_start + l0);

                            face = create_terrain_model_face(num_points, mat_idx, smooth_normal,
                                    x, y, z, w, tc0, tc1, tc2, tc3, texture_coord_scale,
                                    cor, friction, vel_scale, AUTO_TEXTURE_NONE);
                            terrain_model_add_face(model, face, -1);

                            x = point_idxs.data[4*i+2];
                            y = point_idxs.data[4*i+3];
                            z = point_idxs.data[4*i1+3];
                            w = point_idxs.data[4*i1+2];
                            tc0 = V2(0.0f, l1_start);
                            tc1 = V2(1.0f, l1_start);
                            tc2 = V2(1.0f, l1_start + l1);
                            tc3 = V2(0.0f, l1_start + l1);

                            face = create_terrain_model_face(num_points, mat_idx, smooth_normal,
                                    x, y, z, w, tc0, tc1, tc2, tc3, texture_coord_scale,
                                    cor, friction, vel_scale, AUTO_TEXTURE_NONE);
                            terrain_model_add_face(model, face, -1);

                            l0_start += l0;
                        }

                        array_deinit(&point_idxs);
                        array_deinit(&polygon_points);
                    }
                }

                if (igButton("Delete", (ImVec2){0, 0})) {
                    hole_editor_push_state(ce);

                    struct terrain_model *model = ce->edit_terrain_model.model;
                    struct editor_entity_array *selected_array = &ce->edit_terrain_model.selected_array;
                    struct editor_entity_array *hovered_array = &ce->edit_terrain_model.hovered_array;

                    struct editor_entity_array delete_entities;
                    array_init(&delete_entities);
                    for (int i = 0; i < selected_array->length; i++) {
                        struct editor_entity entity = selected_array->data[i];
                        if (entity.type == EDITOR_ENTITY_POINT) {
                            if (!editor_entity_array_contains_entity(&delete_entities, entity)) {
                                array_push(&delete_entities, entity);
                            }

                            // Delete any face connected to the point
                            for (int j = 0; j < model->faces.length; j++) {
                                struct terrain_model_face face = model->faces.data[j];
                                assert(face.num_points == 3 || face.num_points == 4);
                                if (face.x == entity.idx || face.y == entity.idx || face.z == entity.idx ||
                                        (face.num_points == 4 && face.w == entity.idx)) {
                                    struct editor_entity face_entity = make_editor_entity(EDITOR_ENTITY_FACE, j);
                                    if (!editor_entity_array_contains_entity(&delete_entities, face_entity)) {
                                        array_push(&delete_entities, face_entity);
                                    }
                                }
                            }
                        }
                        else if (entity.type == EDITOR_ENTITY_FACE) {
                            if (!editor_entity_array_contains_entity(&delete_entities, entity)) {
                                array_push(&delete_entities, entity);
                            }
                        }
                        else {
                            assert(false);
                        }
                    }
                    selected_array->length = 0;
                    hovered_array->length = 0;

                    // Have to delete entities from high idx to low idx
                    entity_array_sort_idxs(&delete_entities);

                    // Delete faces first
                    for (int i = delete_entities.length - 1; i >= 0; i--) {
                        struct editor_entity entity = delete_entities.data[i]; 
                        if (entity.type == EDITOR_ENTITY_POINT) {
                        }
                        else if (entity.type == EDITOR_ENTITY_FACE) {
                            terrain_model_delete_face(model, entity.idx);
                        }
                        else {
                            assert(false);
                        }
                    }

                    // Delete points second
                    for (int i = delete_entities.length - 1; i >= 0; i--) {
                        struct editor_entity entity = delete_entities.data[i]; 
                        if (entity.type == EDITOR_ENTITY_POINT) {
                            terrain_model_delete_point(model, entity.idx);
                        }
                        else if (entity.type == EDITOR_ENTITY_FACE) {
                        }
                        else {
                            assert(false);
                        }
                    }

                    array_deinit(&delete_entities);
                }

                {
                    int set_faces_mat_idx = -1;

                    if (igButton("MAT1", (ImVec2){0, 0})) {
                        set_faces_mat_idx = 0;
                    }
                    igSameLine(0.0f, -1.0f);
                    if (igButton("MAT2", (ImVec2){0, 0})) {
                        set_faces_mat_idx = 1;
                    }
                    igSameLine(0.0f, -1.0f);
                    if (igButton("MAT3", (ImVec2){0, 0})) {
                        set_faces_mat_idx = 2;
                    }
                    igSameLine(0.0f, -1.0f);
                    if (igButton("MAT4", (ImVec2){0, 0})) {
                        set_faces_mat_idx = 3;
                    }
                    igSameLine(0.0f, -1.0f);
                    if (igButton("MAT5", (ImVec2){0, 0})) {
                        set_faces_mat_idx = 4;
                    }

                    {
                        float *cor = &ce->edit_terrain_model.selected_face_cor;
                        igInputFloat("COR", cor, 0.0f, 0.0f, "%0.5f", 0);
                        if (igButton("Set All##COR", (ImVec2){0, 0})) {
                            hole_editor_push_state(ce);
                            for (int i = 0; i < selected_array->length; i++) {
                                struct editor_entity entity = selected_array->data[i];
                                if (selected_array->data[i].type == EDITOR_ENTITY_FACE) {
                                    struct terrain_model_face *face = &model->faces.data[entity.idx];
                                    face->cor = *cor;
                                }
                            }
                        }
                    }

                    {
                        float *friction = &ce->edit_terrain_model.selected_face_friction;
                        igInputFloat("Friction", friction, 0.0f, 0.0f, "%0.5f", 0);
                        if (igButton("Set All##Friction", (ImVec2){0, 0})) {
                            hole_editor_push_state(ce);
                            for (int i = 0; i < selected_array->length; i++) {
                                struct editor_entity entity = selected_array->data[i];
                                if (selected_array->data[i].type == EDITOR_ENTITY_FACE) {
                                    struct terrain_model_face *face = &model->faces.data[entity.idx];
                                    face->friction = *friction;
                                }
                            }
                        }
                    }

                    {
                        float *vel_scale = &ce->edit_terrain_model.selected_face_vel_scale;
                        igInputFloat("Vel Scale", vel_scale, 0.0f, 0.0f, "%0.5f", 0);
                        if (igButton("Set All##Vel Scale", (ImVec2){0, 0})) {
                            hole_editor_push_state(ce);
                            for (int i = 0; i < selected_array->length; i++) {
                                struct editor_entity entity = selected_array->data[i];
                                if (selected_array->data[i].type == EDITOR_ENTITY_FACE) {
                                    struct terrain_model_face *face = &model->faces.data[entity.idx];
                                    face->vel_scale = *vel_scale;
                                }
                            }
                        }
                    }

                    if (set_faces_mat_idx >= 0) {
                        hole_editor_push_state(ce);
                        for (int i = 0; i < selected_array->length; i++) {
                            struct editor_entity entity = selected_array->data[i];
                            if (selected_array->data[i].type == EDITOR_ENTITY_FACE) {
                                struct terrain_model_face *face = &model->faces.data[entity.idx];
                                face->mat_idx = set_faces_mat_idx;
                            }
                        }
                    }
                }

                for (int i = 0; i < selected_array->length; i++) {
                    igPushIDInt(i);

                    struct editor_entity entity = selected_array->data[i];
                    if (entity.type == EDITOR_ENTITY_POINT) {
                        vec3 p = model->points.data[entity.idx];
                        igText("Point: <%0.5f, %0.5f, %0.5f>", p.x, p.y, p.z);
                    }
                    else if (entity.type == EDITOR_ENTITY_FACE) {
                        struct terrain_model_face *face = &model->faces.data[entity.idx];
                        float *cor = &face->cor;
                        float *friction = &face->friction;
                        float *vel_scale = &face->vel_scale;
                        float *texture_coord_scale = &face->texture_coord_scale;

                        vec3 *p0 = &model->points.data[face->x];
                        vec2 *tc0 = &face->texture_coords[0];

                        vec3 *p1 = &model->points.data[face->y];
                        vec2 *tc1 = &face->texture_coords[1];

                        vec3 *p2 = &model->points.data[face->z];
                        vec2 *tc2 = &face->texture_coords[2];

                        vec3 *p3 = NULL;
                        vec2 *tc3 = NULL;
                        if (face->num_points == 3) {
                        }
                        else if (face->num_points == 4) {
                            p3 = &model->points.data[face->w];
                            tc3 = &face->texture_coords[3];
                        }
                        else {
                            assert(false);
                        }

                        if (igTreeNodeStr("Face")) {
                            igText("Mat: %d", face->mat_idx);

                            if (igTreeNodeStr("Auto Texture")) {
                                bool auto_texture_none = face->auto_texture == AUTO_TEXTURE_NONE;
                                if (igRadioButtonBool("None", auto_texture_none)) {
                                    face->auto_texture = AUTO_TEXTURE_NONE;
                                }

                                bool auto_texture_wood_out = face->auto_texture == AUTO_TEXTURE_WOOD_OUT;
                                if (igRadioButtonBool("Wood Out", auto_texture_wood_out)) {
                                    face->auto_texture = AUTO_TEXTURE_WOOD_OUT;
                                }

                                bool auto_texture_wood_in = face->auto_texture == AUTO_TEXTURE_WOOD_IN;
                                if (igRadioButtonBool("Wood In", auto_texture_wood_in)) {
                                    face->auto_texture = AUTO_TEXTURE_WOOD_IN;
                                }

                                bool auto_texture_wood_top = face->auto_texture == AUTO_TEXTURE_WOOD_TOP;
                                if (igRadioButtonBool("Wood Top", auto_texture_wood_top)) {
                                    face->auto_texture = AUTO_TEXTURE_WOOD_TOP;
                                }

                                bool auto_grass = face->auto_texture == AUTO_TEXTURE_GRASS;
                                if (igRadioButtonBool("Grass", auto_grass)) {
                                    face->auto_texture = AUTO_TEXTURE_GRASS;
                                }

                                igTreePop();
                            }

                            bool smooth_normal = face->smooth_normal;
                            if (igCheckbox("Smooth Normal", &smooth_normal)) {
                                hole_editor_push_state(ce);
                                face->smooth_normal = smooth_normal;
                            }

                            float new_cor = *cor;
                            if (igInputFloat("COR", &new_cor, 0.0f, 0.0f, "%0.5f", 0)) {
                                hole_editor_push_state(ce);
                                *cor = new_cor;
                            }

                            float new_friction = *friction;
                            if (igInputFloat("Friction", &new_friction, 0.0f, 0.0f, "%0.5f", 0)) {
                                hole_editor_push_state(ce);
                                *friction = new_friction;
                            }

                            float new_vel_scale = *vel_scale;
                            if (igInputFloat("Vel Scale", &new_vel_scale, 0.0f, 0.0f, "%0.5f", 0)) {
                                hole_editor_push_state(ce);
                                *vel_scale = new_vel_scale;
                            }

                            float new_texture_coord_scale = *texture_coord_scale;
                            if (igInputFloat("Texture Coord Scale", &new_texture_coord_scale, 
                                        0.0f, 0.0f, "%0.5f", 0)) {
                                hole_editor_push_state(ce);
                                *texture_coord_scale = new_texture_coord_scale;
                            }

                            ImVec4 c0 = (ImVec4){0.0f, 0.0f, 1.0f, 1.0f};
                            igTextColored(c0, "P0: <%0.5f, %0.5f, %0.5f>", p0->x, p0->y, p0->z);

                            vec2 new_tc0 = *tc0;
                            if (igInputFloat2("TC0", (float*) &new_tc0, "%0.5f", 0)) {
                                hole_editor_push_state(ce);
                                *tc0 = new_tc0;
                            }

                            ImVec4 c1 = (ImVec4){0.0f, 1.0f, 0.0f, 1.0f};
                            igTextColored(c1, "P1: <%0.5f, %0.5f, %0.5f>", p1->x, p1->y, p1->z);

                            vec2 new_tc1 = *tc1;
                            if (igInputFloat2("TC1", (float*) &new_tc1, "%0.5f", 0)) {
                                hole_editor_push_state(ce);
                                *tc1 = new_tc1;
                            }

                            ImVec4 c2 = (ImVec4){1.0f, 0.0f, 0.0f, 1.0f};
                            igTextColored(c2, "P2: <%0.5f, %0.5f, %0.5f>", p2->x, p2->y, p2->z);

                            vec2 new_tc2 = *tc2;
                            if (igInputFloat2("TC2", (float*) &new_tc2, "%0.5f", 0)) {
                                hole_editor_push_state(ce);
                                *tc2 = new_tc2;
                            }

                            if (p3) {
                                ImVec4 c3 = (ImVec4){1.0f, 1.0f, 0.0f, 1.0f};
                                igTextColored(c3, "P3: <%0.5f, %0.5f, %0.5f>", p3->x, p3->y, p3->z);

                                vec2 new_tc3 = *tc3;
                                if (igInputFloat2("TC3", (float*) &new_tc3, "%0.5f", 0)) {
                                    hole_editor_push_state(ce);
                                    *tc3 = new_tc3;
                                }
                            }

                            {
                                float width = 150;
                                float height = 100;
                                igBeginChildStr("Face Drawing", (ImVec2){width, height}, true, 0);

                                ImGuiWindow *window = igGetCurrentWindow();

                                mat4 model_mat = ce->edit_terrain_model.model_mat;
                                vec3 wp[4];
                                wp[0] = renderer_world_to_screen(renderer, vec3_apply_mat4(*p0, 1.0f, model_mat));
                                wp[1] = renderer_world_to_screen(renderer, vec3_apply_mat4(*p1, 1.0f, model_mat));
                                wp[2] = renderer_world_to_screen(renderer, vec3_apply_mat4(*p2, 1.0f, model_mat));
                                if (p3) {
                                    wp[3] = renderer_world_to_screen(renderer, vec3_apply_mat4(*p3, 1.0f, model_mat));
                                }
                                vec2 min = V2(FLT_MAX, FLT_MAX); 
                                vec2 max = V2(-FLT_MAX, -FLT_MAX); 

                                for (int i = 0; i < 4; i++) {
                                    if (i == 3 && !p3) {
                                        continue;
                                    }
                                    if (wp[i].x > max.x) max.x = wp[i].x;
                                    if (wp[i].x < min.x) min.x = wp[i].x;
                                    if (wp[i].y > max.y) max.y = wp[i].y;
                                    if (wp[i].y < min.y) min.y = wp[i].y;
                                }

                                for (int i = 0; i < 4; i++) {
                                    if (i == 3 && !p3) {
                                        continue;
                                    }
                                    wp[i].x = 10.0f + (width - 20.0f) * (wp[i].x - min.x) / (max.x - min.x);
                                    wp[i].y = 10.0f + (height - 20.0f) * (wp[i].y - min.y) / (max.y - min.y);
                                }

                                ImVec2 pos = window->Pos;
                                ImVec2 vp0 = (ImVec2){pos.x + wp[0].x, pos.y + wp[0].y};
                                ImVec2 vp1 = (ImVec2){pos.x + wp[1].x, pos.y + wp[1].y};
                                ImVec2 vp2 = (ImVec2){pos.x + wp[2].x, pos.y + wp[2].y};
                                ImVec2 vp3;
                                if (p3) {
                                    vp3 = (ImVec2){pos.x + wp[3].x, pos.y + wp[3].y};
                                }
                                ImDrawList_AddLine(window->DrawList, vp0, vp1, 0xFFFFFFFF, 1.0f);
                                ImDrawList_AddLine(window->DrawList, vp1, vp2, 0xFFFFFFFF, 1.0f);
                                if (p3) {
                                    ImDrawList_AddLine(window->DrawList, vp2, vp3, 0xFFFFFFFF, 1.0f);
                                    ImDrawList_AddLine(window->DrawList, vp3, vp0, 0xFFFFFFFF, 1.0f);
                                }
                                else {
                                    ImDrawList_AddLine(window->DrawList, vp2, vp0, 0xFFFFFFFF, 1.0f);
                                }
                                ImDrawList_AddCircleFilled(window->DrawList, vp0, 5.0f, 0xFFFF0000, 10);
                                ImDrawList_AddCircleFilled(window->DrawList, vp1, 5.0f, 0xFF00FF00, 10);
                                ImDrawList_AddCircleFilled(window->DrawList, vp2, 5.0f, 0xFF0000FF, 10);
                                if (p3) {
                                    ImDrawList_AddCircleFilled(window->DrawList, vp3, 5.0f, 0xFF00FFFF, 10);
                                }

                                igEndChild();
                            }

                            igTreePop();
                        }
                    }
                    else {
                        assert(false);
                    }

                    igPopID();
                }

                igEnd();
            }
        }
        else  {
            if (ce->selected_array.length > 0) {
                static bool selected_entity_window_open = true;
                igSetNextWindowSize((ImVec2){300, 500}, ImGuiCond_FirstUseEver);
                igSetNextWindowPos((ImVec2){5, 310}, ImGuiCond_FirstUseEver, (ImVec2){0, 0});
                igBegin("Selected Entities", &selected_entity_window_open, 0);
                struct editor_entity entity = ce->selected_array.data[0];
                if (entity.type == EDITOR_ENTITY_TERRAIN) {
                    struct terrain_entity *terrain = &ce->hole->terrain_entities.data[entity.idx];
                    igText("Type: TERRAIN");
                    igText("Position: <%0.5f, %0.5f, %0.5f>",
                            terrain->position.x, terrain->position.y, terrain->position.z);
                    igText("Scale: <%.5f, %.5f, %.5f>",
                            terrain->scale.x, terrain->scale.y, terrain->scale.z);
                    //igText("Y Rotation: %.5f", terrain->y_rotation);
                    if (igTreeNodeStr("Lightmap")) {
                        struct lightmap *lightmap = &terrain->lightmap;
                        igText("Width: %d", lightmap->width);
                        igText("Height: %d", lightmap->height);
                        igInputInt("New Width", &ce->lightmap_update.new_height, 0, 0, 0);
                        igInputInt("New Height", &ce->lightmap_update.new_width, 0, 0, 0);
                        if (igButton("Update Lightmap", (ImVec2){0, 0})) {
                            ce->lightmap_update.do_it = true;
                            ce->lightmap_update.new_num_images = 1;
                            ce->lightmap_update.lightmap = lightmap;
                        }
                        igTreePop();
                    }
                    if (igButton("Edit Model", (ImVec2){0, 0})) {
                        ce->edit_terrain_model.active = true;
                        ce->edit_terrain_model.model_mat = terrain_entity_get_transform(terrain);
                        ce->edit_terrain_model.model = &terrain->terrain_model;
                        ce->edit_terrain_model.selected_array.length = 0;
                        ce->edit_terrain_model.hovered_array.length = 0;
                        ce->object_modifier.is_active = false;
                    }
                    if (igButton("Delete", (ImVec2){0, 0})) {
                        hole_editor_push_state(ce);
                        terrain_entity_deinit(terrain);
                        array_splice(&ce->hole->terrain_entities, entity.idx, 1);
                        ce->selected_array.length = 0;
                        ce->hovered_array.length = 0;
                    }
                }
                else if (entity.type == EDITOR_ENTITY_MULTI_TERRAIN_MOVING) {
                    struct multi_terrain_entity *multi_terrain =
                        &ce->hole->multi_terrain_entities.data[entity.idx];
                    igText("Type: MULTI TERRAIN (MOVING PART)");

                    vec3 pos = multi_terrain->moving_position;
                    igText("Position: <%0.5f, %0.5f, %0.5f>", pos.x, pos.y, pos.z);

                    vec3 scale = multi_terrain->moving_scale;
                    igText("Scale: <%.5f, %.5f, %.5f>", scale.x, scale.y, scale.z);

                    //float y_rot = multi_terrain->moving_y_rotation;
                    //igText("Y Rotation: %.5f", y_rot);

                    bool is_pendulum = multi_terrain->movement_data.type == MOVEMENT_TYPE_PENDULUM;
                    if (igCheckbox("Movement Type Pendulum", &is_pendulum)) {
                        if (is_pendulum) {
                            multi_terrain->movement_data.type = MOVEMENT_TYPE_PENDULUM;
                        }
                    }

                    bool is_to_and_from = multi_terrain->movement_data.type == MOVEMENT_TYPE_TO_AND_FROM;
                    if (igCheckbox("Movement Type To And From", &is_to_and_from)) {
                        if (is_to_and_from) {
                            multi_terrain->movement_data.type = MOVEMENT_TYPE_TO_AND_FROM;
                        }
                    }

                    bool is_ramp = multi_terrain->movement_data.type == MOVEMENT_TYPE_RAMP;
                    if (igCheckbox("Movement Type Ramp", &is_ramp)) {
                        if (is_ramp) {
                            multi_terrain->movement_data.type = MOVEMENT_TYPE_RAMP;
                        }
                    }

                    bool is_rotation = multi_terrain->movement_data.type == MOVEMENT_TYPE_ROTATION;
                    if (igCheckbox("Movement Type Rotation", &is_rotation)) {
                        if (is_rotation) {
                            multi_terrain->movement_data.type = MOVEMENT_TYPE_ROTATION;
                        }
                    }

                    struct movement_data *movement_data = &multi_terrain->movement_data;
                    if (movement_data->type == MOVEMENT_TYPE_PENDULUM) {
                        igInputFloat("Theta0", &movement_data->pendulum.theta0, 0.0f, 0.0f, "%0.2f", 0);
                    }
                    else if (movement_data->type == MOVEMENT_TYPE_TO_AND_FROM) {
                        igInputFloat3("P0", (float*) &movement_data->to_and_from.p0, "%0.2f", 0);
                        igInputFloat3("P1", (float*) &movement_data->to_and_from.p1, "%0.2f", 0);
                    }
                    else if (movement_data->type == MOVEMENT_TYPE_RAMP) {
                        igInputFloat3("Rotation Axis", (float*) &movement_data->ramp.rotation_axis, "%0.2f", 0);
                        igInputFloat("Theta0", &movement_data->ramp.theta0, 0.0f, 0.0f, "%0.2f", 0);
                        igInputFloat("Theta1", &movement_data->ramp.theta1, 0.0f, 0.0f, "%0.2f", 0);
                        igInputFloat("Transition Length", 
                                &movement_data->ramp.transition_length, 0.0f, 0.0f, "%0.2f", 0);
                    }
                    else if (movement_data->type == MOVEMENT_TYPE_ROTATION) {
                        igInputFloat("Theta0", (float*) &movement_data->rotation.theta0, 0.0f, 0.0f, "%0.2f", 0);
                        igInputFloat3("Axis", (float*) &movement_data->rotation.axis, "%0.2f", 0);
                    }
                    else {
                        assert(false);
                    }
                    igInputFloat("Time Length", &movement_data->length, 0.0f, 0.0f, "%0.2f", 0);

                    if (igButton("Edit Model", (ImVec2){0, 0})) {
                        ce->edit_terrain_model.active = true;
                        ce->edit_terrain_model.model_mat =
                            multi_terrain_entity_get_moving_transform(multi_terrain, ed->game->t);
                        ce->edit_terrain_model.model = &multi_terrain->moving_terrain_model;
                        ce->edit_terrain_model.selected_array.length = 0;
                        ce->edit_terrain_model.hovered_array.length = 0;
                        ce->object_modifier.is_active = false;
                    }

                    if (igButton("Delete", (ImVec2){0, 0})) {
                        hole_editor_push_state(ce);
                        multi_terrain_entity_deinit(multi_terrain);
                        array_splice(&ce->hole->multi_terrain_entities, entity.idx, 1);
                        ce->selected_array.length = 0;
                        ce->hovered_array.length = 0;
                    }

                    if (igTreeNodeStr("Lightmap")) {
                        struct lightmap *lightmap = &multi_terrain->moving_lightmap;
                        igText("Num Images: %d", lightmap->images.length);
                        igText("Width: %d", lightmap->width);
                        igText("Height: %d", lightmap->height);
                        igInputInt("New Num Images", &ce->lightmap_update.new_num_images, 0, 0, 0);
                        igInputInt("New Width", &ce->lightmap_update.new_height, 0, 0, 0);
                        igInputInt("New Height", &ce->lightmap_update.new_width, 0, 0, 0);
                        if (igButton("Update Lightmap", (ImVec2){0, 0})) {
                            ce->lightmap_update.do_it = true;
                            ce->lightmap_update.lightmap = lightmap;
                        }
                        igTreePop();
                    }
                }
                else if (entity.type == EDITOR_ENTITY_MULTI_TERRAIN_STATIC) {
                    struct multi_terrain_entity *multi_terrain = 
                        &ce->hole->multi_terrain_entities.data[entity.idx];
                    igText("Type: MULTI TERRAIN (STATIC PART)");

                    vec3 pos = multi_terrain->static_position;
                    igText("Position: <%0.5f, %0.5f, %0.5f>", pos.x, pos.y, pos.z);

                    vec3 scale = multi_terrain->static_scale;
                    igText("Scale: <%.5f, %.5f, %.5f>", scale.x, scale.y, scale.z);

                    //float y_rot = multi_terrain->static_y_rotation;
                    //igText("Y Rotation: %.5f", y_rot);

                    if (igButton("Edit Model", (ImVec2){0, 0})) {
                        ce->edit_terrain_model.active = true;
                        ce->edit_terrain_model.model_mat = multi_terrain_entity_get_static_transform(multi_terrain);
                        ce->edit_terrain_model.model = &multi_terrain->static_terrain_model;
                        ce->edit_terrain_model.selected_array.length = 0;
                        ce->edit_terrain_model.hovered_array.length = 0;
                        ce->object_modifier.is_active = false;
                    }

                    if (igButton("Delete", (ImVec2){0, 0})) {
                        hole_editor_push_state(ce);
                        multi_terrain_entity_deinit(multi_terrain);
                        array_splice(&ce->hole->multi_terrain_entities, entity.idx, 1);
                        ce->selected_array.length = 0;
                        ce->hovered_array.length = 0;
                    }

                    if (igTreeNodeStr("Lightmap")) {
                        struct lightmap *lightmap = &multi_terrain->static_lightmap;
                        igText("Num Images: %d", lightmap->images.length);
                        igText("Width: %d", lightmap->width);
                        igText("Height: %d", lightmap->height);
                        igInputInt("New Num Images", &ce->lightmap_update.new_num_images, 0, 0, 0);
                        igInputInt("New Width", &ce->lightmap_update.new_height, 0, 0, 0);
                        igInputInt("New Height", &ce->lightmap_update.new_width, 0, 0, 0);
                        if (igButton("Update Lightmap", (ImVec2){0, 0})) {
                            ce->lightmap_update.do_it = true;
                            ce->lightmap_update.lightmap = lightmap;
                        }
                        igTreePop();
                    }
                }
                else if (entity.type == EDITOR_ENTITY_ENVIRONMENT) {
                    struct environment_entity *environment = 
                        &ce->hole->environment_entities.data[entity.idx];
                    igText("Type: ENVIRONMENT");

                    vec3 pos = environment->position;
                    igText("Position: <%0.5f, %0.5f, %0.5f>", pos.x, pos.y, pos.z);
                    if (igInputFloat3("Scale", (float*) &environment->scale, "%0.3f", 0)) {
                        hole_editor_push_state(ce);
                    }
                    if (igInputFloat4("Orientation", (float*) &environment->orientation, "%0.3f", 0)) {
                        hole_editor_push_state(ce);
                    }
                    igCheckbox("Is Tiled", &environment->is_tiled);

                    bool delete_it = false;
                    if (igButton("Delete", (ImVec2){0, 0})) {
                        delete_it = true;
                    }

                    if (igTreeNodeStr("Model")) {
                        struct model *model = environment->model;
                        mdir_t *dir = &ce->selected_entity.environment.model_directory;
                        for (int i = 0; i < dir->num_files; i++) {
                            mfile_t f = dir->files[i];
                            if (strcmp(f.ext, ".terrain_model") != 0) {
                                continue;
                            }
                            bool is_selected = strcmp(model->name, f.name) == 0;
                            if (igSelectableBool(f.name, is_selected, 0, (ImVec2){0, 0})) {
                                environment->model = asset_store_get_model(f.name);
                            }
                        }
                        igTreePop();
                    }

                    if (igTreeNodeStr("Environment Lightmap")) {
                        struct lightmap *lightmap = &ce->hole->environment_lightmap;
                        igText("Width: %d", lightmap->width);
                        igText("Height: %d", lightmap->height);
                        igInputInt("New Num Images", &ce->lightmap_update.new_num_images, 0, 0, 0);
                        igInputInt("New Width", &ce->lightmap_update.new_height, 0, 0, 0);
                        igInputInt("New Height", &ce->lightmap_update.new_width, 0, 0, 0);
                        if (igButton("Update Lightmap", (ImVec2){0, 0})) {
                            ce->lightmap_update.do_it = true;
                            ce->lightmap_update.lightmap = lightmap;
                        }
                        igTreePop();
                    }

                    if (delete_it) {
                        hole_editor_push_state(ce);
                        environment_entity_deinit(environment);
                        array_splice(&ce->hole->environment_entities, entity.idx, 1);
                        ce->selected_array.length = 0;
                        ce->hovered_array.length = 0;
                    }
                }
                else if (entity.type == EDITOR_ENTITY_WATER) {
                    struct water_entity *water = &ce->hole->water_entities.data[entity.idx];
                    igText("Type: WATER");

                    vec3 pos = water->position;
                    igText("Position: <%0.5f, %0.5f, %0.5f>", pos.x, pos.y, pos.z);

                    vec3 scale = water->scale;
                    igText("Scale: <%.5f, %.5f, %.5f>", scale.x, scale.y, scale.z);

                    if (igButton("Edit Model", (ImVec2){0, 0})) {
                        ce->edit_terrain_model.active = true;
                        ce->edit_terrain_model.model_mat = water_entity_get_transform(water);
                        ce->edit_terrain_model.model = &water->model;
                        ce->edit_terrain_model.selected_array.length = 0;
                        ce->edit_terrain_model.hovered_array.length = 0;
                        ce->object_modifier.is_active = false;
                    }

                    if (igTreeNodeStr("Lightmap")) {
                        struct lightmap *lightmap = &water->lightmap;
                        igText("Width: %d", lightmap->width);
                        igText("Height: %d", lightmap->height);
                        igInputInt("New Width", &ce->lightmap_update.new_height, 0, 0, 0);
                        igInputInt("New Height", &ce->lightmap_update.new_width, 0, 0, 0);
                        if (igButton("Update Lightmap", (ImVec2){0, 0})) {
                            ce->lightmap_update.new_num_images = 1;
                            ce->lightmap_update.do_it = true;
                            ce->lightmap_update.lightmap = lightmap;
                        }
                        igTreePop();
                    }

                    if (igButton("Delete", (ImVec2){0, 0})) {
                        hole_editor_push_state(ce);
                        water_entity_deinit(water);
                        array_splice(&ce->hole->water_entities, entity.idx, 1);
                        ce->selected_array.length = 0;
                        ce->hovered_array.length = 0;
                    }
                }
                else if (entity.type == EDITOR_ENTITY_CUP) {

                }
                else if (entity.type == EDITOR_ENTITY_BEGINNING_CAMERA_ANIMATION) {

                }
                else if (entity.type == EDITOR_ENTITY_CAMERA_ZONE) {
                    struct camera_zone_entity *camera_zone = 
                        &ce->hole->camera_zone_entities.data[entity.idx];
                    igText("Type: CAMERA_ZONE");

                    vec3 pos = camera_zone->position;
                    igText("Position: <%0.5f, %0.5f, %0.5f>", pos.x, pos.y, pos.z);

                    if (igInputFloat2("Size", (float*) &camera_zone->size, "%0.3f", 0)) {
                        hole_editor_push_state(ce);
                    }

                    if (igInputFloat4("Look Direction", (float*) &camera_zone->orientation, "%0.3f", 0)) {
                        hole_editor_push_state(ce);
                    }

                    if (igCheckbox("Look Towards Cup:", &camera_zone->look_towards_cup)) {
                        hole_editor_push_state(ce);
                    }

                    if (igButton("Delete", (ImVec2){0, 0})) {
                    }
                }
                else if (entity.type == EDITOR_ENTITY_BALL_START) {

                }
                igEnd();
            }
        }

        struct object_modifier *om = &ce->object_modifier;
        if (om->is_active) {
            static bool object_modifier_window_open = true;
            igSetNextWindowSize((ImVec2){300, 400}, ImGuiCond_FirstUseEver);
            igSetNextWindowPos((ImVec2){1610, 10}, ImGuiCond_FirstUseEver, (ImVec2){0, 0});
            igBegin("Object Modifier", &object_modifier_window_open, ImGuiWindowFlags_None);

            if (igRadioButtonBool("Translate", om->mode == OBJECT_MODIFIER_MODE_TRANSLATE)) {
                if (om->mode != OBJECT_MODIFIER_MODE_TRANSLATE) {
                    om->mode = OBJECT_MODIFIER_MODE_TRANSLATE;
                    om->is_active = false;
                }
            }

            if (igRadioButtonBool("Rotate", om->mode == OBJECT_MODIFIER_MODE_ROTATE)) {
                if (om->mode != OBJECT_MODIFIER_MODE_ROTATE) {
                    om->mode = OBJECT_MODIFIER_MODE_ROTATE;
                    om->is_active = false;
                }
            }

            if (om->mode == OBJECT_MODIFIER_MODE_TRANSLATE) {
                vec3 pos = vec3_apply_mat4(om->pos, 1.0f, mat4_inverse(om->model_mat));
                vec3 pos0 = pos;
                if (igInputFloat3("Position", (float*) &pos, "%0.5f", 0)) {
                    pos = vec3_apply_mat4(pos, 1.0f, om->model_mat);
                    pos0 = vec3_apply_mat4(pos0, 1.0f, om->model_mat);

                    if (!om->was_used) {
                        om->set_start_positions = true;
                        om->was_used = true;
                        hole_editor_push_state(ce);
                    }
                    om->pos = pos;
                    om->translate_mode.delta_pos = vec3_add(om->translate_mode.delta_pos, vec3_sub(pos, pos0));
                }

                vec3 delta = vec3_apply_mat4(om->translate_mode.delta_pos, 0.0f, mat4_inverse(om->model_mat));
                if (igInputFloat3("Delta", (float*) &delta, "%0.5f", 0)) {
                    delta = vec3_apply_mat4(delta, 0.0f, om->model_mat);
                    if (!om->was_used) {
                        om->set_start_positions = true;
                        om->was_used = true;
                        hole_editor_push_state(ce);
                    }
                    om->translate_mode.delta_pos = delta;
                    om->pos = vec3_add(om->translate_mode.start_pos, delta);
                }

                igInputFloat("Clamp", &om->translate_mode.clamp, 0.0f, 0.0f, "%0.5f", 0);
            }
            else if (om->mode == OBJECT_MODIFIER_MODE_ROTATE) {
                float delta = om->rotate_mode.delta_theta;
                float delta_degrees = delta * 180.0f / MF_PI;
                if (igInputFloat("Delta", (float*) &delta_degrees, 0.0f, 0.0f, "%0.5f", 0)) {
                    if (!om->was_used) {
                        om->set_start_positions = true;
                        om->was_used = true;
                        hole_editor_push_state(ce);
                    }
                    delta = delta_degrees *MF_PI / 180.0f;
                    om->rotate_mode.delta_theta = delta;
                }

                igInputFloat("Clamp", &om->rotate_mode.clamp, 0.0f, 0.0f, "%0.5f", 0);
            }

            igEnd();
        }
    }
    profiler_pop_section();
}
#endif

void game_editor_init(struct game_editor *ed, struct game *game, struct renderer *renderer) {
    profiler_push_section("game_editor_init");
    ed->game = game;

    ed->history_idx = 0;
    array_init_sized(&ed->history, 256);
    array_init_sized(&ed->physics.collision_data_array, 4096);
    ed->physics.selected_collision_data_idx = -1;

    ed->last_hit.code[0] = 0;
    ed->last_hit.start_position = V3(0.0f, 0.0f, 0.0f);
    ed->last_hit.direction = V3(1.0f, 0.0f, 0.0f);
    ed->last_hit.power = 0.0f;
     
    ed->physics.draw_triangles = false;
    ed->physics.draw_cup_debug = false;
    ed->physics.debug_collisions = false;
    ed->physics.draw_triangle_chunks = false;

    map_init(&ed->profiler.section_colors);
    ed->profiler.selected_frame_idx = -1;
    ed->profiler.selected_sub_section = NULL;

    {
        ed->ball_movement.paused = true;
        array_init(&ed->ball_movement.positions);
        array_init(&ed->ball_movement.num_ticks);
    }

    ed->free_camera = false;
    ed->editing_hole = false;
#ifdef HOLE_EDITOR
    hole_editor_init(&ed->hole_editor, renderer);
#endif

    profiler_pop_section();
}

void game_editor_input(const void *event) {
}

static void decode_hit(char *code, vec3 *start_position, vec3 *direction, float *power) {
    int i_start_position_x;
    int i_start_position_y;
    int i_start_position_z;

    int i_direction_x;
    int i_direction_y;
    int i_direction_z;

    int i_power;

    sscanf(code, "%08x%08x%08x%08x%08x%08x%08x",
            &i_start_position_x, 
            &i_start_position_y, 
            &i_start_position_z,
            &i_direction_x,
            &i_direction_y,
            &i_direction_z,
            &i_power);

    start_position->x = (float) (i_start_position_x / 10000.0f);
    start_position->y = (float) (i_start_position_y / 10000.0f);
    start_position->z = (float) (i_start_position_z / 10000.0f);
    direction->x = (float) (i_direction_x / 10000.0f);
    direction->y = (float) (i_direction_y / 10000.0f);
    direction->z = (float) (i_direction_z / 10000.0f);
    *power = (float) (i_power / 10000.0f);
}

static void encode_hit(char *code, vec3 start_position, vec3 direction, float power) {
    int i_start_position_x = (int) (10000.0f * start_position.x);
    int i_start_position_y = (int) (10000.0f * start_position.y);
    int i_start_position_z = (int) (10000.0f * start_position.z);

    int i_direction_x = (int) (10000.0f * direction.x);
    int i_direction_y = (int) (10000.0f * direction.y);
    int i_direction_z = (int) (10000.0f * direction.z);

    int i_power = (int) (10000.0f * power);

    sprintf(code, "%08x%08x%08x%08x%08x%08x%08x",
            i_start_position_x, 
            i_start_position_y, 
            i_start_position_z,
            i_direction_x,
            i_direction_y,
            i_direction_z,
            i_power);
}

void game_editor_set_last_hit(struct game_editor *ed, vec3 start_position, vec3 direction, float power) {
    ed->last_hit.start_position = start_position;
    ed->last_hit.direction = direction;
    ed->last_hit.power = power;
    encode_hit(ed->last_hit.code, start_position, direction, power);
}

void game_editor_update(struct game_editor *ed, float dt, struct button_inputs button_inputs, 
        struct renderer *renderer) {
    profiler_push_section("game_editor_update");

    ImGuiIO *guiIO = igGetIO();
    if (guiIO->WantCaptureMouse) {
        for (int i = 0; i < SAPP_MAX_MOUSEBUTTONS; i++) {
            button_inputs.mouse_down[i] = false;
            button_inputs.mouse_clicked[i] = false;
        }
    }
    if (guiIO->WantCaptureKeyboard) {
        for (int i = 0; i < SAPP_MAX_KEYCODES; i++) {
            button_inputs.button_down[i] = false;
            button_inputs.button_clicked[i] = false;
        }
    }

    if (ed->editing_hole) {
#ifdef HOLE_EDITOR
        hole_editor_update(ed, dt, button_inputs, renderer);
#endif
    }
    else {
        if (ed->physics.debug_collisions) {
            int num_spheres = ed->physics.collision_data_array.length;
            int min_idx = -1;
            float min_t = FLT_MAX;
            for (int i = 0; i < num_spheres; i++) {
                struct physics_collision_data data = ed->physics.collision_data_array.data[i];
                vec3 sphere_pos = data.ball_pos;
                float sphere_radius = 0.01f;
                float t;
                int idx;
                if (ray_intersect_spheres(button_inputs.mouse_down_ray_orig, button_inputs.mouse_down_ray_dir, 
                            &sphere_pos, &sphere_radius, 1, &t, &idx)) {
                    if (t < min_t) {
                        min_t = t;
                        min_idx = i;
                    }
                }
            }
            if (min_idx > 0) {
                ed->physics.selected_collision_data_idx = min_idx;
            }
        }

        {
            static bool game_editor_window_open = false;
            if (button_inputs.button_clicked[SAPP_KEYCODE_F10]) {
                game_editor_window_open = !game_editor_window_open;
            }
            if (game_editor_window_open) {
                igSetNextWindowSize((ImVec2){500, 500}, ImGuiCond_FirstUseEver);
                igSetNextWindowPos((ImVec2){5, 5}, ImGuiCond_FirstUseEver, (ImVec2){0, 0});
                igBegin("Game Editor", &game_editor_window_open, ImGuiWindowFlags_None);

                igText("Frame Rate: %0.3f\n", igGetIO()->Framerate);

#ifdef HOLE_EDITOR
                if (igButton("Open Course Editor", (ImVec2){0, 0})) {
                    ed->hole_editor.hole = &ed->game->hole;
                    strncpy(ed->hole_editor.file.path, ed->hole_editor.hole->filepath, MFILE_MAX_PATH);
                    ed->hole_editor.file.path[MFILE_MAX_PATH - 1] = 0;
                    ed->editing_hole = true;
                }
#endif

                if (igButton("Move Ball To Start", (ImVec2){0, 0})) {
                    ed->game->player_ball.position = ed->game->hole.ball_start_entity.position;
                    ed->game->player_ball.position.y += ed->game->player_ball.radius;
                    ed->game->player_ball.velocity = V3(0.0f, 0.0f, 0.0f);
                }

                igCheckbox("Free Camera", &ed->free_camera);

                if (igTreeNodeStr("Camera")) {
                    vec3 pos = ed->game->cam.pos;
                    igText("Camera Pos: <%f, %f, %f>", pos.x, pos.y, pos.z);
                    igText("Azimuth Angle: %f", ed->game->cam.azimuth_angle);
                    igText("Inclination Angle: %f", ed->game->cam.inclination_angle);
                    igTreePop();
                }

                if (igTreeNodeStr("Hit")) {
                    char *code = ed->last_hit.code;
                    vec3 *start_position = &ed->last_hit.start_position;
                    vec3 *direction = &ed->last_hit.direction;
                    float *power = &ed->last_hit.power;

                    if (igInputText("Code", code, 1024, 0, NULL, NULL)) {
                        decode_hit(code, start_position, direction, power); 
                    }
                    if (igInputFloat3("Start Position", (float*) start_position, "%0.5f", 0)) {
                        encode_hit(code, *start_position, *direction, *power);
                    }
                    if (igInputFloat3("Direction", (float*) direction, "%0.5f", 0)) {
                        encode_hit(code, *start_position, *direction, *power);
                    }
                    if (igInputFloat("Power", power, 0.0f, 0.0f, "%0.5f", 0)) {
                        encode_hit(code, *start_position, *direction, *power);
                    }
                    if (igButton("Do Hit", (ImVec2){0, 0})) {
                        ed->game->player_ball.position = *start_position;
                        game_hit_player_ball(ed->game, *direction, *power, ed);
                        game_editor_push_game_history_hit(ed, *power, *start_position, *direction);
                    }

                    igTreePop();
                }

                if (igTreeNodeStr("History")) {
                    for (int i = 0; i < ed->history.length; i++) {
                        struct game_history history = ed->history.data[i];
                        if (history.type == GAME_HISTORY_REST) {
                            char str[256];
                            sprintf(str, "%d) REST: <%.2f, %.2f, %.2f>", i,
                                    history.rest.position.x, history.rest.position.y, history.rest.position.z);

                            bool selected = i == ed->history_idx - 1;
                            if (igSelectableBool(str, selected, 0, (ImVec2){0, 0})) {
                                ed->history_idx = i + 1;
                                ed->game->player_ball.position = history.rest.position;
                                ed->game->player_ball.velocity = V3(0.0f, 0.0f, 0.0f);
                                ed->game->state = GAME_STATE_WAITING_FOR_AIM;
                            }
                        }
                        else if (history.type == GAME_HISTORY_HIT) {
                            char str[256];
                            sprintf(str, "%d) HIT: <%.2f, %.2f, %.2f>, %.2f", i,
                                    history.hit.direction.x, history.hit.direction.y, history.hit.direction.z,
                                    history.hit.power);

                            bool selected = i == ed->history_idx - 1;
                            if (igSelectableBool(str, selected, 0, (ImVec2){0, 0})) {
                                ed->history_idx = i + 1;
                                ed->game->player_ball.position = history.hit.start_position;
                                game_hit_player_ball(ed->game, history.hit.direction, history.hit.power, ed);
                            } 
                        }
                        else {
                            assert(false);
                        }
                    }
                    igTreePop();
                }

                if (igTreeNodeStr("Aim Icon")) {
                    bool edited = false;
                    edited |= igInputFloat2("P0", (float*) &ed->game->aim.icon_bezier_point[0], "%0.5f", 0);
                    edited |= igInputFloat2("P1", (float*) &ed->game->aim.icon_bezier_point[1], "%0.5f", 0);
                    edited |= igInputFloat2("P2", (float*) &ed->game->aim.icon_bezier_point[2], "%0.5f", 0);
                    edited |= igInputFloat2("P3", (float*) &ed->game->aim.icon_bezier_point[3], "%0.5f", 0);
                    edited |= igInputFloat2("Offset", (float*) &ed->game->aim.icon_offset, "%0.5f", 0);
                    edited |= igInputFloat("Width", (float*) &ed->game->aim.icon_width, 0.0f, 0.0f, "%0.5f", 0);
                    if (edited) {
                        renderer_update_game_icon_buffer(renderer, ed->game->aim.icon_offset, 
                                ed->game->aim.icon_bezier_point[0], ed->game->aim.icon_bezier_point[1],
                                ed->game->aim.icon_bezier_point[2], ed->game->aim.icon_bezier_point[3]);
                    }
                    {
                        igText("Aim Angle: %f", ed->game->aim.angle);
                        igText("Aim Delta: %f, %f", ed->game->aim.delta.x, ed->game->aim.delta.y);
                    }
                    igTreePop();
                }

                if (igTreeNodeStr("Physics")) {
                    igCheckbox("Draw Triangles", &ed->physics.draw_triangles);
                    igCheckbox("Draw Hole Debug", &ed->physics.draw_cup_debug);
                    igCheckbox("Debug Collisions", &ed->physics.debug_collisions);
                    igCheckbox("Draw Triangle Chunks", &ed->physics.draw_triangle_chunks);

                    vec3 pos = ed->game->physics.grid.corner_pos;
                    igText("Grid Pos: <%.3f, %.3f, %.3f>", pos.x, pos.y, pos.z);

                    float cell_size = ed->game->physics.grid.cell_size;
                    igText("Grid Cell Size: %.3f", cell_size);

                    int num_rows = ed->game->physics.grid.num_rows;
                    igText("Grid Num Rows: %d", num_rows);

                    int num_cols = ed->game->physics.grid.num_cols;
                    igText("Grid Num Cols: %d", num_cols);

                    {
                        vec3 ball_pos = ed->game->player_ball.position;
                        vec3 cup_pos = ed->game->hole.cup_entity.position;
                        float dist_to_cup = vec3_distance(ball_pos, cup_pos);
                        igText("Distance To Hole: %0.3f", dist_to_cup);
                        igInputFloat("Hole Force", &ed->game->physics.cup_force, 0.0f, 0.0f, "%0.3f", 0);
                    }

                    igTreePop();
                }

                if (igTreeNodeStr("Game State")) {
                    enum game_state state = ed->game->state;
                    if (state == GAME_STATE_NOTHING) {
                        igText("State: NOTHING");
                    }
                    else if (state == GAME_STATE_BEGINNING_CAMERA_ANIMATION) {
                        igText("State: BEGINNING_CAMERA_ANIMATION");
                    }
                    else if (state == GAME_STATE_WAITING_FOR_AIM) {
                        igText("State: WAITING_FOR_AIM");
                    }
                    else if (state == GAME_STATE_BEGINNING_AIM) {
                        igText("State: BEGINNING_AIM");
                    }
                    else if (state == GAME_STATE_AIMING) {
                        igText("State: AIMING");
                    }
                    else if (state == GAME_STATE_SIMULATING_BALL) {
                        igText("State: SIMULATING_BALL");
                        struct ball_entity *ball = &ed->game->player_ball;
                        igText("Time Going Slow: %0.3f", ball->time_going_slow);
                    }

                    igTreePop();
                }

                if (igTreeNodeStr("Profiler")) {
                    struct profiler_state *prof = profiler_get_state();

                    struct array_profiler_section_info info_array;
                    array_init(&info_array);

                    const char *key;
                    map_iter_t iter = map_iter(&prof->section_info_map);
                    while ((key = map_next(&prof->section_info_map, &iter))) {
                        struct profiler_section_info *info = map_get(&prof->section_info_map, key);
                        assert(info);
                        array_push(&info_array, *info);
                    }

                    for (int i = 0; i < info_array.length; i++) {
                        struct profiler_section_info info = info_array.data[i];
                        igText("%s: %f", info.name, info.avg_dt);
                    }

                    igTreePop();
                }

                if (igTreeNodeStr("Aim Line")) {
                    for (int i = 1; i < ed->game->aim.num_line_points; i++) {
                        vec3 p0 = ed->game->aim.line_points[i - 1];
                        vec3 p1 = ed->game->aim.line_points[i];
                        float dist = vec3_distance(p1, p0);
                        igText("P: <%f, %f, %f>, %f", p1.x, p1.y, p1.z, dist);
                    }
                    igTreePop();
                }

                if (igTreeNodeStr("UI")) {
                    igText("Mouse Window Pos: <%f, %f>", button_inputs.window_mouse_pos.x, 
                            button_inputs.window_mouse_pos.y);
                    igText("Mouse Pos: <%f, %f>", button_inputs.mouse_pos.x,
                            button_inputs.mouse_pos.y);
                    igTreePop();
                }

                igEnd();
            }
        }

        if (ed->physics.selected_collision_data_idx >= 0 && 
                ed->physics.selected_collision_data_idx < ed->physics.collision_data_array.length) {
            static bool selected_collision_window_open;
            igSetNextWindowSize((ImVec2){300, 500}, ImGuiCond_FirstUseEver);
            igSetNextWindowPos((ImVec2){1610, 10}, ImGuiCond_FirstUseEver, (ImVec2){0, 0});
            igBegin("Selected Collision", &selected_collision_window_open, ImGuiWindowFlags_None);

            int idx = ed->physics.selected_collision_data_idx;
            struct physics_collision_data collision_data = ed->physics.collision_data_array.data[idx];

            if (igButton("Prev", (ImVec2){0, 0})) {
                if (idx - 1 >= 0) {
                    ed->physics.selected_collision_data_idx = idx - 1;
                }
            }
            igSameLine(0.0f, -1.0f);
            if (igButton("Next", (ImVec2){0, 0})) {
                if (idx + 1 < ed->physics.collision_data_array.length) {
                    ed->physics.selected_collision_data_idx = idx + 1;
                }
            }

            igText("Tick: %d", collision_data.tick_idx);
            igText("Ball Position: <%f, %f, %f>", 
                    collision_data.ball_pos.x, collision_data.ball_pos.y, collision_data.ball_pos.z);
            igText("Ball Velocity: <%f, %f, %f>", 
                    collision_data.ball_vel.x, collision_data.ball_vel.y, collision_data.ball_vel.z);

            const char *contact_type_string[] = {
                "Point A", "Point B", "Point C",
                "Edge AB", "Edge AC", "Edge BC",
                "FACE",
            };

            for (int i = 0; i < collision_data.num_ball_contacts; i++) {
                struct ball_contact contact = collision_data.ball_contacts[i];
                igText("Contact: %d", i);
                igText("    Type: %s", contact_type_string[contact.type]);
                igText("    Ignored: %d", contact.is_ignored);
                igText("    Penetration: %0.2f", contact.penetration);
                igText("    Impulse Magnitude: %0.2f", contact.impulse_mag);
                igText("    Impulse: <%0.2f, %0.2f, %0.2f>", 
                        contact.impulse.x, contact.impulse.y, contact.impulse.z);
                igText("    Restitution: %0.2f", contact.restitution); 
                igText("    Velocity Scale: %0.2f", contact.vel_scale);
                igText("    Start Speed: %0.2f", vec3_length(contact.v0));
                igText("    End Speed: %0.2f", vec3_length(contact.v1));
                igText("    Start Velocity: <%0.2f, %0.2f, %0.2f>", 
                        contact.v0.x, contact.v0.y, contact.v0.z);
                igText("    End Velocity: <%0.2f, %0.2f, %0.2f>", 
                        contact.v1.x, contact.v1.y, contact.v1.z);
                igText("    Cull Dot: %0.2f", contact.cull_dot);
                igText("    Position: <%0.2f, %0.2f, %0.2f>",
                        contact.position.x, contact.position.y, contact.position.z);
                igText("    Normal: <%0.2f, %0.2f, %0.2f>",
                        contact.normal.x, contact.normal.y, contact.normal.z);
                igText("    Triangle Normal: <%0.2f, %0.2f, %0.2f>",
                        contact.triangle_normal.x, contact.triangle_normal.y, contact.triangle_normal.z);
            }
            igEnd();
        }
    }

    profiler_pop_section();
}

void game_editor_push_physics_contact_data(struct game_editor *ed, struct ball_contact contact) {
    assert(ed->physics.collision_data_array.length > 0);
    struct physics_collision_data *collision_data = &array_last(&ed->physics.collision_data_array);
    if (collision_data->num_ball_contacts < GAME_MAX_NUM_BALL_CONTACTS) {
        collision_data->ball_contacts[collision_data->num_ball_contacts++] = contact;
    }
}

void game_editor_push_physics_collision_data(struct game_editor *ed, int tick_idx,
        vec3 ball_pos, vec3 ball_vel) {
    struct physics_collision_data data;
    data.tick_idx = tick_idx;
    data.ball_pos = ball_pos;
    data.ball_vel = ball_vel;
    data.num_ball_contacts = 0;
    if (ed->physics.collision_data_array.length < ed->physics.collision_data_array.capacity) {
        array_push(&ed->physics.collision_data_array, data);
    }
}

void game_editor_push_game_history_rest(struct game_editor *ed, vec3 position) {
    struct game_history history;
    history.type = GAME_HISTORY_REST;
    history.rest.position = position;

    ed->history.length = ed->history_idx;
    array_insert(&ed->history, ed->history_idx, history);
    ed->history_idx++;
}

void game_editor_push_game_history_hit(struct game_editor *ed, float power, vec3 start_position,
        vec3 direction) {
    struct game_history history;
    history.type = GAME_HISTORY_HIT;
    history.hit.power = power;
    history.hit.start_position = start_position;
    history.hit.direction = direction;

    ed->history.length = ed->history_idx;
    array_insert(&ed->history, ed->history_idx, history);
    ed->history_idx++;
}

void game_editor_push_ball_movement(struct game_editor *ed, int num_ticks) {
    if (ed->ball_movement.paused) {
        return;
    }
    array_push(&ed->ball_movement.positions, ed->game->player_ball.draw_position); 
    array_push(&ed->ball_movement.num_ticks, num_ticks); 
}

void game_editor_draw_warnings(struct game_editor *ed) {
    int entry_count = mlog_get_entry_count();

    if (entry_count > 0) {
        static bool warnings_window_open;
        igSetNextWindowSize((ImVec2){300, 500}, ImGuiCond_FirstUseEver);
        igSetNextWindowPos((ImVec2){10, 10}, ImGuiCond_FirstUseEver, (ImVec2){0, 0});
        igBegin("Warnings", &warnings_window_open, ImGuiWindowFlags_None);

        for (int i = 0; i < entry_count; i++) {
            igText(mlog_get_entry(i));
        }

        igEnd();
    }
}
