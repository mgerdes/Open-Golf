#define _CRT_SECURE_NO_WARNINGS

#include "editor/editor.h"

#include <assert.h>
#include <float.h>
#include <string.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include "IconsFontAwesome5/IconsFontAwesome5.h"
#include "sokol/sokol_time.h"
#include "stb/stb_image_write.h"
#include "common/alloc.h"
#include "common/bvh.h"
#include "common/data.h"
#include "common/graphics.h"
#include "common/inputs.h"
#include "common/log.h"
#include "common/script.h"

static golf_editor_t editor;
static golf_inputs_t *inputs;
static golf_graphics_t *graphics;

golf_editor_t *golf_editor_get(void) {
    return &editor;
}

void golf_editor_init(void) {
    inputs = golf_inputs_get();
    graphics = golf_graphics_get();

    golf_data_load("data/config/editor.cfg", false);
    golf_data_load("data/levels/level-1.level", false);

    ImGuiIO *IO = igGetIO();
    IO->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    memset(&editor, 0, sizeof(editor));

    {
        graphics->cam_pos = V3(5, 5, 5);
        graphics->cam_dir = vec3_normalize(V3(-1, -1, -1));
        graphics->cam_up = V3(0, 1, 0);
        editor.camera.inclination_angle = -2.25f;
        editor.camera.azimuth_angle = 0.65f;
    }

    {
        strcpy(editor.level_path, "data/levels/level-1.level");
        editor.level = golf_data_get_level("data/levels/level-1.level");

        vec_init(&editor.undo_actions, "editor");
        vec_init(&editor.redo_actions, "editor");
        editor.started_action = false;

        editor.hovered_idx = -1;
        vec_init(&editor.selected_idxs, "editor");
    }

    {
        editor.in_edit_mode = false;
        vec_init(&editor.edit_mode.selected_entities, "editor");
        vec_init(&editor.edit_mode.starting_positions, "editor");
        vec_init(&editor.edit_mode.point_idxs, "editor");
    }

    golf_gizmo_init(&editor.gizmo);
    vec_init(&editor.starting_transforms, "editor");

    {
        editor.gi_state.num_iterations = 1;
        editor.gi_state.num_dilates = 1;
        editor.gi_state.num_smooths = 1;
        editor.gi_state.gamma = 1;
        editor.gi_state.hemisphere_size = 16;
        editor.gi_state.z_near = 0.0005f;
        editor.gi_state.z_far = 5.0f;
        editor.gi_state.interpolation_passes = 4;
        editor.gi_state.interpolation_threshold = 0.01f;
        editor.gi_state.camera_to_surface_distance_modifier = 0.0f;
    }

    {
        editor.file_picker.open_popup = false;
        editor.file_picker.search[0] = 0;
    }

    {
        editor.select_box.is_open = false;
        editor.select_box.p0 = V2(0, 0);
        editor.select_box.p1 = V2(0, 0);
        vec_init(&editor.select_box.hovered_entities, "editor");
    }

    {
        editor.renderer.gi_on = true;
    }

    editor.open_save_as_popup = false;
    vec_init(&editor.copied_entities, "editor");
}

static void _golf_editor_file_picker(const char *name, golf_data_type_t type, char *path, void **data) {
    igInputText(name, path, GOLF_FILE_MAX_PATH, ImGuiInputTextFlags_ReadOnly, NULL, NULL);
    if (igIsItemClicked(ImGuiMouseButton_Left)) {
        editor.file_picker.open_popup = true;
        editor.file_picker.type = type;
        editor.file_picker.path = path;
        editor.file_picker.data = data;
    }
}

static void _golf_editor_start_editing_geo(golf_geo_t *geo, golf_transform_t transform) {
    if (editor.in_edit_mode) {
        golf_log_warning("Already in geo editing mode");
        return;
    }

    if (editor.gizmo.is_active) {
        return;
    }

    editor.in_edit_mode = true;
    editor.edit_mode.geo = geo;
    editor.edit_mode.transform = transform;
    editor.edit_mode.is_entity_hovered = 0;
    editor.edit_mode.selected_entities.length = 0;
    editor.select_box.is_open = false;
    editor.select_box.hovered_entities.length = 0;
}

static void _golf_editor_stop_editing_geo(void) {
    if (!editor.in_edit_mode) {
        golf_log_warning("Already not in geo editing mode");
        return;
    }

    if (editor.gizmo.is_active) {
        return;
    }

    editor.in_edit_mode = false;
} 

static void _golf_editor_queue_start_action(golf_editor_action_t action) {
    if (editor.has_queued_action) {
        golf_log_warning("Queueing action with one already queued");
        return;
    }

    editor.has_queued_action = true;
    editor.queued_action = action;
}

static void _golf_editor_queue_commit_action(void) {
    if (editor.has_queued_commit) {
        golf_log_warning("Queueing commit when one is already queued");
        return;
    }

    editor.has_queued_commit = true;
}

static void _golf_editor_queue_decommit_action(void) {
    if (editor.has_queued_decommit) {
        golf_log_warning("Queueing decommit when one is already queued");
        return;
    }

    editor.has_queued_decommit = true;
}

static void _golf_editor_action_init(golf_editor_action_t *action, const char *name) {
    action->name = name;
    vec_init(&action->datas, "editor");
}

static void _golf_editor_action_deinit(golf_editor_action_t *action) {
    for (int i = 0; i < action->datas.length; i++) {
        golf_editor_action_data_t data = action->datas.data[i];
        golf_free(data.copy);
    }
    vec_deinit(&action->datas);
}

static void _golf_editor_action_push_data(golf_editor_action_t *action, void *data, int data_size) {
    golf_editor_action_data_t action_data; 
    action_data.size = data_size;
    action_data.ptr = data;
    action_data.copy = golf_alloc(data_size);
    memcpy(action_data.copy, action_data.ptr, data_size);
    vec_push(&action->datas, action_data);
}

static void _golf_editor_start_action(golf_editor_action_t action) {
    if (editor.started_action) {
        golf_log_warning("Starting action with one already started");
        return;
    }

    editor.started_action = true;
    editor.cur_action = action;
}

static void _golf_editor_start_action_with_data(void *data, int data_len, const char *name) {
    golf_editor_action_t action;
    _golf_editor_action_init(&action, name);
    _golf_editor_action_push_data(&action, data, data_len);
    _golf_editor_start_action(action);
}

static void _golf_editor_commit_action(void) {
    if (!editor.started_action) {
        golf_log_warning("Commiting action without one started");
        return;
    }

    editor.started_action = false;
    vec_push(&editor.undo_actions, editor.cur_action);
    for (int i = 0; i < editor.redo_actions.length; i++) {
        _golf_editor_action_deinit(&editor.redo_actions.data[i]);
    }
    editor.redo_actions.length = 0;
}

static void _golf_editor_decommit_action(void) {
    if (!editor.started_action) {
        golf_log_warning("Commiting action without one started");
        return;
    }

    _golf_editor_action_deinit(&editor.cur_action);
    editor.started_action = false;
}

// Assumes you just pushed a new entity onto the level's entities vec
static void _golf_editor_commit_entity_create_action(void) {
    golf_entity_t *entity = &vec_last(&editor.level->entities);
    entity->active = false;
    _golf_editor_start_action_with_data(&entity->active, sizeof(entity->active), "Create entity");
    entity->active = true;
    _golf_editor_commit_action();
}

static void _golf_editor_fix_actions(char *new_ptr, char *old_ptr_start, char *old_ptr_end, golf_editor_action_t *cur_action) {
    if (cur_action) {
        for (int i = 0; i < cur_action->datas.length; i++) {
            golf_editor_action_data_t *action_data = &cur_action->datas.data[i];
            if (action_data->ptr >= old_ptr_start && action_data->ptr < old_ptr_end) {
                action_data->ptr = new_ptr + (action_data->ptr - old_ptr_start);
            }
        }
    }

    for (int i = 0; i < editor.undo_actions.length; i++) {
        golf_editor_action_t *action = &editor.undo_actions.data[i];
        for (int i = 0; i < action->datas.length; i++) {
            golf_editor_action_data_t *action_data = &action->datas.data[i];
            if (action_data->ptr >= old_ptr_start && action_data->ptr < old_ptr_end) {
                action_data->ptr = new_ptr + (action_data->ptr - old_ptr_start);
            }
        }
    }
    for (int i = 0; i < editor.redo_actions.length; i++) {
        golf_editor_action_t *action = &editor.redo_actions.data[i];
        for (int i = 0; i < action->datas.length; i++) {
            golf_editor_action_data_t *action_data = &action->datas.data[i];
            if (action_data->ptr >= old_ptr_start && action_data->ptr < old_ptr_end) {
                action_data->ptr = new_ptr + (action_data->ptr - old_ptr_start);
            }
        }
    }
}

#define _vec_push_and_fix_actions(v, val, cur_action)\
    do {\
        char *old_ptr = (char*)(v)->data;\
        vec_push(v, val); \
        if ((char*)(v)->data != old_ptr)\
            _golf_editor_fix_actions((char*)(v)->data, old_ptr, old_ptr + sizeof(val) * ((v)->length - 1), cur_action);\
    } while(0)

static void _golf_editor_undo_action(void) {
    if (editor.undo_actions.length == 0) {
        return;
    }

    golf_editor_action_t *undo_action = &vec_pop(&editor.undo_actions);
    golf_editor_action_t redo_action;
    _golf_editor_action_init(&redo_action, undo_action->name);
    for (int i = undo_action->datas.length - 1; i >= 0; i--) {
        golf_editor_action_data_t *data = &undo_action->datas.data[i];
        _golf_editor_action_push_data(&redo_action, data->ptr, data->size);
        memcpy(data->ptr, data->copy, data->size);
    }
    vec_push(&editor.redo_actions, redo_action);
    _golf_editor_action_deinit(undo_action);
}

static void _golf_editor_redo_action(void) {
    if (editor.redo_actions.length == 0) {
        return;
    }

    golf_editor_action_t *redo_action = &vec_pop(&editor.redo_actions);
    golf_editor_action_t undo_action;
    _golf_editor_action_init(&undo_action, redo_action->name);
    for (int i = redo_action->datas.length - 1; i >= 0; i--) {
        golf_editor_action_data_t *data = &redo_action->datas.data[i];
        _golf_editor_action_push_data(&undo_action, data->ptr, data->size);
        memcpy(data->ptr, data->copy, data->size);
    }
    vec_push(&editor.undo_actions, undo_action);
    _golf_editor_action_deinit(redo_action);
}

static void _golf_editor_undoable_igInputText(const char *label, char *buf, int buf_len, bool *edit_done, void *additional_buf, int additional_buf_len, const char *action_name) {
    if (edit_done) {
        *edit_done = false;
    }

    igInputText(label, buf, buf_len, ImGuiInputTextFlags_None, NULL, NULL);
    if (igIsItemActivated()) {
        golf_editor_action_t action;
        _golf_editor_action_init(&action, action_name);
        _golf_editor_action_push_data(&action, buf, buf_len);
        if (additional_buf) {
            _golf_editor_action_push_data(&action, additional_buf, additional_buf_len);
        }
        _golf_editor_queue_start_action(action);
    }
    if (igIsItemDeactivated()) {
        if (edit_done) {
            *edit_done = true;
        }
        if (igIsItemDeactivatedAfterEdit()) {
            _golf_editor_queue_commit_action();
        }
        else {
            _golf_editor_queue_decommit_action();
        }
    }
}

static void _golf_editor_undoable_igCheckbox(const char *label, bool *v, const char *action_name) {
    bool v0 = *v;
    igCheckbox(label, v);
    if (v0 != *v) {
        *v = !*v;
        _golf_editor_start_action_with_data(v, sizeof(bool), action_name);
        _golf_editor_commit_action();
        *v = !*v;
    }
}

static void _golf_editor_undoable_igInputInt(const char *label, int *i, const char *action_name) {
    igInputInt(label, i, 0, 0, ImGuiInputTextFlags_None);
    if (igIsItemActivated()) {
        golf_editor_action_t action;
        _golf_editor_action_init(&action, action_name);
        _golf_editor_action_push_data(&action, i, sizeof(int));
        _golf_editor_queue_start_action(action);
    }
    if (igIsItemDeactivated()) {
        if (igIsItemDeactivatedAfterEdit()) {
            _golf_editor_queue_commit_action();
        }
        else {
            _golf_editor_queue_decommit_action();
        }
    }
}

static void _golf_editor_undoable_igInputFloat(const char *label, float *f, const char *action_name) {
    igInputFloat(label, f, 0, 0, "%.4f", ImGuiInputTextFlags_None);
    if (igIsItemActivated()) {
        golf_editor_action_t action;
        _golf_editor_action_init(&action, action_name);
        _golf_editor_action_push_data(&action, f, sizeof(float));
        _golf_editor_queue_start_action(action);
    }
    if (igIsItemDeactivated()) {
        if (igIsItemDeactivatedAfterEdit()) {
            _golf_editor_queue_commit_action();
        }
        else {
            _golf_editor_queue_decommit_action();
        }
    }
}

static void _golf_editor_undoable_igInputFloat2(const char *label, float *f2, const char *action_name) {
    igInputFloat2(label, f2, "%.4f", ImGuiInputTextFlags_None);
    if (igIsItemActivated()) {
        golf_editor_action_t action;
        _golf_editor_action_init(&action, action_name);
        _golf_editor_action_push_data(&action, f2, 2 * sizeof(float));
        _golf_editor_queue_start_action(action);
    }
    if (igIsItemDeactivated()) {
        if (igIsItemDeactivatedAfterEdit()) {
            _golf_editor_queue_commit_action();
        }
        else {
            _golf_editor_queue_decommit_action();
        }
    }
}

static void _golf_editor_undoable_igInputFloat3(const char *label, float *f3, const char *action_name) {
    igInputFloat3(label, f3, "%.4f", ImGuiInputTextFlags_None);
    if (igIsItemActivated()) {
        golf_editor_action_t action;
        _golf_editor_action_init(&action, action_name);
        _golf_editor_action_push_data(&action, f3, 3 * sizeof(float));
        _golf_editor_queue_start_action(action);
    }
    if (igIsItemDeactivated()) {
        if (igIsItemDeactivatedAfterEdit()) {
            _golf_editor_queue_commit_action();
        }
        else {
            _golf_editor_queue_decommit_action();
        }
    }
}

static void _golf_editor_undoable_igInputFloat4(const char *label, float *f4, const char *action_name) {
    igInputFloat4(label, f4, "%.4f", ImGuiInputTextFlags_None);
    if (igIsItemActivated()) {
        golf_editor_action_t action;
        _golf_editor_action_init(&action, action_name);
        _golf_editor_action_push_data(&action, f4, 4 * sizeof(float));
        _golf_editor_queue_start_action(action);
    }
    if (igIsItemDeactivated()) {
        if (igIsItemDeactivatedAfterEdit()) {
            _golf_editor_queue_commit_action();
        }
        else {
            _golf_editor_queue_decommit_action();
        }
    }
}

static bool _golf_editor_is_entity_selected(int idx) {
    int found_idx = -1;
    vec_find(&editor.selected_idxs, idx, found_idx);
    return found_idx != -1;
}

static void _golf_editor_select_entity(int idx) {
    if (idx < 0) {
        editor.selected_idxs.length = 0;
    }
    else {
        bool shift_down = inputs->button_down[SAPP_KEYCODE_LEFT_SHIFT];
        bool selected = _golf_editor_is_entity_selected(idx);

        if (selected) {
            if (shift_down) {
                vec_remove(&editor.selected_idxs, idx);
            }
            else {
                editor.selected_idxs.length = 0;
                vec_push(&editor.selected_idxs, idx);
            }
        }
        else {
            if (!shift_down) {
                editor.selected_idxs.length = 0;
            }
            vec_push(&editor.selected_idxs, idx);
        }
    }
}

static void _golf_editor_duplicate_selected_entities(void) {
    if (editor.in_edit_mode) {
        golf_geo_t *geo = editor.edit_mode.geo;

        vec_golf_geo_point_t new_points;
        vec_init(&new_points, "editor");

        vec_golf_geo_face_t new_faces;
        vec_init(&new_faces, "editor");

        vec_int_t copied_point_idxs;
        vec_init(&copied_point_idxs, "editor");

        for (int i = 0; i < editor.edit_mode.selected_entities.length; i++) {
            golf_edit_mode_entity_t *entity = &editor.edit_mode.selected_entities.data[i];
            switch (entity->type) {
                case GOLF_EDIT_MODE_ENTITY_FACE: {
                    golf_geo_face_t face = geo->faces.data[entity->idx];

                    vec_int_t idx_copy;
                    vec_init(&idx_copy, "face");
                    vec_pusharr(&idx_copy, face.idx.data, face.idx.length);

                    vec_vec2_t uvs_copy;
                    vec_init(&uvs_copy, "face");
                    vec_pusharr(&uvs_copy, face.uvs.data, face.uvs.length);

                    golf_geo_face_t new_face = golf_geo_face(face.material_name, face.idx.length, idx_copy, face.uv_gen_type, uvs_copy);
                    for (int i = 0; i < face.idx.length; i++) {
                        int idx = face.idx.data[i];

                        int new_point_idx;
                        bool already_copied_point = false;
                        for (int i = 0; i < copied_point_idxs.length; i++) {
                            if (copied_point_idxs.data[i] == idx) {
                                new_point_idx = geo->points.length + i;
                                already_copied_point = true;
                                break;
                            }
                        }

                        if (!already_copied_point) {
                            new_point_idx = geo->points.length + new_points.length;
                            golf_geo_point_t point = geo->points.data[idx];
                            golf_geo_point_t new_point = golf_geo_point(point.position);
                            vec_push(&copied_point_idxs, idx);
                            vec_push(&new_points, new_point);
                        }

                        new_face.idx.data[i] = new_point_idx;
                    }

                    entity->idx = geo->faces.length + new_faces.length;
                    vec_push(&new_faces, new_face);
                    break;
                }
                case GOLF_EDIT_MODE_ENTITY_LINE: {
                    int new_point0_idx; 
                    int new_point1_idx; 
                    bool already_copied_point0 = false;
                    bool already_copied_point1 = false;
                    for (int i = 0; i < copied_point_idxs.length; i++) {
                        if (copied_point_idxs.data[i] == entity->idx) {
                            new_point0_idx = geo->points.length + i;
                            already_copied_point0 = true;
                        }
                        if (copied_point_idxs.data[i] == entity->idx2) {
                            new_point1_idx = geo->points.length + i;
                            already_copied_point1 = true;
                        }
                        if (already_copied_point0 && already_copied_point1) {
                            break;
                        }
                    }

                    if (!already_copied_point0) {
                        new_point0_idx = geo->points.length + new_points.length;
                        golf_geo_point_t point = geo->points.data[entity->idx];
                        golf_geo_point_t new_point = golf_geo_point(point.position);
                        vec_push(&copied_point_idxs, entity->idx);
                        vec_push(&new_points, new_point);
                    }
                    if (!already_copied_point1) {
                        new_point1_idx = geo->points.length + new_points.length;
                        golf_geo_point_t point = geo->points.data[entity->idx2];
                        golf_geo_point_t new_point = golf_geo_point(point.position);
                        vec_push(&copied_point_idxs, entity->idx2);
                        vec_push(&new_points, new_point);
                    }

                    entity->idx = new_point0_idx;
                    entity->idx2 = new_point1_idx;
                    break;
                }
                case GOLF_EDIT_MODE_ENTITY_POINT: {
                    int new_point_idx;
                    bool already_copied_point = false;
                    for (int i = 0; i < copied_point_idxs.length; i++) {
                        if (copied_point_idxs.data[i] == entity->idx) {
                            new_point_idx = geo->points.length + i;
                            already_copied_point = true;
                            break;
                        }
                    }
                    if (!already_copied_point) {
                        new_point_idx = geo->points.length + new_points.length;
                        golf_geo_point_t point = geo->points.data[entity->idx];
                        golf_geo_point_t new_point = golf_geo_point(point.position);
                        vec_push(&copied_point_idxs, entity->idx);
                        vec_push(&new_points, new_point);
                    }

                    entity->idx = new_point_idx;
                    break;
                }
            }
        }

        golf_editor_action_t action;
        _golf_editor_action_init(&action, "Duplicate geo entities");

        for (int i = 0; i < new_points.length; i++) {
            _vec_push_and_fix_actions(&geo->points, new_points.data[i], &action);
            golf_geo_point_t *point = &vec_last(&geo->points);
            point->active = false;
            _golf_editor_action_push_data(&action, &point->active, sizeof(point->active));
            point->active = true;
        }
        for (int i = 0; i < new_faces.length; i++) {
            _vec_push_and_fix_actions(&geo->faces, new_faces.data[i], &action);
            golf_geo_face_t *face = &vec_last(&geo->faces);
            face->active = false;
            _golf_editor_action_push_data(&action, &face->active, sizeof(face->active));
            face->active = true;
        }

        _golf_editor_start_action(action);
        _golf_editor_commit_action();

        vec_deinit(&new_points);
        vec_deinit(&new_faces);
        vec_deinit(&copied_point_idxs);
    }
    else {
        for (int i = 0; i < editor.selected_idxs.length; i++) {
            int idx = editor.selected_idxs.data[i];
            golf_entity_t *selected_entity = &editor.level->entities.data[idx];
            golf_entity_t entity_copy = golf_entity_make_copy(selected_entity);
            _vec_push_and_fix_actions(&editor.level->entities, entity_copy, NULL);
        }

        golf_editor_action_t action;
        _golf_editor_action_init(&action, "Duplicate entities");

        for (int i = 0; i < editor.selected_idxs.length; i++) {
            int idx = editor.level->entities.length - i - 1;
            golf_entity_t *entity = &editor.level->entities.data[idx];
            entity->active = false;
            _golf_editor_action_push_data(&action, &entity->active, sizeof(entity->active));
            entity->active = true;
        }

        _golf_editor_start_action(action);
        _golf_editor_commit_action();
    }
}

static void _golf_editor_copy_selected_entities(void) {
    if (editor.in_edit_mode) {
    }
    else {
        editor.copied_entities.length = 0;
        for (int i = 0; i < editor.selected_idxs.length; i++) {
            int idx = editor.selected_idxs.data[i];
            golf_entity_t *selected_entity = &editor.level->entities.data[idx];
            golf_entity_t entity_copy = golf_entity_make_copy(selected_entity);
            vec_push(&editor.copied_entities, entity_copy);
        }
    }
}

static void _golf_editor_paste_selected_entities(void) {
    if (editor.in_edit_mode) {
    }
    else {
        golf_editor_action_t action;
        _golf_editor_action_init(&action, "Paste entities");

        for (int i = 0; i < editor.copied_entities.length; i++) {
            golf_entity_t entity_copy = editor.copied_entities.data[i];
            _vec_push_and_fix_actions(&editor.level->entities, entity_copy, &action);

            golf_entity_t *entity = &vec_last(&editor.level->entities);
            entity->active = false;
            _golf_editor_action_push_data(&action, &entity->active, sizeof(entity->active));
            entity->active = true;
        }
        editor.copied_entities.length = 0;

        _golf_editor_start_action(action);
        _golf_editor_commit_action();
    }
}

static void _golf_editor_push_unique_idx(vec_int_t *idxs, int idx) {
    for (int i = 0; i < idxs->length; i++) {
        if (idxs->data[i] == idx) {
            return;
        }
    }
    vec_push(idxs, idx);
}

static void _golf_geo_delete_face(golf_geo_t *geo, int idx, golf_editor_action_t *action) {
    golf_geo_face_t *face = &geo->faces.data[idx];
    if (!face->active) {
        golf_log_warning("Double deleting face");
        return;
    }

    _golf_editor_action_push_data(action, &face->active, sizeof(face->active));
    face->active = false;
}

static void _golf_geo_delete_point(golf_geo_t *geo, int idx, golf_editor_action_t *action) {
    golf_geo_point_t *point = &geo->points.data[idx];
    if (!point->active) {
        golf_log_warning("Double deleting point");
        return;
    }

    _golf_editor_action_push_data(action, &point->active, sizeof(point->active));
    point->active = false;

    for (int i = 0; i < geo->faces.length; i++) {
        golf_geo_face_t *face = &geo->faces.data[i];
        if (!face->active) continue;

        for (int j = 0; j < face->idx.length; j++) {
            if (face->idx.data[j] == idx) {
                _golf_geo_delete_face(geo, i, action);
                break;
            }
        }
    }
}

static void _golf_editor_delete_selected_entities(void) {
    golf_editor_action_t action;
    _golf_editor_action_init(&action, "Delete selected entities");

    if (editor.in_edit_mode) {
        vec_int_t point_idxs;
        vec_init(&point_idxs, "editor");

        golf_geo_t *geo = editor.edit_mode.geo;
        for (int i = 0; i < editor.edit_mode.selected_entities.length; i++) {
            golf_edit_mode_entity_t *entity = &editor.edit_mode.selected_entities.data[i];
            switch (entity->type) {
                case GOLF_EDIT_MODE_ENTITY_FACE: 
                    _golf_geo_delete_face(geo, entity->idx, &action);
                    break;
                case GOLF_EDIT_MODE_ENTITY_POINT: 
                    _golf_editor_push_unique_idx(&point_idxs, entity->idx);
                    break;
                case GOLF_EDIT_MODE_ENTITY_LINE: 
                    _golf_editor_push_unique_idx(&point_idxs, entity->idx);
                    _golf_editor_push_unique_idx(&point_idxs, entity->idx2);
                    break;
            }
        }
        for (int i = 0; i < point_idxs.length; i++) {
            _golf_geo_delete_point(geo, point_idxs.data[i], &action);
        }

        vec_deinit(&point_idxs);
    }
    else {
        for (int i = 0; i < editor.selected_idxs.length; i++) {
            int idx = editor.selected_idxs.data[i];
            golf_entity_t *entity = &editor.level->entities.data[idx];
            _golf_editor_action_push_data(&action, &entity->active, sizeof(entity->active));
            entity->active = false;
        }
    }

    _golf_editor_start_action(action);
    _golf_editor_commit_action();
}

static void _golf_editor_edit_mode_select_entity(golf_edit_mode_entity_t entity) {
    bool shift_down = inputs->button_down[SAPP_KEYCODE_LEFT_SHIFT];
    bool selected = golf_editor_is_edit_entity_selected(entity);

    if (selected) {
        if (shift_down) {
            for (int i = 0; i < editor.edit_mode.selected_entities.length; i++) {
                golf_edit_mode_entity_t entity2 = editor.edit_mode.selected_entities.data[i];
                if (golf_editor_edit_entities_compare(entity, entity2)) {
                    vec_splice(&editor.edit_mode.selected_entities, i, 1);
                    break;
                }
            }
        }
        else {
            editor.edit_mode.selected_entities.length = 0;
            vec_push(&editor.edit_mode.selected_entities, entity);
        }
    }
    else {
        if (!shift_down) {
            editor.edit_mode.selected_entities.length = 0;
        }
        vec_push(&editor.edit_mode.selected_entities, entity);
    }
}

static void _golf_editor_edit_transform(golf_transform_t *transform) {
    if (igTreeNode_Str("Transform")) {
        _golf_editor_undoable_igInputFloat3("Position", (float*)&transform->position, "Modify transform position");
        _golf_editor_undoable_igInputFloat3("Scale", (float*)&transform->scale, "Modify transform scale");
        _golf_editor_undoable_igInputFloat4("Rotation", (float*)&transform->rotation, "Modify transform rotation");
        igTreePop();
    }
}

static void _golf_editor_edit_lightmap_section(golf_lightmap_section_t *lightmap_section) {
    if (igTreeNode_Str("Lightmap Section")) {
        _golf_editor_undoable_igInputText("Lightmap Name", lightmap_section->lightmap_name, GOLF_MAX_NAME_LEN, NULL, NULL, 0, "Modify lightmap section's lightmap name");

        if (editor.selected_idxs.length > 1) {
            if (igButton("Mass Apply Lightmap Name", (ImVec2){0, 0})) {
                golf_editor_action_t action;
                _golf_editor_action_init(&action, "Mass apply lightmap name");

                const char *name = lightmap_section->lightmap_name;
                for (int i = 1; i < editor.selected_idxs.length; i++) {
                    int idx = editor.selected_idxs.data[i];
                    golf_entity_t *entity = &editor.level->entities.data[idx];
                    golf_lightmap_section_t *other_lightmap_section = golf_entity_get_lightmap_section(entity);
                    if (other_lightmap_section) {
                        char *other_name = other_lightmap_section->lightmap_name;
                        _golf_editor_action_push_data(&action, other_name, GOLF_MAX_NAME_LEN);
                        snprintf(other_name, GOLF_MAX_NAME_LEN, "%s", name);
                    }
                }

                _golf_editor_start_action(action);
                _golf_editor_commit_action();
            }
        }
        igTreePop();
    }
}

static void _golf_editor_edit_movement(golf_movement_t *movement) {
    if (igTreeNode_Str("Movement")) {
        golf_movement_type_t movement_type_before = movement->type;
        const char *items[] = { "None", "Linear", "Spinner" };
        igCombo_Str_arr("Type", (int*)&movement->type, items, sizeof(items) / sizeof(items[0]), 0);
        if (movement_type_before != movement->type) {
            golf_movement_type_t movement_type_after = movement->type;
            movement->type = movement_type_before;
            _golf_editor_start_action_with_data(movement, sizeof(golf_movement_t), "Change movement type");
            _golf_editor_commit_action();
            movement->type = movement_type_after;

            movement->t = 0;
            movement->length = 1;
            switch (movement->type) {
                case GOLF_MOVEMENT_NONE:
                    break;
                case GOLF_MOVEMENT_LINEAR:
                    movement->linear.p0 = V3(0, 0, 0);
                    movement->linear.p1 = V3(0, 0, 0);
                    break;
                case GOLF_MOVEMENT_SPINNER:
                    break;
            }
        }

        switch (movement->type) {
            case GOLF_MOVEMENT_NONE:
                break;
            case GOLF_MOVEMENT_LINEAR: {
                _golf_editor_undoable_igInputFloat("Length", (float*)&movement->length, "Modify movement length");
                _golf_editor_undoable_igInputFloat3("P0", (float*)&movement->linear.p0, "Modify linear movement p0");
                _golf_editor_undoable_igInputFloat3("P1", (float*)&movement->linear.p1, "Modify linear movement p1");
                break;
            }
            case GOLF_MOVEMENT_SPINNER: {
                _golf_editor_undoable_igInputFloat("Length", (float*)&movement->length, "Modify movement length");
                break;
            }
        }

        igTreePop();
    }
}

static void _golf_editor_edit_geo(golf_geo_t *geo, golf_transform_t transform) {
    if (igTreeNode_Str("Geo")) {
        if (igButton("Edit Geo", (ImVec2){0, 0})) {
            _golf_editor_start_editing_geo(geo, transform);
        }
        igTreePop();
    }
}

static void _golf_editor_geo_tab(void) {
    golf_geo_t *geo = editor.edit_mode.geo;
    int num_points_selected = 0;
    for (int i = 0; i < editor.edit_mode.selected_entities.length; i++) {
        golf_edit_mode_entity_t entity = editor.edit_mode.selected_entities.data[i];
        if (entity.type == GOLF_EDIT_MODE_ENTITY_POINT) {
            num_points_selected++;
        }
    }
    if (num_points_selected >= 3) {
        if (igButton("Create Face", (ImVec2){0, 0}) || inputs->button_clicked[SAPP_KEYCODE_C]) {
            int n = num_points_selected;
            vec_int_t idxs;
            vec_init(&idxs, "face");
            vec_vec2_t uvs;
            vec_init(&uvs, "face");
            for (int i = 0; i < editor.edit_mode.selected_entities.length; i++) {
                golf_edit_mode_entity_t entity = editor.edit_mode.selected_entities.data[i];
                if (entity.type == GOLF_EDIT_MODE_ENTITY_POINT) {
                    vec_push(&idxs, entity.idx);
                    vec_push(&uvs, V2(0, 0));
                }
            }
            golf_geo_face_uv_gen_type_t uv_gen_type = GOLF_GEO_FACE_UV_GEN_MANUAL;
            golf_geo_face_t face = golf_geo_face("default", n, idxs, uv_gen_type, uvs);
            _vec_push_and_fix_actions(&geo->faces, face, NULL);

            golf_geo_face_t *new_face = &vec_last(&geo->faces);
            new_face->active = false;
            golf_editor_action_t action;
            _golf_editor_action_init(&action, "Create face");
            _golf_editor_action_push_data(&action, &new_face->active, sizeof(new_face->active));
            new_face->active = true;
            _golf_editor_start_action(action);
            _golf_editor_commit_action();
        }
    }

    if (igTreeNode_Str("Faces")) {
        for (int i = 0; i < geo->faces.length; i++) {
            golf_geo_face_t *face = &geo->faces.data[i];
            if (!face->active) continue;

            bool selected = golf_editor_is_edit_entity_selected(golf_edit_mode_entity_face(i));
            igPushID_Int(i);
            if (igSelectable_Bool("Face", selected, ImGuiSelectableFlags_None, (ImVec2){0, 0})) {
                _golf_editor_edit_mode_select_entity(golf_edit_mode_entity_face(i));
            }
            igPopID();
        }
        igTreePop();
    }

    if (igTreeNode_Str("Points")) {
        for (int i = 0; i < geo->points.length; i++) {
            golf_geo_point_t *point = &geo->points.data[i];
            if (!point->active) continue;

            bool selected = golf_editor_is_edit_entity_selected(golf_edit_mode_entity_point(i));
            igPushID_Int(i);
            if (igSelectable_Bool("Point", selected, ImGuiSelectableFlags_None, (ImVec2){0, 0})) {
                _golf_editor_edit_mode_select_entity(golf_edit_mode_entity_point(i));
            }
            igPopID();
        }
        igTreePop();
    }

    if (igTreeNode_Str("Generator")) {
        golf_script_store_t *script_store = golf_script_store_get();
        golf_script_t *selected_script = geo->generator_data.script;
        const char *selected_path = "";
        if (selected_script) {
            selected_path = selected_script->path;
        }
        if (igBeginCombo("Scripts", selected_path, ImGuiComboFlags_None)) {
            for (int i = 0; i < script_store->scripts.length; i++) {
                golf_script_t *script = script_store->scripts.data[i];
                bool selected = selected_script && strcmp(selected_script->path, script->path) == 0;
                if (igSelectable_Bool(script->path, selected, ImGuiSelectableFlags_None, (ImVec2){0, 0})) {
                    if (geo->generator_data.script != script) {
                        _golf_editor_start_action_with_data(&geo->generator_data.script, sizeof(geo->generator_data.script), "Modify script");
                        _golf_editor_commit_action();
                        geo->generator_data.script = script;
                    }
                }
            }
            igEndCombo();
        }
        if (selected_script) {
            gs_val_t generate_val;
            if (golf_script_get_val(selected_script, "generate", &generate_val) && generate_val.type == GS_VAL_FN) {
                gs_stmt_t *fn_stmt = generate_val.fn_stmt;
                for (int i = 0; i < fn_stmt->fn_decl.num_args; i++) {
                    gs_val_type type = fn_stmt->fn_decl.arg_types[i];
                    const char *symbol = fn_stmt->fn_decl.arg_symbols[i].symbol;

                    golf_geo_generator_data_arg_t *arg;
                    if (!golf_geo_generator_data_get_arg(&geo->generator_data, symbol, &arg)) {
                        golf_geo_generator_data_arg_t arg0;
                        snprintf(arg0.name, GOLF_MAX_NAME_LEN, "%s", symbol);
                        arg0.val = gs_val_default(type);
                        _vec_push_and_fix_actions(&geo->generator_data.args, arg0, NULL);

                        golf_geo_generator_data_get_arg(&geo->generator_data, symbol, &arg);
                    }
                    if (arg->val.type != type) {
                        arg->val = gs_val_default(type);
                    }

                    switch (type) {
                        case GS_VAL_BOOL: 
                            _golf_editor_undoable_igCheckbox(symbol, &arg->val.bool_val, "Modify arg");
                            break;
                        case GS_VAL_INT: 
                            _golf_editor_undoable_igInputInt(symbol, &arg->val.int_val, "Modify arg");
                            break;
                        case GS_VAL_FLOAT: 
                            _golf_editor_undoable_igInputFloat(symbol, &arg->val.float_val, "Modify arg");
                            break;
                        case GS_VAL_VEC2:
                            _golf_editor_undoable_igInputFloat2(symbol, (float*)&arg->val.vec2_val, "Modify arg");
                            break;
                        case GS_VAL_VEC3: 
                            _golf_editor_undoable_igInputFloat3(symbol, (float*)&arg->val.vec3_val, "Modify arg");
                            break;
                        case GS_VAL_LIST: 
                        case GS_VAL_STRING:
                        case GS_VAL_FN:
                        case GS_VAL_C_FN:
                        case GS_VAL_ERROR:
                        case GS_VAL_VOID:
                        case GS_VAL_NUM_TYPES: {
                            break;
                        }
                    }
                }

                if (igButton("Run", (ImVec2){0, 0})) {
                    bool error = false;
                    int num_args = fn_stmt->fn_decl.num_args;
                    gs_val_t *args = golf_alloc(sizeof(gs_val_t) * num_args);
                    for (int i = 0; i < num_args; i++) {
                        gs_val_type type = fn_stmt->fn_decl.arg_types[i];
                        const char *symbol = fn_stmt->fn_decl.arg_symbols[i].symbol;

                        golf_geo_generator_data_arg_t *arg;
                        if (!golf_geo_generator_data_get_arg(&geo->generator_data, symbol, &arg)) {
                            error = true;
                            golf_log_warning("Could not find argument %s", symbol);
                            break;
                        }
                        if (arg->val.type != type) {
                            error = true;
                            golf_log_warning("Invalid type for argument %s", symbol);
                            break;
                        }

                        args[i] = arg->val;
                    }

                    if (!error) {
                        golf_editor_action_t action;
                        _golf_editor_action_init(&action, "Run generator");
                        _golf_editor_action_push_data(&action, &geo->points.length, sizeof(geo->points.length));
                        _golf_editor_action_push_data(&action, geo->points.data, sizeof(golf_geo_point_t) * geo->points.length);
                        _golf_editor_action_push_data(&action, &geo->faces.length, sizeof(geo->faces.length));
                        _golf_editor_action_push_data(&action, geo->faces.data, sizeof(golf_geo_face_t) * geo->faces.length);
                        _golf_editor_start_action(action);
                        _golf_editor_commit_action();

                        geo->points.length = 0;
                        geo->faces.length = 0;
                        golf_script_eval_fn(selected_script, "generate", args, num_args);
                    }
                    golf_free(args);
                }
            }
        }

        igTreePop();
    }

    if (igButton("Exit Editing Geo", (ImVec2){0, 0})) {
        _golf_editor_stop_editing_geo();
    }
}

static void _golf_editor_entities_tab(void) {
    for (int i = 0; i < editor.level->entities.length; i++) {
        golf_entity_t *entity = &editor.level->entities.data[i];
        if (!entity->active) continue;
        if (entity->parent_idx >= 0) continue;

        bool selected = _golf_editor_is_entity_selected(i);
        igPushID_Int(i);
        switch (entity->type) {
            case BALL_START_ENTITY:
            case MODEL_ENTITY: 
            case GEO_ENTITY:
            case HOLE_ENTITY: {
                if (igSelectable_Bool(entity->name, selected, ImGuiSelectableFlags_None, (ImVec2){0, 0})) {
                    _golf_editor_select_entity(i);
                }
                if (igBeginDragDropSource(ImGuiDragDropFlags_None)) {
                    igSetDragDropPayload("entity_payload", &i, sizeof(int), ImGuiCond_None);
                    igText("%s", entity->name);
                    igEndDragDropSource();
                }
                break;
            }

            case GROUP_ENTITY: {
                ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow;
                if (selected) {
                    node_flags |= ImGuiTreeNodeFlags_Selected;
                }
                bool is_open = igTreeNodeEx_Str(entity->name, node_flags);
                if (igIsItemClicked(ImGuiMouseButton_Left)) {
                    _golf_editor_select_entity(i);
                }
                if (igBeginDragDropTarget()) {
                    const ImGuiPayload *payload = igAcceptDragDropPayload("entity_payload", ImGuiDragDropFlags_None);
                    if (payload) {
                        int payload_entity_idx = *((int*)payload->Data);
                        golf_entity_t *payload_entity = &editor.level->entities.data[payload_entity_idx];

                        golf_editor_action_t action;
                        _golf_editor_action_init(&action, "Entity join group");
                        _golf_editor_action_push_data(&action, &payload_entity->parent_idx, sizeof(payload_entity->parent_idx));
                        payload_entity->parent_idx = i;
                        _golf_editor_start_action(action);
                        _golf_editor_commit_action();
                    }
                    igEndDragDropTarget();
                }

                if (is_open) {
                    for (int j = 0; j < editor.level->entities.length; j++) {
                        golf_entity_t *child_entity = &editor.level->entities.data[j];
                        if (!child_entity->active) continue;
                        if (child_entity->parent_idx != i) continue;

                        bool selected = _golf_editor_is_entity_selected(j);
                        igPushID_Int(j);
                        if (igSelectable_Bool(child_entity->name, selected, ImGuiSelectableFlags_None, (ImVec2){0, 0})) {
                            _golf_editor_select_entity(j);
                        }
                        igPopID();
                    }
                    igTreePop();
                }
                break;
            }
        }
        igPopID();
    }

    if (igButton("Create Model Entity", (ImVec2){0, 0})) {
        golf_transform_t transform = golf_transform(V3(0, 0, 0), V3(1, 1, 1), QUAT(0, 0, 0, 1));

        float uv_scale = 1.0f;

        golf_lightmap_section_t lightmap_section;
        {
            const char *model_path = "data/models/cube.obj";
            golf_model_t *model = golf_data_get_model(model_path);
            vec_vec2_t uvs;
            vec_init(&uvs, "editor");
            for (int i = 0; i < model->positions.length; i++) {
                vec_push(&uvs, V2(0, 0));
            }

            lightmap_section = golf_lightmap_section("main", uvs);
            golf_lightmap_section_finalize(&lightmap_section);
        }

        golf_movement_t movement;
        movement = golf_movement_none();

        golf_entity_t entity = golf_entity_model("Model", transform, "data/models/cube.obj", uv_scale, lightmap_section, movement);
        _vec_push_and_fix_actions(&editor.level->entities, entity, NULL);
        _golf_editor_commit_entity_create_action();
    }

    /*
    if (igButton("Create Geo Entity", (ImVec2){0, 0})) {
        golf_geo_t geo;
        golf_geo_init_cube(&geo);
        golf_geo_update_model(&geo);
        golf_transform_t transform = golf_transform(V3(0, 0, 0), V3(1, 1, 1), QUAT(0, 0, 0, 1));
        golf_movement_t movement = golf_movement_none();

        golf_lightmap_section_t lightmap_section;
        {
            golf_model_t *model = &geo.model;
            vec_vec2_t uvs;
            vec_init(&uvs, "editor");
            for (int i = 0; i < model->positions.length; i++) {
                vec_push(&uvs, V2(0, 0));
            }

            golf_lightmap_section_init(&lightmap_section, "main", uvs);
            vec_deinit(&uvs);
        }

        golf_entity_t entity = golf_entity_geo("geo", transform, movement, geo, lightmap_section);
        _vec_push_and_fix_actions(&editor.level->entities, entity, NULL);
        _golf_editor_commit_entity_create_action();
    }
    */
    
    if (igButton("Create Group Entity", (ImVec2){0, 0})) {
        golf_transform_t transform = golf_transform(V3(0, 0, 0), V3(1, 1, 1), QUAT(0, 0, 0, 1));
        golf_entity_t entity = golf_entity_group("group", transform);
        _vec_push_and_fix_actions(&editor.level->entities, entity, NULL);
    }
}

void golf_editor_update(float dt) {
    golf_config_t *editor_cfg = golf_data_get_config("data/config/editor.cfg");
    ImGuiIO *IO = igGetIO();
    ImGuiViewport* viewport = igGetMainViewport();
    igSetNextWindowPos(viewport->Pos, ImGuiCond_Always, (ImVec2){ 0, 0 });
    igSetNextWindowSize(viewport->Size, ImGuiCond_Always);
    igSetNextWindowViewport(viewport->ID);
    igSetNextWindowBgAlpha(0);
    igPushStyleVar_Float(ImGuiStyleVar_WindowRounding, 0);
    igPushStyleVar_Vec2(ImGuiStyleVar_WindowPadding, (ImVec2){ 0, 0 });
    igBegin("Dockspace", 0, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);
    igPopStyleVar(2);

    if (igBeginMenuBar())
    {
        if (igBeginMenu("File", true))
        {
            if (igMenuItem_Bool("Open", NULL, false, true)) {
                editor.file_picker.open_popup = true;
                editor.file_picker.type = GOLF_DATA_LEVEL;
                editor.file_picker.path = editor.level_path;
                editor.file_picker.data = (void**)&editor.level;
            }
            if (igMenuItem_Bool("Save", NULL, false, true)) {
                golf_log_note("Saving...");
                golf_level_save(editor.level, editor.level_path);
            }
            if (igMenuItem_Bool("Save As", NULL, false, true)) {
                editor.open_save_as_popup = true;
            }
            if (igMenuItem_Bool("Close", NULL, false, true)) {
            }
            igEndMenu();
        }
        if (igBeginMenu("Edit", true))
        {
            if (igMenuItem_Bool("Undo", NULL, false, true)) {
                _golf_editor_undo_action();
            }
            if (igMenuItem_Bool("Redo", NULL, false, true)) {
                _golf_editor_redo_action();
            }
            if (igBeginMenu("History", true)) {
                int ig_id = 0;
                for (int i = 0; i < editor.undo_actions.length; i++) {
                    igPushID_Int(ig_id++);
                    golf_editor_action_t *action = &editor.undo_actions.data[i];
                    if (igMenuItem_Bool(action->name, NULL, false, true)) {
                    }
                    igPopID();
                }
                igSeparator();
                for (int i = editor.redo_actions.length - 1; i >= 0; i--) {
                    igPushID_Int(ig_id++);
                    golf_editor_action_t *action = &editor.redo_actions.data[i];
                    if (igMenuItem_Bool(action->name, NULL, false, true)) {
                    }
                    igPopID();
                }
                igEndMenu();
            }
            if (igMenuItem_Bool("Duplicate", NULL, false, true)) {
                _golf_editor_duplicate_selected_entities();
            }
            if (igMenuItem_Bool("Copy", NULL, false, true)) {
                _golf_editor_copy_selected_entities();
            }
            if (igMenuItem_Bool("Paste", NULL, false, true)) {
                _golf_editor_paste_selected_entities();
            }
            igEndMenu();
        }

        igText("%s", editor.level_path);
        igEndMenuBar();
    }

    {
        ImGuiID dock_main_id = igGetID_Str("dockspace");
        igDockSpace(dock_main_id, (ImVec2){0, 0}, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoTabBar, NULL);

        static bool built_dockspace = false;
        if (!built_dockspace) {
            built_dockspace = true;

            igDockBuilderRemoveNode(dock_main_id); 
            igDockBuilderAddNode(dock_main_id, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_DockSpace);
            igDockBuilderSetNodeSize(dock_main_id, viewport->Size);

            ImGuiID dock_id_right_top = igDockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.30f, NULL, &dock_main_id);
            ImGuiID dock_id_right_bottom = igDockBuilderSplitNode(dock_id_right_top, ImGuiDir_Down, 0.60f, NULL, &dock_id_right_top);
            ImGuiID dock_id_top = igDockBuilderSplitNode(dock_main_id, ImGuiDir_Up, 0.05f, NULL, &dock_main_id);

            igDockBuilderDockWindow("Viewport", dock_main_id);
            igDockBuilderDockWindow("Top", dock_id_top);
            igDockBuilderDockWindow("RightTop", dock_id_right_top);
            igDockBuilderDockWindow("RightBottom", dock_id_right_bottom);
            igDockBuilderFinish(dock_main_id);
        }

        ImGuiDockNode *central_node = igDockBuilderGetCentralNode(dock_main_id);
        editor.viewport_pos = V2(central_node->Pos.x, central_node->Pos.y);
        editor.viewport_size = V2(central_node->Size.x, central_node->Size.y);
    }

    for (int i = 0; i < editor.selected_idxs.length; i++) {
        int idx = editor.selected_idxs.data[i];
        if (!editor.level->entities.data[idx].active) {
            vec_splice(&editor.selected_idxs, i, 1);
            i--;
        }
    }

    if (!editor.gizmo.is_active) {
        if (editor.in_edit_mode) {
            editor.gizmo.is_on = false;

            golf_geo_t *geo = editor.edit_mode.geo;
            mat4 model_mat = golf_transform_get_model_mat(editor.edit_mode.transform);
            editor.edit_mode.point_idxs.length = 0;
            for (int i = 0; i < editor.edit_mode.selected_entities.length; i++) {
                golf_edit_mode_entity_t *entity = &editor.edit_mode.selected_entities.data[i];
                switch (entity->type) {
                    case GOLF_EDIT_MODE_ENTITY_FACE: {
                        golf_geo_face_t face = geo->faces.data[entity->idx];
                        for (int i = 0; i < face.idx.length; i++) {
                            int idx = face.idx.data[i];
                            _golf_editor_push_unique_idx(&editor.edit_mode.point_idxs, idx);
                        }
                        break;
                    }
                    case GOLF_EDIT_MODE_ENTITY_LINE:
                        _golf_editor_push_unique_idx(&editor.edit_mode.point_idxs, entity->idx);
                        _golf_editor_push_unique_idx(&editor.edit_mode.point_idxs, entity->idx2);
                        break;
                    case GOLF_EDIT_MODE_ENTITY_POINT:
                        _golf_editor_push_unique_idx(&editor.edit_mode.point_idxs, entity->idx);
                        break;
                }
            }

            if (editor.edit_mode.point_idxs.length > 0) {
                editor.gizmo.is_on = true;
                editor.gizmo.transform.position = V3(0, 0, 0);
                editor.gizmo.transform.rotation = editor.edit_mode.transform.rotation;
                editor.gizmo.transform.scale = V3(1, 1, 1);
                for (int i = 0; i < editor.edit_mode.point_idxs.length; i++) {
                    int idx = editor.edit_mode.point_idxs.data[i];
                    golf_geo_point_t *p = &geo->points.data[idx];
                    vec3 pos = vec3_apply_mat4(p->position, 1, model_mat);
                    editor.gizmo.transform.position = vec3_add(editor.gizmo.transform.position, pos);
                }
                int n = editor.edit_mode.point_idxs.length;
                editor.gizmo.transform.position = vec3_scale(editor.gizmo.transform.position, 1.0f / n);
            }

            if (inputs->mouse_down[SAPP_MOUSEBUTTON_LEFT] && vec2_length(inputs->screen_mouse_down_delta) > 10) {
                editor.select_box.is_open = true;
            }

            if (editor.select_box.is_open) {
                igCaptureMouseFromApp(true);
                if (!inputs->mouse_down[SAPP_MOUSEBUTTON_LEFT]) {
                    if (!inputs->button_down[SAPP_KEYCODE_LEFT_SHIFT]) {
                        editor.edit_mode.selected_entities.length = 0;
                    }

                    for (int i = 0; i < editor.select_box.hovered_entities.length; i++) {
                        golf_edit_mode_entity_t entity = editor.select_box.hovered_entities.data[i];
                        if (!golf_editor_is_edit_entity_selected(entity)) {
                            vec_push(&editor.edit_mode.selected_entities, entity);
                        }
                    }

                    editor.select_box.is_open = false;
                }

                vec2 p0 = inputs->screen_mouse_down_pos;
                vec2 p1 = inputs->screen_mouse_pos;
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
                editor.select_box.p0 = p0;
                editor.select_box.p1 = p1;

                float h_width = 0.5f * (p1.x -  p0.x);
                float h_height = 0.5f * (p1.y -  p0.y);
                vec3 box_center = V3(p0.x + h_width, p0.y + h_height, 0);
                vec3 box_half_lengths = V3(h_width, h_height, 1);

                editor.select_box.hovered_entities.length = 0;
                for (int i = 0; i < geo->points.length; i++) {
                    golf_geo_point_t point = geo->points.data[i];
                    if (!point.active) continue;

                    vec3 p = vec3_apply_mat4(geo->points.data[i].position, 1, model_mat);
                    vec2 p_screen = golf_graphics_world_to_screen(p);
                    vec3 p_screen3 = V3(p_screen.x, graphics->window_size.y - p_screen.y, 0);
                    if (point_inside_box(p_screen3, box_center, box_half_lengths)) {
                        vec_push(&editor.select_box.hovered_entities, golf_edit_mode_entity_point(i));
                    }
                }

                {
                    vec_vec3_t triangle_points;
                    vec_int_t triangle_idxs;
                    vec_bool_t is_inside;
                    vec_init(&triangle_points, "editor");
                    vec_init(&triangle_idxs, "editor");
                    vec_init(&is_inside, "editor");

                    for (int i = 0; i < geo->faces.length; i++) {
                        golf_geo_face_t face = geo->faces.data[i];
                        if (!face.active) continue;

                        for (int j = 1; j < face.idx.length - 1; j++) {
                            vec3 p0 = geo->points.data[face.idx.data[0]].position;
                            vec3 p1 = geo->points.data[face.idx.data[j]].position;
                            vec3 p2 = geo->points.data[face.idx.data[j + 1]].position;
                            p0 = vec3_apply_mat4(p0, 1, model_mat);
                            p1 = vec3_apply_mat4(p1, 1, model_mat);
                            p2 = vec3_apply_mat4(p2, 1, model_mat);
                            vec_push(&triangle_points, p0);
                            vec_push(&triangle_points, p1);
                            vec_push(&triangle_points, p2);
                            vec_push(&triangle_idxs, i);
                            vec_push(&is_inside, false);
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
                        frustum_corners[i] = golf_graphics_screen_to_world(frustum_corners[i]);
                    }
                    triangles_inside_frustum(triangle_points.data, triangle_points.length / 3,
                            frustum_corners, is_inside.data);
                    for (int i = 0; i < is_inside.length; i++) {
                        if (!is_inside.data[i]) {
                            continue;
                        }

                        int face_idx = triangle_idxs.data[i];
                        golf_edit_mode_entity_t entity = golf_edit_mode_entity_face(face_idx);
                        vec_push(&editor.select_box.hovered_entities, entity);
                    }

                    vec_deinit(&triangle_points);
                    vec_deinit(&triangle_idxs);
                    vec_deinit(&is_inside);
                }

                {
                    ImVec2 im_p0 = (ImVec2){p0.x, graphics->window_size.y - p0.y};
                    ImVec2 im_p1 = (ImVec2){p0.x, graphics->window_size.y - p1.y};
                    ImVec2 im_p2 = (ImVec2){p1.x, graphics->window_size.y - p1.y};
                    ImVec2 im_p3 = (ImVec2){p1.x, graphics->window_size.y - p0.y};
                    ImU32 im_col = igGetColorU32_Vec4((ImVec4){1, 1, 1, 1});
                    float radius = 1;
                    ImDrawList *draw_list = igGetWindowDrawList();
                    ImDrawList_AddLine(draw_list, im_p0, im_p1, im_col, radius);
                    ImDrawList_AddLine(draw_list, im_p1, im_p2, im_col, radius);
                    ImDrawList_AddLine(draw_list, im_p2, im_p3, im_col, radius);
                    ImDrawList_AddLine(draw_list, im_p3, im_p0, im_col, radius);
                }
            }
        }
        else {
            editor.gizmo.is_on = false;
            for (int i = 0; i < editor.selected_idxs.length; i++) {
                int idx = editor.selected_idxs.data[i];
                golf_entity_t *entity = &editor.level->entities.data[idx];
                golf_transform_t transform = golf_entity_get_world_transform(editor.level, entity);
                golf_gizmo_set_transform(&editor.gizmo, transform);
                editor.gizmo.is_on = true;
            }
        }
    }

    {
        bool is_active = editor.gizmo.is_active;
        golf_gizmo_update(&editor.gizmo, igGetWindowDrawList());

        if (is_active) {
            golf_transform_t delta_transform = editor.gizmo.delta_transform;

            if (editor.in_edit_mode) {
                mat4 model_mat = golf_transform_get_model_mat(editor.edit_mode.transform);
                mat4 inv_model_mat = mat4_inverse(model_mat);

                vec3 local_pos = vec3_apply_mat4(editor.gizmo.transform.position, 1, inv_model_mat);
                vec3 local_delta_pos = vec3_apply_mat4(delta_transform.position, 0, inv_model_mat);
                quat delta_rot = delta_transform.rotation;
                mat4 delta_rot_mat = mat4_from_quat(delta_rot);

                for (int i = 0; i < editor.edit_mode.point_idxs.length; i++) {
                    int idx = editor.edit_mode.point_idxs.data[i];
                    vec3 starting_position = editor.edit_mode.starting_positions.data[i];
                    vec3 dir = vec3_apply_mat4(vec3_sub(starting_position, local_pos), 1, delta_rot_mat);
                    golf_geo_point_t *point = &editor.edit_mode.geo->points.data[idx];
                    point->position = vec3_add(vec3_add(local_pos, dir), local_delta_pos);
                }
            }
            else {
                int starting_transform_idx = 0;
                for (int i = 0; i < editor.selected_idxs.length; i++) {
                    int idx = editor.selected_idxs.data[i];
                    golf_entity_t *entity = &editor.level->entities.data[idx];
                    golf_transform_t *transform = golf_entity_get_transform(entity);
                    if (transform) {
                        golf_transform_t starting_transform = editor.starting_transforms.data[starting_transform_idx++];
                        transform->position = vec3_add(starting_transform.position, delta_transform.position);
                        transform->rotation = quat_multiply(delta_transform.rotation, starting_transform.rotation);
                        transform->scale = vec3_add(starting_transform.scale, delta_transform.scale);
                    }
                }
            }
        }

        if (!is_active && editor.gizmo.is_active) {
            golf_editor_action_t action;
            _golf_editor_action_init(&action, "Modify transform");

            if (editor.in_edit_mode) {
                editor.edit_mode.starting_positions.length = 0;
                for (int i = 0; i < editor.edit_mode.point_idxs.length; i++) {
                    int idx = editor.edit_mode.point_idxs.data[i];
                    golf_geo_point_t *point = &editor.edit_mode.geo->points.data[idx];
                    vec_push(&editor.edit_mode.starting_positions, point->position);
                    _golf_editor_action_push_data(&action, &point->position, sizeof(point->position));
                }
            }
            else {
                editor.starting_transforms.length = 0;
                for (int i = 0; i < editor.selected_idxs.length; i++) {
                    int idx = editor.selected_idxs.data[i];
                    golf_entity_t *entity = &editor.level->entities.data[idx];
                    golf_transform_t *transform = golf_entity_get_transform(entity);
                    if (transform) {
                        vec_push(&editor.starting_transforms, *transform);
                        _golf_editor_action_push_data(&action, transform, sizeof(golf_transform_t));
                    }
                }
            }

            _golf_editor_queue_start_action(action);
        }
        if (is_active && !editor.gizmo.is_active) {
            _golf_editor_queue_commit_action();
        }
    }

    {
        igBegin("Top", NULL, ImGuiWindowFlags_NoTitleBar);

        // Translate Mode Button
        {
            bool is_on = editor.gizmo.operation == GOLF_GIZMO_TRANSLATE;
            if (is_on) {
                igPushStyleColor_U32(ImGuiCol_Button, igGetColorU32_Col(ImGuiCol_ButtonActive, 1));
            }
            if (igButton(ICON_FA_ARROWS_ALT, (ImVec2){20, 20})) {
                golf_gizmo_set_operation(&editor.gizmo, GOLF_GIZMO_TRANSLATE);
            }
            if (is_on) {
                igPopStyleColor(1);
            }
            if (igIsItemHovered(ImGuiHoveredFlags_None)) {

                igBeginTooltip();
                igText("Translate (W)");
                igEndTooltip();
            }
        }

        // Rotate Mode Button
        {
            igSameLine(0, 2);
            bool is_on = editor.gizmo.operation == GOLF_GIZMO_ROTATE;
            if (is_on) {
                igPushStyleColor_U32(ImGuiCol_Button, igGetColorU32_Col(ImGuiCol_ButtonActive, 1));
            }
            if (igButton(ICON_FA_SYNC_ALT, (ImVec2){20, 20})) {
                golf_gizmo_set_operation(&editor.gizmo, GOLF_GIZMO_ROTATE);
            }
            if (is_on) {
                igPopStyleColor(1);
            }
            if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                igBeginTooltip();
                igText("Rotate (E)");
                igEndTooltip();
            }
        }

        // Scale Mode Button
        {
            igSameLine(0, 2);
            bool is_on = editor.gizmo.operation == GOLF_GIZMO_SCALE;
            if (is_on) {
                igPushStyleColor_U32(ImGuiCol_Button, igGetColorU32_Col(ImGuiCol_ButtonActive, 1));
            }
            if (igButton(ICON_FA_EXPAND_ALT, (ImVec2){20, 20})) {
                golf_gizmo_set_operation(&editor.gizmo, GOLF_GIZMO_SCALE);
            }
            if (is_on) {
                igPopStyleColor(1);
            }
            if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                igBeginTooltip();
                igText("Scale (R)");
                igEndTooltip();
            }
        }

        // Local / Global Button
        {
            igSameLine(0, 10);
            if (editor.gizmo.mode == GOLF_GIZMO_LOCAL) {
                if (igButton(ICON_FA_CUBE, (ImVec2){20, 20})) {
                    golf_gizmo_set_mode(&editor.gizmo, GOLF_GIZMO_WORLD);
                }
                if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                    igBeginTooltip();
                    igText("Local");
                    igEndTooltip();
                }
            }
            else if (editor.gizmo.mode == GOLF_GIZMO_WORLD) {
                if (igButton(ICON_FA_GLOBE_ASIA, (ImVec2){20, 20})) {
                    golf_gizmo_set_mode(&editor.gizmo, GOLF_GIZMO_LOCAL);
                }
                if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                    igBeginTooltip();
                    igText("Global");
                    igEndTooltip();
                }
            }
        }

        // Snap Button
        {
            igSameLine(0, 1);
            static float *v = NULL;
            if (editor.gizmo.operation == GOLF_GIZMO_TRANSLATE) {
                v = &editor.gizmo.translate.snap;
            }
            else if (editor.gizmo.operation == GOLF_GIZMO_SCALE) {
                v = &editor.gizmo.scale.snap;
            }
            else if (editor.gizmo.operation == GOLF_GIZMO_ROTATE) {
                v = &editor.gizmo.rotate.snap;
            }
            else {
                golf_log_error("Invalid value for gizmo operation %d", editor.gizmo.operation);
            }

            igPushItemWidth(100);
            igInputFloat("", v, 0, 0, "%.3f", ImGuiInputTextFlags_AutoSelectAll);
            igPopItemWidth();
        }

        // Settings Button
        {
            igSameLine(0, 10);
            if (igButton(ICON_FA_COG, (ImVec2){20, 20})) {
                //editor.gizmo.use_snap = !editor.gizmo.use_snap;
                igOpenPopup_Str("settings_popup", ImGuiPopupFlags_None);
            }
            if (igBeginPopup("settings_popup", ImGuiWindowFlags_None)) {
                if (igTreeNode_Str("Global Illumination")) {
                    igPushItemWidth(75);

                    _golf_editor_undoable_igInputInt("Num Iterations", &editor.gi_state.num_iterations, "Modify GI Settings - Num Iterations");
                    _golf_editor_undoable_igInputInt("Num Dilates", &editor.gi_state.num_dilates, "Modify GI Settings - Num Dilates");
                    _golf_editor_undoable_igInputInt("Num Smooths", &editor.gi_state.num_smooths, "Modify GI Settings - Num Smooths");
                    _golf_editor_undoable_igInputFloat("Gamma", &editor.gi_state.gamma, "Modify GI Settigs - Gamma");

                    _golf_editor_undoable_igInputInt("Hemisphere Size", &editor.gi_state.hemisphere_size, "Modify GI Settings - Hemisphere Size");
                    if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                        igBeginTooltip();
                        igText("Resolution of the hemisphere renderings. must be a power of two! typical: 64.");
                        igEndTooltip();
                    }

                    _golf_editor_undoable_igInputFloat("Z Near", &editor.gi_state.z_near, "Modify GI Settings - Z Near");
                    if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                        igBeginTooltip();
                        igText("Hemisphere min draw distances.");
                        igEndTooltip();
                    }

                    _golf_editor_undoable_igInputFloat("Z Far", &editor.gi_state.z_far, "Modify GI Settings - Z Far");
                    if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                        igBeginTooltip();
                        igText("Hemisphere max draw distances.");
                        igEndTooltip();
                    }

                    _golf_editor_undoable_igInputInt("Interpolation Passes", &editor.gi_state.interpolation_passes, "Modify GI Settings - Interpolation Passes");
                    if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                        igBeginTooltip();
                        igText("Hierarchical selective interpolation passes (0-8; initial step size = 2^passes).");
                        igEndTooltip();
                    }

                    _golf_editor_undoable_igInputFloat("Interpolation Threshold", &editor.gi_state.interpolation_threshold, "Modify GI Settings - Interpolation Threshold");
                    if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                        igBeginTooltip();
                        igText("Error value below which lightmap pixels are interpolated instead of rendered.");
                        igText("Use output image from LM_DEBUG_INTERPOLATION to determine a good value.");
                        igText("Values around and below 0.01 are probably ok.");
                        igText("The lower the value, the more hemispheres are rendered -> sower, but possibly better quality.");
                        igEndTooltip();
                    }

                    _golf_editor_undoable_igInputFloat("Camera to Surface Distance Modifier", &editor.gi_state.camera_to_surface_distance_modifier, "Modify GI Settings - Camera to Surface Distance Modifier");
                    if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                        igBeginTooltip();
                        igText("Modifier for the height of the rendered hemispheres above the sruface.");
                        igText("-1.0f => stick to surface.");
                        igText("0.0f => minimum height for interpolated surface normals.");
                        igText("> 0.0f => improves gradients on surfaces with interpolated normals due to the flat surface horizion.");
                        igText("But may introduce other artifacts.");
                        igEndTooltip();
                    }

                    if (igButton("Bake Lightmaps", (ImVec2){0, 0})) {
                        if (!editor.gi_running) {
                            golf_log_note("Lightmap Generator Started");
                            golf_gi_t *gi = &editor.gi;  
                            golf_gi_init(gi, true, true, 
                                    editor.gi_state.gamma,
                                    editor.gi_state.num_iterations,
                                    editor.gi_state.num_dilates,
                                    editor.gi_state.num_smooths,
                                    editor.gi_state.hemisphere_size,
                                    editor.gi_state.z_near,
                                    editor.gi_state.z_far,
                                    editor.gi_state.interpolation_passes,
                                    editor.gi_state.interpolation_threshold,
                                    editor.gi_state.camera_to_surface_distance_modifier);

                            for (int i = 0; i < editor.level->lightmap_images.length; i++) {
                                golf_lightmap_image_t *lightmap = &editor.level->lightmap_images.data[i];
                                if (!lightmap->active) continue;

                                golf_gi_start_lightmap(gi, lightmap);
                                for (int i = 0; i < editor.level->entities.length; i++) {
                                    golf_entity_t *entity = &editor.level->entities.data[i];
                                    if (!entity->active) continue;

                                    golf_lightmap_section_t *lightmap_section = golf_entity_get_lightmap_section(entity);
                                    golf_transform_t *transform = golf_entity_get_transform(entity);
                                    golf_model_t *model = golf_entity_get_model(entity);
                                    golf_movement_t *movement = golf_entity_get_movement(entity);
                                    if (lightmap_section && transform && model && movement &&
                                            strcmp(lightmap_section->lightmap_name, lightmap->name) == 0) {
                                        golf_transform_t world_transform = golf_entity_get_world_transform(editor.level, entity);
                                        golf_gi_add_lightmap_section(gi, lightmap_section, model, world_transform, *movement);
                                    }
                                }
                                golf_gi_end_lightmap(gi);
                            }

                            golf_gi_start(gi);
                            editor.gi_running = true;
                            editor.gi_state.open_popup = true;
                            editor.gi_state.creating_hole = false;
                        }
                    }

                    if (igButton("Create Hole", (ImVec2){0, 0})) {
                        if (!editor.gi_running) {
                            /*
                            golf_log_note("Creating hole model and lightmaps");
                            golf_gi_t *gi = &editor.gi;
                            golf_gi_init(gi, true, true, 
                                    editor.gi_state.gamma,
                                    editor.gi_state.num_iterations,
                                    editor.gi_state.num_dilates,
                                    editor.gi_state.num_smooths,
                                    editor.gi_state.hemisphere_size,
                                    editor.gi_state.z_near,
                                    editor.gi_state.z_far,
                                    editor.gi_state.interpolation_passes,
                                    editor.gi_state.interpolation_threshold,
                                    editor.gi_state.camera_to_surface_distance_modifier);
                            {
                                golf_model_t *model = golf_data_get_model("data/models/hole.obj");
                                mat4 model_mat = mat4_identity();
                                golf_gi_add_entity(gi, model, model_mat, &editor.gi_state.hole_lightmap);
                            }
                            golf_gi_start(gi);
                            editor.gi_running = true;
                            editor.gi_state.open_popup = true;
                            editor.gi_state.creating_hole = true;
                            */
                        }
                    }

                    igPopItemWidth();
                    igTreePop();
                }
                if (igTreeNode_Str("Renderer")) {
                    igCheckbox("GI On", &editor.renderer.gi_on);
                    igTreePop();
                }
                igEndPopup();
            }
            if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                igBeginTooltip();
                igText("Settings");
                igEndTooltip();
            }
        }

        igEnd();
    }

    igBegin("RightTop", NULL, ImGuiWindowFlags_NoTitleBar);
    if (igBeginTabBar("", ImGuiTabBarFlags_None)) {
        if (editor.in_edit_mode) {
            if (igBeginTabItem("Geo", NULL, ImGuiTabItemFlags_None)) {
                _golf_editor_geo_tab();
                igEndTabItem();
            }
        }
        else {
            if (igBeginTabItem("Entities", NULL, ImGuiTabItemFlags_None)) {
                _golf_editor_entities_tab();
                igEndTabItem();
            }
        }

        if (igBeginTabItem("Materials", NULL, ImGuiTabItemFlags_None)) {
            for (int i = 0; i < editor.level->materials.length; i++) {
                golf_material_t *material = &editor.level->materials.data[i];
                if (!material->active) {
                    continue;
                }

                igPushID_Int(i);
                if (igTreeNodeEx_StrStr("material", ImGuiTreeNodeFlags_None, "%s", material->name)) {
                    golf_material_type_t material_type_before = material->type;
                    const char *items[] = { "Texture", "Color", "Diffuse", "Environment" };
                    igCombo_Str_arr("Type", (int*)&material->type, items, sizeof(items) / sizeof(items[0]), 0);
                    if (material_type_before != material->type) {
                        golf_material_type_t material_type_after = material->type;

                        material->type = material_type_before;
                        _golf_editor_start_action_with_data(material, sizeof(golf_material_t), "Change material type");
                        _golf_editor_commit_action();
                        material->type = material_type_after;

                        switch (material->type) {
                            case GOLF_MATERIAL_TEXTURE: {
                                snprintf(material->texture_path, GOLF_FILE_MAX_PATH, "%s", "data/textures/fallback.png");
                                material->texture = golf_data_get_texture("data/textures/fallback.png");
                                break;
                            }
                            case GOLF_MATERIAL_COLOR: {
                                material->color = V4(1, 0, 0, 1);
                                break;
                            }
                            case GOLF_MATERIAL_DIFFUSE_COLOR: {
                                material->color = V4(1, 0, 0, 1);
                                break;
                            }
                            case GOLF_MATERIAL_ENVIRONMENT: {
                                snprintf(material->texture_path, GOLF_FILE_MAX_PATH, "%s", "data/textures/fallback.png");
                                material->texture = golf_data_get_texture("data/textures/fallback.png");
                                break;
                            }
                        }
                    }

                    _golf_editor_undoable_igInputText("Name", material->name, GOLF_MAX_NAME_LEN, NULL, NULL, 0, "Modify material name");

                    switch (material->type) {
                        case GOLF_MATERIAL_TEXTURE: {
                            _golf_editor_file_picker("Texture", GOLF_DATA_TEXTURE, material->texture_path, (void**)&material->texture);
                            break;
                        }
                        case GOLF_MATERIAL_COLOR: {
                            _golf_editor_undoable_igInputFloat3("Color", (float*)&material->color, "Modify material color");
                            break;
                        }
                        case GOLF_MATERIAL_DIFFUSE_COLOR: {
                            _golf_editor_undoable_igInputFloat3("Color", (float*)&material->color, "Modify material color");
                            break;
                        }
                        case GOLF_MATERIAL_ENVIRONMENT: {
                            _golf_editor_file_picker("Texture", GOLF_DATA_TEXTURE, material->texture_path, (void**)&material->texture);
                            break;
                        }
                    }

                    _golf_editor_undoable_igInputFloat("Friction", &material->friction, "Modify material friction");
                    _golf_editor_undoable_igInputFloat("Restitution", &material->restitution, "Modify material restitution");
                    _golf_editor_undoable_igInputFloat("Velocity Scale", &material->vel_scale, "Modify material vel_scale");

                    if (igButton("Delete Material", (ImVec2){0, 0})) {
                        _golf_editor_start_action_with_data(&material->active, sizeof(material->active), "Delete material");
                        material->active = false;
                        _golf_editor_commit_action();
                    }
                    igTreePop();
                }
                igPopID();
            }

            if (igButton("Create Material", (ImVec2){0, 0})) {
                golf_material_t new_material;
                memset(&new_material, 0, sizeof(golf_material_t));
                new_material.active = true;
                snprintf(new_material.name, GOLF_MAX_NAME_LEN, "%s", "new");
                new_material.type = GOLF_MATERIAL_COLOR;
                new_material.color = V4(1, 0, 0, 1);
                _vec_push_and_fix_actions(&editor.level->materials, new_material, NULL);

                golf_material_t *material = &vec_last(&editor.level->materials);
                material->active = false;
                _golf_editor_start_action_with_data(&material->active, sizeof(material->active), "Create material");
                material->active = true;
                _golf_editor_commit_action();
            }
            igEndTabItem();
        }

        if (igBeginTabItem("Lightmap Images", NULL, ImGuiTabItemFlags_None)) {
            for (int i = 0; i < editor.level->lightmap_images.length; i++) {
                golf_lightmap_image_t *lightmap_image = &editor.level->lightmap_images.data[i];
                if (!lightmap_image->active) continue;

                bool queue_action = false;
                bool queue_commit = false;
                bool queue_decommit = false;

                igPushID_Int(i);
                if (igTreeNodeEx_StrStr("lightmap_image", ImGuiTreeNodeFlags_None, "%s", lightmap_image->name)) {
                    _golf_editor_undoable_igInputText("Name", lightmap_image->name, GOLF_MAX_NAME_LEN, NULL, NULL, 0, "Modify lightmap image name");
                    igInputInt("Resolution", &lightmap_image->resolution, 0, 0, ImGuiInputTextFlags_None);
                    if (igIsItemActivated()) {
                        queue_action = true;
                    }
                    if (igIsItemDeactivated()) {
                        if (igIsItemDeactivatedAfterEdit()) {
                            queue_commit = true;
                        }
                        else {
                            queue_decommit = true;
                        }
                    }

                    _golf_editor_undoable_igInputFloat("Time Length", &lightmap_image->time_length, "Modify lightmap time length");

                    igInputInt("Num Samples", &lightmap_image->edited_num_samples, 0, 0, ImGuiInputTextFlags_None);
                    if (igIsItemActivated()) {
                        queue_action = true;
                    }
                    if (igIsItemDeactivated()) {
                        if (igIsItemDeactivatedAfterEdit()) {
                            queue_commit = true;
                        }
                        else {
                            queue_decommit = true;
                        }
                    }

                    igText("Size <%d, %d>", lightmap_image->width, lightmap_image->height);
                    for (int i = 0; i < lightmap_image->num_samples; i++) {
                        igImage((ImTextureID)(uintptr_t)lightmap_image->sg_image[i].id, 
                                (ImVec2){(float)lightmap_image->width, (float)lightmap_image->height},
                                (ImVec2){0, 0},
                                (ImVec2){1, 1}, 
                                (ImVec4){1, 1, 1, 1},
                                (ImVec4){1, 1, 1, 1});
                    }

                    if (igButton("Delete", (ImVec2){0, 0})) {
                        _golf_editor_start_action_with_data(&lightmap_image->active, sizeof(lightmap_image->active), "Delete lightmap image");
                        lightmap_image->active = false;
                        _golf_editor_commit_action();
                    }

                    igTreePop();
                }
                igPopID();

                if (queue_action) {
                    golf_editor_action_t action;
                    _golf_editor_action_init(&action, "Modify lightmap image");
                    _golf_editor_action_push_data(&action, lightmap_image, sizeof(golf_lightmap_image_t));
                    _golf_editor_queue_start_action(action);
                }
                if (queue_commit) {
                    int resolution = lightmap_image->resolution;
                    int width = lightmap_image->width;
                    int height = lightmap_image->height;
                    float time_length = lightmap_image->time_length;
                    int num_samples = lightmap_image->edited_num_samples;
                    unsigned char **data = golf_alloc(sizeof(unsigned char*) * num_samples);
                    for (int i = 0; i < num_samples; i++) {
                        data[i] = golf_alloc(lightmap_image->width * lightmap_image->height);
                        memset(data[i], 0xFF, lightmap_image->width * lightmap_image->height);
                    }

                    char name[GOLF_MAX_NAME_LEN];
                    snprintf(name, GOLF_MAX_NAME_LEN, "%s", lightmap_image->name);

                    sg_image *sg_images = golf_alloc(sizeof(sg_image) * num_samples);

                    *lightmap_image = golf_lightmap_image(name, resolution, width, height, time_length, num_samples, data, sg_images);
                    golf_lightmap_image_finalize(lightmap_image);

                    _golf_editor_queue_commit_action();
                }
                if (queue_decommit) {
                    _golf_editor_queue_decommit_action();
                }
            }

            if (igButton("Create Lightmap Image", (ImVec2){0, 0})) {
                unsigned char **image_data = golf_alloc(sizeof(unsigned char*) * 1);
                image_data[0] = golf_alloc(sizeof(unsigned char) * 1);
                image_data[0][0] = 0xFF;
                sg_image *sg_images = golf_alloc(sizeof(sg_image) * 1);
                golf_lightmap_image_t new_lightmap_image = golf_lightmap_image("new", 256, 1, 1, 1, 1, image_data, sg_images);
                golf_lightmap_image_finalize(&new_lightmap_image);
                _vec_push_and_fix_actions(&editor.level->lightmap_images, new_lightmap_image, NULL);

                golf_lightmap_image_t *lightmap_image = &vec_last(&editor.level->lightmap_images);
                lightmap_image->active = false;
                _golf_editor_start_action_with_data(&lightmap_image->active, sizeof(lightmap_image->active), "Create lightmap image");
                lightmap_image->active = true;
                _golf_editor_commit_action();
            }
            igEndTabItem();
        }

        igEndTabBar();
    }
    igEnd();

    igBegin("RightBottom", NULL, ImGuiWindowFlags_NoTitleBar);
    if (editor.in_edit_mode) {
        golf_geo_t *geo = editor.edit_mode.geo;
        for (int i = 0; i < editor.edit_mode.selected_entities.length; i++) {
            if (i == 0) {
                igSetNextItemOpen(true, ImGuiCond_Once);
            }

            golf_edit_mode_entity_t entity = editor.edit_mode.selected_entities.data[i];
            igPushID_Int(i);
            switch (entity.type) {
                case GOLF_EDIT_MODE_ENTITY_FACE: {
                    if (igTreeNode_Str("Face")) {
                        golf_geo_face_t *face = &geo->faces.data[entity.idx];
                        _golf_editor_undoable_igInputText("Material", face->material_name, GOLF_MAX_NAME_LEN, NULL, NULL, 0, "Modify face material name");
                        if (igButton("Permeate Material Name", (ImVec2){0, 0})) {
                            golf_editor_action_t action;
                            _golf_editor_action_init(&action, "Permeate material name");
                            for (int j = 0; j < editor.edit_mode.selected_entities.length; j++) {
                                if (i == j) continue;

                                golf_edit_mode_entity_t other_entity = editor.edit_mode.selected_entities.data[j];
                                if (other_entity.type == GOLF_EDIT_MODE_ENTITY_FACE) {
                                    golf_geo_face_t *other_face = &geo->faces.data[other_entity.idx];
                                    _golf_editor_action_push_data(&action, other_face->material_name, sizeof(other_face->material_name));
                                    memcpy(other_face->material_name, face->material_name, sizeof(other_face->material_name));
                                }
                            }
                            _golf_editor_start_action(action);
                            _golf_editor_commit_action();
                        }

                        igText("Num points: %d", face->idx.length);
                        for (int i = 0; i < face->idx.length; i++) {
                            golf_geo_point_t *p = &geo->points.data[face->idx.data[i]];
                            igPushID_Int(i);
                            _golf_editor_undoable_igInputFloat3("Position", (float*)&p->position, "Modify point position");
                            igPopID();
                        }
                        {
                            golf_geo_face_uv_gen_type_t type_before = face->uv_gen_type;
                            const char **items = golf_geo_uv_gen_type_strings();
                            int count = GOLF_GEO_FACE_UV_GEN_COUNT;
                            igCombo_Str_arr("UV Gen Type", (int*)&face->uv_gen_type, items, count, 0);
                            if (type_before != face->uv_gen_type) {
                                golf_geo_face_uv_gen_type_t type_after = face->uv_gen_type;
                                face->uv_gen_type = type_before;
                                _golf_editor_start_action_with_data(&face->uv_gen_type, sizeof(face->uv_gen_type), "Change face uv gen type");
                                _golf_editor_commit_action();
                                face->uv_gen_type = type_after;
                            }
                            if (igButton("Permeate UV Gen Type", (ImVec2){0, 0})) {
                                golf_editor_action_t action;
                                _golf_editor_action_init(&action, "Permeate uv gen type");
                                for (int j = 0; j < editor.edit_mode.selected_entities.length; j++) {
                                    if (i == j) continue;

                                    golf_edit_mode_entity_t other_entity = editor.edit_mode.selected_entities.data[j];
                                    if (other_entity.type == GOLF_EDIT_MODE_ENTITY_FACE) {
                                        golf_geo_face_t *other_face = &geo->faces.data[other_entity.idx];
                                        _golf_editor_action_push_data(&action, &other_face->uv_gen_type, sizeof(other_face->uv_gen_type));
                                        other_face->uv_gen_type = face->uv_gen_type;
                                    }
                                }
                                _golf_editor_start_action(action);
                                _golf_editor_commit_action();
                            }
                        }
                        for (int i = 0; i < face->uvs.length; i++) {
                            vec2 *uv = &face->uvs.data[i];
                            igPushID_Int(i);
                            _golf_editor_undoable_igInputFloat2("UV", (float*)uv, "Modify point uv");
                            igPopID();
                        }
                        igTreePop();
                    }
                    break;
                }
                case GOLF_EDIT_MODE_ENTITY_LINE: {
                    if (igTreeNode_Str("Line")) {
                        golf_geo_point_t *p0 = &geo->points.data[entity.idx];
                        golf_geo_point_t *p1 = &geo->points.data[entity.idx2];
                        _golf_editor_undoable_igInputFloat3("P0", (float*)&p0->position, "Modify point position");
                        _golf_editor_undoable_igInputFloat3("P1", (float*)&p1->position, "Modify point position");
                        igTreePop();
                    }
                    break;
                }
                case GOLF_EDIT_MODE_ENTITY_POINT: {
                    if (igTreeNode_Str("Point")) {
                        golf_geo_point_t *p = &geo->points.data[entity.idx];
                        _golf_editor_undoable_igInputFloat3("Position", (float*)&p->position, "Modify point position");
                        igTreePop();
                    }
                    break;
                }
            }
            igPopID();
        }
    }
    else {
        if (editor.selected_idxs.length > 0) {
            int idx = editor.selected_idxs.data[0];
            golf_entity_t *entity = &editor.level->entities.data[idx];
            switch (entity->type) {
                case MODEL_ENTITY: {
                    igText("TYPE: Model");
                    break;
                }
                case BALL_START_ENTITY: {
                    igText("TYPE: Ball Start");
                    break;
                }
                case HOLE_ENTITY: {
                    igText("TYPE: Hole");
                    break;
                }
                case GEO_ENTITY: {
                    igText("TYPE: Geo");
                    break;
                }
                case GROUP_ENTITY: {
                    igText("TYPE: Group");
                    break;
                }
            }
            igText("IDX: %d", idx);

            bool edit_done = false;
            _golf_editor_undoable_igInputText("Name", entity->name, GOLF_MAX_NAME_LEN, &edit_done, NULL, 0, "Modify entity name");

            if (entity->type == MODEL_ENTITY) {
                _golf_editor_file_picker("Model", GOLF_DATA_MODEL, entity->model.model_path, (void**)&entity->model.model);
                _golf_editor_undoable_igInputFloat("UV Scale", &entity->model.uv_scale, "Modify model uv scale");
            }

            golf_transform_t *transform = golf_entity_get_transform(entity);
            if (transform) {
                _golf_editor_edit_transform(transform);
            }

            golf_lightmap_section_t *lightmap_section = golf_entity_get_lightmap_section(entity);
            if (lightmap_section) {
                _golf_editor_edit_lightmap_section(lightmap_section);
            }

            golf_movement_t *movement = golf_entity_get_movement(entity);
            if (movement) {
                _golf_editor_edit_movement(movement);
            }

            golf_geo_t *geo = golf_entity_get_geo(entity);
            if (geo && transform) {
                _golf_editor_edit_geo(geo, *transform);
            }
        }
    }
    igEnd();

    if (editor.open_save_as_popup) {
        igOpenPopup_Str("save_as", ImGuiPopupFlags_None);
        editor.open_save_as_popup = false;
    }

    igSetNextWindowSize((ImVec2){500, 200}, ImGuiCond_Always);
    if (igBeginPopupModal("save_as", NULL, ImGuiWindowFlags_None)) {
        igText("Save As");
        igInputText("Path", editor.level_path, GOLF_FILE_MAX_PATH, ImGuiInputTextFlags_None, NULL, NULL);
        if (igButton("Save", (ImVec2){0, 0})) {
            golf_log_note("Saving...");
            golf_level_save(editor.level, editor.level_path);
            golf_data_force_remount();
            golf_data_load(editor.level_path, false);
            editor.level = golf_data_get_level(editor.level_path);
            igCloseCurrentPopup();
        }
        igEndPopup();
    }

    if (editor.file_picker.open_popup) {
        igOpenPopup_Str("file_picker", ImGuiPopupFlags_None);
        editor.file_picker.open_popup = false;
    }
    if (igBeginPopup("file_picker", ImGuiWindowFlags_None)) {
        igText("File Picker");
        igInputText("Search", editor.file_picker.search, GOLF_FILE_MAX_PATH, ImGuiInputTextFlags_None, NULL, NULL);

        vec_golf_file_t files;
        vec_init(&files, "editor");
        golf_data_get_all_matching(editor.file_picker.type, editor.file_picker.search, &files); 
        for (int i = 0; i < files.length; i++) {
            golf_file_t file = files.data[i];
            if (igSelectable_Bool(file.path, false, ImGuiSelectableFlags_None, (ImVec2){0, 0})) {
                golf_editor_action_t action;
                _golf_editor_action_init(&action, "Select file");
                _golf_editor_action_push_data(&action, editor.file_picker.path, GOLF_FILE_MAX_PATH);
                _golf_editor_action_push_data(&action, editor.file_picker.data, sizeof(void*));

                golf_data_load(file.path, false);
                strcpy(editor.file_picker.path, file.path);
                //golf_data_t *data = golf_data_get_file(file.path); 
                //*editor.file_picker.data = data->ptr;

                _golf_editor_start_action(action);
                _golf_editor_commit_action();
            }
        }
        vec_deinit(&files);

        if (editor.file_picker.type == GOLF_DATA_LEVEL) {
            editor.hovered_idx = -1;
            editor.selected_idxs.length = 0;
            editor.in_edit_mode = false;
        }
        igEndPopup();
    }

    if (editor.gi_state.open_popup) {
        igOpenPopup_Str("Lightmap Generator Running", ImGuiPopupFlags_None);
        editor.gi_state.open_popup = false;
    }
    if (igBeginPopupModal("Lightmap Generator Running", NULL, ImGuiWindowFlags_None)) {
        igText("Progress: 1 / 10");
        if (!editor.gi_running && !golf_gi_is_running(&editor.gi)) {
            igCloseCurrentPopup();
        }
        igEndPopup();
    }

    igEnd();

    if (editor.has_queued_commit) {
        editor.has_queued_commit = false;
        _golf_editor_commit_action();
    }

    if (editor.has_queued_decommit) {
        editor.has_queued_decommit = false;
        _golf_editor_decommit_action();
    }

    if (editor.has_queued_action) {
        editor.has_queued_action = false;
        _golf_editor_start_action(editor.queued_action);
    }

    for (int i = 0; i < editor.level->entities.length; i++) {
        golf_entity_t *entity = &editor.level->entities.data[i];
        if (!entity->active) continue;

        golf_movement_t *movement = golf_entity_get_movement(entity);
        if (movement) {
            movement->t += dt;
            if (movement->t >= movement->length) {
                movement->t = movement->t - movement->length;
            }
        }

        golf_geo_t *geo = golf_entity_get_geo(entity);
        if (geo) {
            if (!geo->model_updated_this_frame) {
                golf_geo_update_model(geo);
            }
            geo->model_updated_this_frame = false;
        }
    }

    for (int i = 0; i < editor.level->lightmap_images.length; i++) {
        golf_lightmap_image_t *lightmap = &editor.level->lightmap_images.data[i];
        if (!lightmap->active) continue;

        lightmap->cur_time += dt;
        if (lightmap->cur_time >= lightmap->time_length) {
            lightmap->cur_time = lightmap->cur_time - lightmap->time_length;
        }
    }

    if (editor.in_edit_mode) {
        golf_geo_t *geo = editor.edit_mode.geo;
        mat4 model_mat = golf_transform_get_model_mat(editor.edit_mode.transform);

        vec_vec3_t triangles;
        vec_int_t face_idxs;
        vec_vec3_t line_segments_p0;
        vec_vec3_t line_segments_p1;
        vec_float_t line_segments_radius;
        vec_int_t line_p0_idx;
        vec_int_t line_p1_idx;
        vec_init(&triangles, "editor");
        vec_init(&face_idxs, "editor");
        vec_init(&line_segments_p0, "editor");
        vec_init(&line_segments_p1, "editor");
        vec_init(&line_segments_radius, "editor");
        vec_init(&line_p0_idx, "editor");
        vec_init(&line_p1_idx, "editor");
        for (int i = 0; i < geo->faces.length; i++) {
            golf_geo_face_t face = geo->faces.data[i];
            if (!face.active) continue;

            int idx0 = face.idx.data[0];
            for (int j = 1; j < face.idx.length - 1; j++) {
                int idx1 = face.idx.data[j];
                int idx2 = face.idx.data[j + 1];

                golf_geo_point_t p0 = geo->points.data[idx0];
                golf_geo_point_t p1 = geo->points.data[idx1];
                golf_geo_point_t p2 = geo->points.data[idx2];

                vec3 pos0 = vec3_apply_mat4(p0.position, 1, model_mat);
                vec3 pos1 = vec3_apply_mat4(p1.position, 1, model_mat);
                vec3 pos2 = vec3_apply_mat4(p2.position, 1, model_mat);

                vec_push(&triangles, pos0);
                vec_push(&triangles, pos1);
                vec_push(&triangles, pos2);
                vec_push(&face_idxs, i);
            }

            for (int j = 0; j < face.idx.length; j++) {
                int idx0 = face.idx.data[j];
                int idx1 = face.idx.data[(j + 1) % face.idx.length];

                golf_geo_point_t p0 = geo->points.data[idx0];
                golf_geo_point_t p1 = geo->points.data[idx1];

                vec3 pos0 = vec3_apply_mat4(p0.position, 1, model_mat);
                vec3 pos1 = vec3_apply_mat4(p1.position, 1, model_mat);

                float radius = CFG_NUM(editor_cfg, "edit_mode_line_size");

                vec_push(&line_segments_p0, pos0);
                vec_push(&line_segments_p1, pos1);
                vec_push(&line_segments_radius, radius);
                vec_push(&line_p0_idx, idx0);
                vec_push(&line_p1_idx, idx1);
            }
        }

        vec_vec3_t sphere_centers;
        vec_float_t sphere_radiuses;
        vec_int_t point_idxs;
        vec_init(&sphere_centers, "editor");
        vec_init(&sphere_radiuses, "editor");
        vec_init(&point_idxs, "editor");
        for (int i = 0; i < geo->points.length; i++) {
            golf_geo_point_t p = geo->points.data[i];
            if (!p.active) continue;

            vec3 pos = vec3_apply_mat4(p.position, 1, model_mat);
            float radius = CFG_NUM(editor_cfg, "edit_mode_sphere_size");
            vec_push(&sphere_centers, pos);
            vec_push(&sphere_radiuses, radius);
            vec_push(&point_idxs, i);
        }

        vec3 ro = inputs->mouse_ray_orig;
        vec3 rd = inputs->mouse_ray_dir;

        float line_t = FLT_MAX;
        int line_idx = -1;
        ray_intersect_segments(ro, rd, line_segments_p0.data, line_segments_p1.data, line_segments_radius.data, line_segments_p0.length, &line_t, &line_idx);

        float triangle_t = FLT_MAX;
        int triangle_idx = -1;
        ray_intersect_triangles(ro, rd, triangles.data, triangles.length, &triangle_t, &triangle_idx);

        float sphere_t = FLT_MAX;
        int sphere_idx = -1;
        ray_intersect_spheres(ro, rd, sphere_centers.data, sphere_radiuses.data, sphere_centers.length, &sphere_t, &sphere_idx);

        if (line_t < FLT_MAX) line_t -= CFG_NUM(editor_cfg, "edit_mode_line_size");

        if (sphere_t < FLT_MAX && sphere_t < line_t && sphere_t < triangle_t) {
            editor.edit_mode.is_entity_hovered = true;
            int idx = point_idxs.data[sphere_idx];
            editor.edit_mode.hovered_entity = golf_edit_mode_entity_point(idx);
        }
        else if (line_t < FLT_MAX && line_t < sphere_t && line_t < triangle_t) {
            editor.edit_mode.is_entity_hovered = true;
            int idx0 = line_p0_idx.data[line_idx];
            int idx1 = line_p1_idx.data[line_idx];
            editor.edit_mode.hovered_entity = golf_edit_mode_entity_line(idx0, idx1);
        }
        else if (triangle_t < FLT_MAX && triangle_t < sphere_t && triangle_t < line_t) {
            editor.edit_mode.is_entity_hovered = true;
            int idx = face_idxs.data[triangle_idx];
            editor.edit_mode.hovered_entity = golf_edit_mode_entity_face(idx);
        }
        else {
            editor.edit_mode.is_entity_hovered = false;
        }

        if (!IO->WantCaptureMouse && !editor.mouse_down_in_imgui && inputs->mouse_clicked[SAPP_MOUSEBUTTON_LEFT]) {
            if (editor.edit_mode.is_entity_hovered) {
                _golf_editor_edit_mode_select_entity(editor.edit_mode.hovered_entity);
            }
            else {
                editor.edit_mode.selected_entities.length = 0;
            }
        }

        vec_deinit(&triangles);
        vec_deinit(&face_idxs);
        vec_deinit(&line_segments_p0);
        vec_deinit(&line_segments_p1);
        vec_deinit(&line_segments_radius);
        vec_deinit(&line_p0_idx);
        vec_deinit(&line_p1_idx);
        vec_deinit(&sphere_centers);
        vec_deinit(&sphere_radiuses);
        vec_deinit(&point_idxs);
    }
    else {
        vec_golf_bvh_node_info_t node_infos;
        vec_init(&node_infos, "editor");

        for (int i = 0; i < editor.level->entities.length; i++) {
            golf_entity_t *entity = &editor.level->entities.data[i];
            if (!entity->active) continue;

            golf_model_t *model = golf_entity_get_model(entity);
            golf_transform_t *transform = golf_entity_get_transform(entity);
            if (model && transform) {
                golf_transform_t world_transform = golf_entity_get_world_transform(editor.level, entity);
                mat4 model_mat = golf_transform_get_model_mat(world_transform);
                vec_push(&node_infos, golf_bvh_node_info(&editor.bvh, i, model, model_mat, editor.level));
            }
        }

        {
            golf_bvh_construct(&editor.bvh, node_infos);
            editor.hovered_idx = -1;
            float t = FLT_MAX;
            int idx = -1;
            if (golf_bvh_ray_test(&editor.bvh, inputs->mouse_ray_orig, inputs->mouse_ray_dir, &t, &idx, NULL)) {
                editor.hovered_idx = idx;
            }
        }

        if (!IO->WantCaptureMouse && !editor.mouse_down_in_imgui && inputs->mouse_clicked[SAPP_MOUSEBUTTON_LEFT]) {
            _golf_editor_select_entity(editor.hovered_idx);
        }

        vec_deinit(&node_infos);
    }

    if (editor.gi_running) {
        golf_gi_t *gi = &editor.gi;
        if (!golf_gi_is_running(gi)) {
            golf_log_note("Lightmap Generator Finished");

            golf_editor_action_t action;
            _golf_editor_action_init(&action, "Create lightmap");
            for (int i = 0; i < gi->entities.length; i++) {
                golf_gi_entity_t *gi_entity = &gi->entities.data[i];

                int res = gi_entity->resolution;
                int w = gi_entity->image_width;
                int h = gi_entity->image_height;
                float time_length = gi_entity->time_length;
                int num_samples = gi_entity->num_samples;
                unsigned char **data = golf_alloc(sizeof(unsigned char*) * num_samples);
                for (int s = 0; s < num_samples; s++) {
                    data[s] = golf_alloc(w * h);
                    for (int i = 0; i < w * h; i++) {
                        float a = gi_entity->image_data[s][i];
                        if (a > 1.0f) a = 1.0f;
                        if (a < 0.0f) a = 0.0f;
                        data[s][i] = (unsigned char)(0xFF * a);
                    }
                }
                sg_image *sg_images = golf_alloc(sizeof(sg_image) * num_samples);

                golf_lightmap_image_t *lightmap_image = gi_entity->lightmap_image;
                char name[GOLF_MAX_NAME_LEN];
                snprintf(name, GOLF_MAX_NAME_LEN, "%s", lightmap_image->name);
                _golf_editor_action_push_data(&action, lightmap_image, sizeof(golf_lightmap_image_t));
                *lightmap_image = golf_lightmap_image(name, res, w, h, time_length, num_samples, data, sg_images);
                golf_lightmap_image_finalize(lightmap_image);

                for (int i = 0; i < gi_entity->gi_lightmap_sections.length; i++) {
                    golf_gi_lightmap_section_t *gi_lightmap_section = &gi_entity->gi_lightmap_sections.data[i];
                    golf_lightmap_section_t *lightmap_section = gi_lightmap_section->lightmap_section;
                    _golf_editor_action_push_data(&action, lightmap_section, sizeof(golf_lightmap_section_t));

                    char name[GOLF_MAX_NAME_LEN];
                    snprintf(name, GOLF_MAX_NAME_LEN, "%s", lightmap_section->lightmap_name);

                    vec_vec2_t uvs;
                    vec_init(&uvs, "level");
                    vec_pusharr(&uvs, gi_lightmap_section->lightmap_uvs.data, gi_lightmap_section->lightmap_uvs.length);

                    *lightmap_section = golf_lightmap_section(name, uvs);
                    golf_lightmap_section_finalize(lightmap_section);
                }
            }
            _golf_editor_start_action(action);
            _golf_editor_commit_action();

            golf_gi_deinit(gi);
            editor.gi_running = false;
        }
    }

    if (!editor.mouse_down && inputs->mouse_down[SAPP_MOUSEBUTTON_LEFT]) {
        editor.mouse_down_in_imgui = IO->WantCaptureMouse;
        editor.mouse_down = true;
    }
    if (editor.mouse_down && !inputs->mouse_down[SAPP_MOUSEBUTTON_LEFT]) {
        editor.mouse_down = false;
    }

    if (!IO->WantCaptureMouse) {
        if (inputs->button_down[SAPP_KEYCODE_LEFT]) {
            editor.camera.azimuth_angle -= 1.0f * dt;
        }
        if (inputs->button_down[SAPP_KEYCODE_RIGHT]) {
            editor.camera.azimuth_angle += 1.0f * dt;
        }
        if (inputs->button_down[SAPP_KEYCODE_UP]) {
            editor.camera.inclination_angle -= 1.0f * dt;
        }
        if (inputs->button_down[SAPP_KEYCODE_DOWN]) {
            editor.camera.inclination_angle += 1.0f * dt;
        }
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT]) {
            editor.camera.azimuth_angle += 0.2f * dt * inputs->mouse_delta.x;
            editor.camera.inclination_angle -= 0.2f * dt * inputs->mouse_delta.y;
        }
    }

    float cam_speed = 8.0f;
    if (!IO->WantCaptureKeyboard) {
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT] && inputs->button_down[SAPP_KEYCODE_W]) {
            graphics->cam_pos.x -= cam_speed * dt * cosf(editor.camera.azimuth_angle);
            graphics->cam_pos.z -= cam_speed * dt * sinf(editor.camera.azimuth_angle);
        }
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT] && inputs->button_down[SAPP_KEYCODE_S]) {
            graphics->cam_pos.x += cam_speed * dt * cosf(editor.camera.azimuth_angle);
            graphics->cam_pos.z += cam_speed * dt * sinf(editor.camera.azimuth_angle);
        }
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT] && inputs->button_down[SAPP_KEYCODE_D]) {
            graphics->cam_pos.x -= cam_speed * dt * cosf(editor.camera.azimuth_angle + 0.5f * MF_PI);
            graphics->cam_pos.z -= cam_speed * dt * sinf(editor.camera.azimuth_angle + 0.5f * MF_PI);
        }
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT] && inputs->button_down[SAPP_KEYCODE_A]) {
            graphics->cam_pos.x += cam_speed * dt * cosf(editor.camera.azimuth_angle + 0.5f * MF_PI);
            graphics->cam_pos.z += cam_speed * dt * sinf(editor.camera.azimuth_angle + 0.5f * MF_PI);
        }
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT] && inputs->button_down[SAPP_KEYCODE_Q]) {
            graphics->cam_pos.y -= cam_speed * dt;
        }
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT] && inputs->button_down[SAPP_KEYCODE_E]) {
            graphics->cam_pos.y += cam_speed * dt;
        }
        if (inputs->button_clicked[SAPP_KEYCODE_G]) {
            if (!editor.in_edit_mode && editor.selected_idxs.length > 0) {
                int idx = editor.selected_idxs.data[0];
                golf_entity_t *entity = &editor.level->entities.data[idx];
                golf_transform_t *transform = golf_entity_get_transform(entity);
                golf_geo_t *geo = golf_entity_get_geo(entity);
                if (transform && geo) {
                    golf_transform_t world_transform = golf_entity_get_world_transform(editor.level, entity);
                    _golf_editor_start_editing_geo(geo, world_transform);
                }
            }
            else if (editor.in_edit_mode) {
                _golf_editor_stop_editing_geo();
            }
        }
        if (inputs->button_down[SAPP_KEYCODE_LEFT_CONTROL] && inputs->button_clicked[SAPP_KEYCODE_Z]) {
            _golf_editor_undo_action();
        }
        if (inputs->button_down[SAPP_KEYCODE_LEFT_CONTROL] && inputs->button_clicked[SAPP_KEYCODE_Y]) {
            _golf_editor_redo_action();
        }
        if (inputs->button_down[SAPP_KEYCODE_LEFT_CONTROL] && inputs->button_clicked[SAPP_KEYCODE_D]) {
            _golf_editor_duplicate_selected_entities();
        }
        if (inputs->button_down[SAPP_KEYCODE_LEFT_CONTROL] && inputs->button_clicked[SAPP_KEYCODE_S]) {
            golf_log_note("Saving...");
            golf_level_save(editor.level, editor.level_path);
        }
        if (inputs->button_down[SAPP_KEYCODE_LEFT_CONTROL] && inputs->button_clicked[SAPP_KEYCODE_C]) {
            _golf_editor_copy_selected_entities();
        }
        if (inputs->button_down[SAPP_KEYCODE_LEFT_CONTROL] && inputs->button_clicked[SAPP_KEYCODE_V]) {
            _golf_editor_paste_selected_entities();
        }
        if (inputs->button_clicked[SAPP_KEYCODE_T]) {
            golf_gizmo_set_operation(&editor.gizmo, GOLF_GIZMO_TRANSLATE);
        }
        if (inputs->button_clicked[SAPP_KEYCODE_R]) {
            golf_gizmo_set_operation(&editor.gizmo, GOLF_GIZMO_ROTATE);
        }
        if (inputs->button_clicked[SAPP_KEYCODE_Y]) {
            golf_gizmo_set_operation(&editor.gizmo, GOLF_GIZMO_SCALE);
        }
        if (inputs->button_clicked[SAPP_KEYCODE_DELETE]) {
            _golf_editor_delete_selected_entities();
        }
    }

    {
        float theta = editor.camera.inclination_angle;
        float phi = editor.camera.azimuth_angle;
        graphics->cam_dir.x = sinf(theta) * cosf(phi);
        graphics->cam_dir.y = cosf(theta);
        graphics->cam_dir.z = sinf(theta) * sinf(phi);
    }
}

golf_edit_mode_entity_t golf_edit_mode_entity_face(int face_idx) {
    golf_edit_mode_entity_t entity;
    entity.type = GOLF_EDIT_MODE_ENTITY_FACE;
    entity.idx = face_idx;
    entity.idx2 = -1;
    return entity;
}

golf_edit_mode_entity_t golf_edit_mode_entity_line(int point0_idx, int point1_idx) {
    golf_edit_mode_entity_t entity;
    entity.type = GOLF_EDIT_MODE_ENTITY_LINE;
    entity.idx = point0_idx;
    entity.idx2 = point1_idx;
    return entity;
}

golf_edit_mode_entity_t golf_edit_mode_entity_point(int point_idx) {
    golf_edit_mode_entity_t entity;
    entity.type = GOLF_EDIT_MODE_ENTITY_POINT;
    entity.idx = point_idx;
    entity.idx2 = -1;
    return entity;
}

bool golf_editor_edit_entities_compare(golf_edit_mode_entity_t entity0, golf_edit_mode_entity_t entity1) {
    if (entity0.type != entity1.type) {
        return false;
    }
    switch (entity0.type) {
        case GOLF_EDIT_MODE_ENTITY_FACE:
        case GOLF_EDIT_MODE_ENTITY_POINT:
            return entity0.idx == entity1.idx;
            break;
        case GOLF_EDIT_MODE_ENTITY_LINE:
            return (entity0.idx == entity1.idx && entity0.idx2 == entity1.idx2) ||
                (entity0.idx == entity1.idx2 && entity0.idx2 == entity1.idx);
            break;
    }
    return false;
}

bool golf_editor_is_edit_entity_hovered(golf_edit_mode_entity_t entity) {
    if (!editor.in_edit_mode) {
        return false;
    }

    if (editor.select_box.is_open) {
        for (int i = 0; i < editor.select_box.hovered_entities.length; i++) {
            golf_edit_mode_entity_t hovered_entity = editor.select_box.hovered_entities.data[i];
            if (golf_editor_edit_entities_compare(hovered_entity, entity)) {
                return true;
            }
        }
    }
    
    bool is_entity_hovered = editor.edit_mode.is_entity_hovered;
    golf_edit_mode_entity_t hovered_entity = editor.edit_mode.hovered_entity;
    return is_entity_hovered && golf_editor_edit_entities_compare(hovered_entity, entity);
}

bool golf_editor_is_edit_entity_selected(golf_edit_mode_entity_t entity) {
    for (int i = 0; i < editor.edit_mode.selected_entities.length; i++) {
        golf_edit_mode_entity_t entity2 = editor.edit_mode.selected_entities.data[i];
        if (golf_editor_edit_entities_compare(entity, entity2)) {
            return true;
        }
    }
    return false;
}

void golf_editor_edit_mode_geo_add_point(golf_geo_point_t point) {
    if (!editor.in_edit_mode) {
        golf_log_warning("Editor not in edit mode");
        return;
    }

    golf_geo_t *geo = editor.edit_mode.geo;
    _vec_push_and_fix_actions(&geo->points, point, NULL);
}

void golf_editor_edit_mode_geo_add_face(golf_geo_face_t face) {
    if (!editor.in_edit_mode) {
        golf_log_warning("Editor not in edit mode");
        return;
    }

    golf_geo_t *geo = editor.edit_mode.geo;
    if (face.idx.length < 3) {
        golf_log_warning("Invalid number of points face");
        return;
    }
    for (int i = 0; i < face.idx.length; i++) {
        int idx = face.idx.data[i];
        if (idx < 0 || idx >= geo->points.length) {
            golf_log_warning("Invalid point idx in face");
            return;
        }
    }
    _vec_push_and_fix_actions(&geo->faces, face, NULL);
}
