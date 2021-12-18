#include "golf/editor.h"

#include <float.h>
#include <string.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include "cimguizmo/cimguizmo.h"
#include "IconsFontAwesome5/IconsFontAwesome5.h"
#include "stb/stb_image_write.h"
#include "golf/alloc.h"
#include "golf/data.h"
#include "golf/inputs.h"
#include "golf/log.h"
#include "golf/renderer.h"

static golf_editor_t editor;
static golf_inputs_t *inputs;
static golf_renderer_t *renderer;

golf_editor_t *golf_editor_get(void) {
    return &editor;
}

void golf_editor_init(void) {
    inputs = golf_inputs_get();
    renderer = golf_renderer_get();

    golf_data_load("data/config/editor.cfg");

    ImGuiIO *IO = igGetIO();
    IO->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    memset(&editor, 0, sizeof(editor));

    {
        editor.level = golf_data_get_level("data/levels/level-1/level-1.level");

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

    {
        editor.gizmo.is_using = false;
        editor.gizmo.bounds_mode_on = false;
        editor.gizmo.operation = TRANSLATE;
        editor.gizmo.mode = LOCAL;
        editor.gizmo.use_snap = false;
        editor.gizmo.translate_snap = 1;
        editor.gizmo.scale_snap = 1;
        editor.gizmo.rotate_snap = 1;
        editor.gizmo.model_mat = mat4_identity();
    }

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
}

static void _golf_editor_start_editing_geo(golf_geo_t *geo, mat4 model_mat) {
    if (editor.in_edit_mode) {
        golf_log_warning("Already in geo editing mode");
        return;
    }

    editor.in_edit_mode = true;
    editor.edit_mode.geo = geo;
    editor.edit_mode.model_mat = model_mat;
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

static void _golf_editor_fix_actions(char *new_ptr, char *old_ptr_start, char *old_ptr_end) {
    for (int i = 0; i < editor.undo_actions.length; i++) {
        golf_editor_action_t *action = &editor.undo_actions.data[i];
        for (int i = 0; i < action->datas.length; i++) {
            golf_editor_action_data_t *action_data = &action->datas.data[i];
            if (action_data->ptr >= old_ptr_start && action_data->ptr < old_ptr_end) {
                action_data->ptr = new_ptr + (action_data->ptr - old_ptr_start);
            }
        }
    }
}

#define _vec_push_and_fix_actions(v, val)\
    do {\
        char *old_ptr = (char*)(v)->data;\
        vec_push(v, val); \
        if ((char*)(v)->data != old_ptr)\
            _golf_editor_fix_actions((char*)(v)->data, old_ptr, old_ptr + sizeof(val) * ((v)->length - 1));\
    } while(0)

static void _golf_editor_undo_action(void) {
    if (editor.undo_actions.length == 0) {
        return;
    }

    golf_editor_action_t *undo_action = &vec_pop(&editor.undo_actions);
    golf_editor_action_t redo_action;
    _golf_editor_action_init(&redo_action, undo_action->name);
    for (int i = 0; i < undo_action->datas.length; i++) {
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
    for (int i = 0; i < redo_action->datas.length; i++) {
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

static void _golf_editor_duplicate_selected_entities(void) {
    for (int i = 0; i < editor.selected_idxs.length; i++) {
        int idx = editor.selected_idxs.data[i];
        golf_entity_t *selected_entity = &editor.level->entities.data[idx];
        golf_entity_t entity_copy = golf_entity_make_copy(selected_entity);
        _vec_push_and_fix_actions(&editor.level->entities, entity_copy);
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

static void _golf_editor_recalculate_guizmo_model_mat(void);

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
    _golf_editor_recalculate_guizmo_model_mat();
}

static void _golf_editor_recalculate_guizmo_model_mat(void) {
    if (editor.in_edit_mode) {
        mat4 model_mat = editor.edit_mode.model_mat;
        golf_geo_t *geo = editor.edit_mode.geo;

        vec3 local_pos = V3(0, 0, 0);
        for (int i = 0; i < editor.edit_mode.selected_entities.length; i++) {
            golf_edit_mode_entity_t entity = editor.edit_mode.selected_entities.data[i];
            switch (entity.type) {
                case GOLF_EDIT_MODE_ENTITY_FACE: {
                    vec3 face_pos = V3(0, 0, 0);
                    golf_geo_face_t face = geo->faces.data[entity.idx];
                    for (int i = 0; i < face.idx.length; i++) {
                        int idx = face.idx.data[i];

                        golf_geo_point_t p = geo->points.data[idx];
                        face_pos = vec3_add(p.position, face_pos);
                    }
                    face_pos = vec3_scale(face_pos, 1.0f / face.idx.length);
                    local_pos = vec3_add(local_pos, face_pos);
                    break;
                }
                case GOLF_EDIT_MODE_ENTITY_LINE: {
                    golf_geo_point_t p0 = geo->points.data[entity.idx];
                    golf_geo_point_t p1 = geo->points.data[entity.idx2];

                    vec3 line_pos = vec3_scale(vec3_add(p0.position, p1.position), 0.5f);
                    local_pos = vec3_add(local_pos, line_pos);
                    break;
                }
                case GOLF_EDIT_MODE_ENTITY_POINT: {
                    golf_geo_point_t p = geo->points.data[entity.idx];
                    local_pos = vec3_add(local_pos, p.position);
                    break;
                }
            }
        }
        local_pos = vec3_scale(local_pos, 1.0f / editor.edit_mode.selected_entities.length);

        editor.edit_mode.local_model_mat = mat4_translation(local_pos);
        editor.edit_mode.world_model_mat = mat4_multiply(model_mat, editor.edit_mode.local_model_mat);
        editor.gizmo.model_mat = editor.edit_mode.world_model_mat;
    }
    else {
    }
}

static void _golf_editor_update_guizmo(void) {
    ImGuizmo_SetRect(renderer->viewport_pos.x, renderer->viewport_pos.y, renderer->viewport_size.x, renderer->viewport_size.y);
    mat4 view_mat_t = mat4_transpose(renderer->view_mat);
    mat4 proj_mat_t = mat4_transpose(renderer->proj_mat);

    float snap_values[3];
    float *snap = NULL;
    if (editor.gizmo.use_snap) {
        if (editor.gizmo.operation == TRANSLATE) {
            snap_values[0] = snap_values[1] = snap_values[2] = editor.gizmo.translate_snap;
        }
        else if (editor.gizmo.operation == SCALE) {
            snap_values[0] = snap_values[1] = snap_values[2] = editor.gizmo.scale_snap;
        }
        else if (editor.gizmo.operation == ROTATE) {
            snap_values[0] = snap_values[1] = snap_values[2] = editor.gizmo.rotate_snap;
        }
        else {
            golf_log_error("Invalid value for gizmo operation %d", editor.gizmo.operation);
        }
        snap = snap_values;
    }

    if (editor.in_edit_mode && editor.edit_mode.selected_entities.length > 0) {
        //mat4 model_mat = editor.edit_mode.model_mat;
        golf_geo_t *geo = editor.edit_mode.geo;

        mat4 mat = mat4_transpose(editor.gizmo.model_mat); 
        ImGuizmo_Manipulate(view_mat_t.m, proj_mat_t.m, editor.gizmo.operation, 
                editor.gizmo.mode, mat.m, NULL, snap, NULL, NULL);
        editor.gizmo.model_mat = mat4_transpose(mat);

        bool is_using = ImGuizmo_IsUsing();
        if (!editor.gizmo.is_using && is_using) {
            editor.edit_mode.point_idxs.length = 0;
            for (int i = 0; i < editor.edit_mode.selected_entities.length; i++) {
                golf_edit_mode_entity_t entity = editor.edit_mode.selected_entities.data[i];
                switch (entity.type) {
                    case GOLF_EDIT_MODE_ENTITY_FACE: {
                        golf_geo_face_t face = geo->faces.data[entity.idx];
                        for (int i = 0; i < face.idx.length; i++) {
                            int idx = face.idx.data[i];
                            _golf_editor_push_unique_idx(&editor.edit_mode.point_idxs, idx);
                        }
                        break;
                    }
                    case GOLF_EDIT_MODE_ENTITY_LINE: {
                        _golf_editor_push_unique_idx(&editor.edit_mode.point_idxs, entity.idx);
                        _golf_editor_push_unique_idx(&editor.edit_mode.point_idxs, entity.idx2);
                        break;
                    }
                    case GOLF_EDIT_MODE_ENTITY_POINT: {
                        _golf_editor_push_unique_idx(&editor.edit_mode.point_idxs, entity.idx);
                        break;
                    }
                }
            }

            golf_editor_action_t action;
            _golf_editor_action_init(&action, "Modify edit-mode-entity position");
            editor.edit_mode.starting_positions.length = 0;
            for (int i = 0; i < editor.edit_mode.point_idxs.length; i++) {
                int idx = editor.edit_mode.point_idxs.data[i];
                golf_geo_point_t *p = &geo->points.data[idx];
                _golf_editor_action_push_data(&action, &p->position, sizeof(p->position));
                vec3 pos = vec3_apply_mat4(p->position, 1, mat4_inverse(editor.edit_mode.local_model_mat));
                vec_push(&editor.edit_mode.starting_positions, pos);
            }
            _golf_editor_start_action(action);
            editor.edit_mode.starting_model_mat = editor.gizmo.model_mat;
        }
        if (editor.gizmo.is_using && !is_using) {
            _golf_editor_commit_action();
        }
        editor.gizmo.is_using = is_using;

        if (editor.gizmo.is_using) {
            for (int i = 0; i < editor.edit_mode.point_idxs.length; i++) {
                int idx = editor.edit_mode.point_idxs.data[i];
                golf_geo_point_t *p = &geo->points.data[idx];
                vec3 starting_position = editor.edit_mode.starting_positions.data[i];

                mat4 model_mat = mat4_multiply_n(3, 
                        editor.edit_mode.local_model_mat,
                        mat4_inverse(editor.edit_mode.starting_model_mat),
                        editor.gizmo.model_mat);
                p->position = vec3_apply_mat4(starting_position, 1, model_mat);
            }
        }
    }
    else if (!editor.in_edit_mode && editor.selected_idxs.length > 0) {
        mat4 model_mat = mat4_transpose(editor.gizmo.model_mat);
        mat4 delta_matrix;
        ImGuizmo_Manipulate(view_mat_t.m, proj_mat_t.m, editor.gizmo.operation, 
                editor.gizmo.mode, model_mat.m, delta_matrix.m, snap, NULL, NULL);
        editor.gizmo.model_mat = mat4_transpose(model_mat);

        float delta_translation_arr[3];
        float delta_rotation_arr[3];
        float delta_scale_arr[3];
        ImGuizmo_DecomposeMatrixToComponents(delta_matrix.m, delta_translation_arr, delta_rotation_arr, delta_scale_arr);
        vec3 delta_translation = vec3_create_from_array(delta_translation_arr);

        for (int i = 0; i < editor.selected_idxs.length; i++) {
            int idx = editor.selected_idxs.data[i];
            golf_entity_t *entity = &editor.level->entities.data[idx];
            golf_transform_t *transform = golf_entity_get_transform(entity);
            if (transform) {
                transform->position = vec3_add(transform->position, delta_translation);
            }
        }

        bool is_using = ImGuizmo_IsUsing();
        if (!editor.gizmo.is_using && is_using) {
            golf_editor_action_t action;
            _golf_editor_action_init(&action, "Modify transform position");
            for (int i = 0; i < editor.selected_idxs.length; i++) {
                int idx = editor.selected_idxs.data[i];
                golf_entity_t *entity = &editor.level->entities.data[idx];
                golf_transform_t *transform = golf_entity_get_transform(entity);
                if (transform) {
                    _golf_editor_action_push_data(&action, transform, sizeof(golf_transform_t)); 
                }
            }
            _golf_editor_start_action(action);
        }
        if (editor.gizmo.is_using && !is_using) {
            _golf_editor_commit_action();
        }
        editor.gizmo.is_using = is_using;
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

static void _golf_editor_edit_geo(golf_geo_t *geo, mat4 model_mat) {
    if (igTreeNode_Str("Geo")) {
        if (igButton("Edit Geo", (ImVec2){0, 0})) {
            _golf_editor_start_editing_geo(geo, model_mat);
        }
        igTreePop();
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
            }
            if (igMenuItem_Bool("Save", NULL, false, true)) {
                golf_log_note("Saving...");
                golf_level_save(editor.level, "data/levels/level-1/level-1.level");
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
            igEndMenu();
        }
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
        renderer->viewport_pos = V2(central_node->Pos.x, central_node->Pos.y);
        renderer->viewport_size = V2(central_node->Size.x, central_node->Size.y);
    }

    for (int i = 0; i < editor.selected_idxs.length; i++) {
        int idx = editor.selected_idxs.data[i];
        if (!editor.level->entities.data[idx].active) {
            vec_splice(&editor.selected_idxs, i, 1);
            i--;
        }
    }

    _golf_editor_update_guizmo();

    {
        igBegin("Top", NULL, ImGuiWindowFlags_NoTitleBar);

        // Translate Mode Button
        {
            bool is_on = editor.gizmo.operation == TRANSLATE;
            if (is_on) {
                igPushStyleColor_U32(ImGuiCol_Button, igGetColorU32_Col(ImGuiCol_ButtonActive, 1));
            }
            if (igButton(ICON_FA_ARROWS_ALT, (ImVec2){20, 20})) {
                editor.gizmo.operation = TRANSLATE;
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
            bool is_on = editor.gizmo.operation == ROTATE;
            if (is_on) {
                igPushStyleColor_U32(ImGuiCol_Button, igGetColorU32_Col(ImGuiCol_ButtonActive, 1));
            }
            if (igButton(ICON_FA_SYNC_ALT, (ImVec2){20, 20})) {
                editor.gizmo.operation = ROTATE;
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
            bool is_on = editor.gizmo.operation == SCALE;
            if (is_on) {
                igPushStyleColor_U32(ImGuiCol_Button, igGetColorU32_Col(ImGuiCol_ButtonActive, 1));
            }
            if (igButton(ICON_FA_EXPAND_ALT, (ImVec2){20, 20})) {
                editor.gizmo.operation = SCALE;
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

        // Bounds Mode Button
        {
            igSameLine(0, 2);
            bool is_on = editor.gizmo.bounds_mode_on;
            if (is_on) {
                igPushStyleColor_U32(ImGuiCol_Button, igGetColorU32_Col(ImGuiCol_ButtonActive, 1));
            }
            if (igButton(ICON_FA_EXPAND, (ImVec2){20, 20})) {
                editor.gizmo.bounds_mode_on = !editor.gizmo.bounds_mode_on;
            }
            if (is_on) {
                igPopStyleColor(1);
            }
            if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                igBeginTooltip();
                igText("Bounds (T)");
                igEndTooltip();
            }
        }

        // Local / Global Button
        {
            igSameLine(0, 10);
            if (editor.gizmo.mode == LOCAL) {
                if (igButton(ICON_FA_CUBE, (ImVec2){20, 20})) {
                    editor.gizmo.mode = WORLD;
                }
                if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                    igBeginTooltip();
                    igText("Local");
                    igEndTooltip();
                }
            }
            else if (editor.gizmo.mode == WORLD) {
                if (igButton(ICON_FA_GLOBE_ASIA, (ImVec2){20, 20})) {
                    editor.gizmo.mode = LOCAL;
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
            igSameLine(0, 10);
            bool is_on = editor.gizmo.use_snap;
            if (is_on) {
                igPushStyleColor_U32(ImGuiCol_Button, igGetColorU32_Col(ImGuiCol_ButtonActive, 1));
            }
            if (igButton(ICON_FA_TH, (ImVec2){20, 20})) {
                editor.gizmo.use_snap = !editor.gizmo.use_snap;
            }
            if (is_on) {
                igPopStyleColor(1);
            }
            if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                igBeginTooltip();
                igText("Snap");
                igEndTooltip();
            }

            igSameLine(0, 1);
            static float *v = NULL;
            if (editor.gizmo.operation == TRANSLATE) {
                v = &editor.gizmo.translate_snap;
            }
            else if (editor.gizmo.operation == SCALE) {
                v = &editor.gizmo.scale_snap;
            }
            else if (editor.gizmo.operation == ROTATE) {
                v = &editor.gizmo.rotate_snap;
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

                                        golf_gi_add_lightmap_section(gi, lightmap_section, model, *transform, *movement);
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

    if (editor.in_edit_mode) {
        golf_geo_t *geo = editor.edit_mode.geo;
        golf_geo_update_model(geo);

        igBegin("RightTop", NULL, ImGuiWindowFlags_NoTitleBar);
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
        igEnd();

        igBegin("RightBottom", NULL, ImGuiWindowFlags_NoTitleBar);
        for (int i = 0; i < editor.edit_mode.selected_entities.length; i++) {
            golf_edit_mode_entity_t entity = editor.edit_mode.selected_entities.data[i];
            igPushID_Int(i);
            switch (entity.type) {
                case GOLF_EDIT_MODE_ENTITY_FACE: {
                    if (igTreeNode_Str("Face")) {
                        golf_geo_face_t *face = &geo->faces.data[entity.idx];
                        igText("Num points: %d", face->idx.length);
                        for (int i = 0; i < face->idx.length; i++) {
                            golf_geo_point_t *p = &geo->points.data[face->idx.data[i]];
                            igPushID_Int(i);
                            _golf_editor_undoable_igInputFloat3("Position", (float*)&p->position, "Modify point position");
                            igPopID();
                        }
                        if (igButton("Delete", (ImVec2){0, 0})) {
                            golf_editor_action_t action;
                            _golf_editor_action_init(&action, "Delete geo face");
                            _golf_geo_delete_face(geo, entity.idx, &action);
                            _golf_editor_start_action(action);
                            _golf_editor_commit_action();
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
                        if (igButton("Delete", (ImVec2){0, 0})) {
                            golf_editor_action_t action;
                            _golf_editor_action_init(&action, "Delete geo line");
                            _golf_geo_delete_point(geo, entity.idx, &action);
                            _golf_geo_delete_point(geo, entity.idx2, &action);
                            _golf_editor_start_action(action);
                            _golf_editor_commit_action();
                        }
                        igTreePop();
                    }
                    break;
                }
                case GOLF_EDIT_MODE_ENTITY_POINT: {
                    if (igTreeNode_Str("Point")) {
                        golf_geo_point_t *p = &geo->points.data[entity.idx];
                        _golf_editor_undoable_igInputFloat3("Position", (float*)&p->position, "Modify point position");
                        if (igButton("Delete", (ImVec2){0, 0})) {
                            golf_editor_action_t action;
                            _golf_editor_action_init(&action, "Delete geo point");
                            _golf_geo_delete_point(geo, entity.idx, &action);
                            _golf_editor_start_action(action);
                            _golf_editor_commit_action();
                        }
                        igTreePop();
                    }
                    break;
                }
            }
            igPopID();
        }
        igEnd();
    }
    else {
        igBegin("RightTop", NULL, ImGuiWindowFlags_NoTitleBar);
        if (igBeginTabBar("", ImGuiTabBarFlags_None)) {
            if (igBeginTabItem("Entities", NULL, ImGuiTabItemFlags_None)) {
                for (int i = 0; i < editor.level->entities.length; i++) {
                    golf_entity_t *entity = &editor.level->entities.data[i];
                    if (!entity->active) continue;
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
                            break;
                        }
                    }
                    igPopID();
                }

                if (igButton("Create Model Entity", (ImVec2){0, 0})) {
                    golf_transform_t transform = golf_transform(V3(0, 0, 0), V3(1, 1, 1), QUAT(0, 0, 0, 1));

                    golf_lightmap_section_t lightmap_section;
                    {
                        const char *model_path = "data/models/cube.obj";
                        golf_model_t *model = golf_data_get_model(model_path);
                        vec_vec2_t uvs;
                        vec_init(&uvs, "editor");
                        for (int i = 0; i < model->positions.length; i++) {
                            vec_push(&uvs, V2(0, 0));
                        }

                        golf_lightmap_section_init(&lightmap_section, "main", uvs, 0, uvs.length);
                        vec_deinit(&uvs);
                    }

                    golf_movement_t movement;
                    movement = golf_movement_none();

                    golf_entity_t entity = golf_entity_model("Model", transform, "data/models/cube.obj", lightmap_section, movement);
                    _vec_push_and_fix_actions(&editor.level->entities, entity);
                    _golf_editor_commit_entity_create_action();
                }

                if (igButton("Create Geo Entity", (ImVec2){0, 0})) {
                    golf_geo_t geo;
                    golf_geo_init_cube(&geo);
                    golf_geo_update_model(&geo);
                    golf_transform_t transform = golf_transform(V3(0, 0, 0), V3(1, 1, 1), QUAT(0, 0, 0, 1));
                    golf_movement_t movement = golf_movement_none();

                    golf_entity_t entity = golf_entity_geo("geo", transform, movement, geo);
                    _vec_push_and_fix_actions(&editor.level->entities, entity);
                    _golf_editor_commit_entity_create_action();
                }

                if (igButton("Create Hole Entity", (ImVec2){0, 0})) {
                }

                igEndTabItem();
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
                                    material->color = V3(1, 0, 0);
                                    break;
                                }
                                case GOLF_MATERIAL_DIFFUSE_COLOR: {
                                    material->color = V3(1, 0, 0);
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
                                bool edit_done = false;
                                _golf_editor_undoable_igInputText("Texture", material->texture_path, GOLF_FILE_MAX_PATH, &edit_done, &material->texture, sizeof(material->texture), "Modify material texture");
                                if (edit_done) {
                                    material->texture = golf_data_get_texture(material->texture_path);
                                }
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
                                bool edit_done = false;
                                _golf_editor_undoable_igInputText("Texture", material->texture_path, GOLF_FILE_MAX_PATH, &edit_done, &material->texture, sizeof(material->texture), "Modify material texture");
                                if (edit_done) {
                                    material->texture = golf_data_get_texture(material->texture_path);
                                }
                                break;
                            }
                        }

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
                    new_material.color = V3(1, 0, 0);
                    _vec_push_and_fix_actions(&editor.level->materials, new_material);

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
                        golf_lightmap_image_init(lightmap_image, name, resolution, width, height, time_length, num_samples, data);
                        golf_free(data);
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
                    golf_lightmap_image_t new_lightmap_image;
                    golf_lightmap_image_init(&new_lightmap_image, "new", 256, 1, 1, 1, 1, image_data);
                    golf_free(image_data);
                    _vec_push_and_fix_actions(&editor.level->lightmap_images, new_lightmap_image);

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
        if (editor.selected_idxs.length > 0) {
            int idx = editor.selected_idxs.data[0];
            golf_entity_t *entity = &editor.level->entities.data[idx];
            switch (entity->type) {
                case MODEL_ENTITY: {
                    igText("TYPE: Model");
                    bool edit_done = false;
                    _golf_editor_undoable_igInputText("Model", entity->model.model_path, GOLF_FILE_MAX_PATH, &edit_done, &entity->model.model, sizeof(entity->model.model), "Modify model");
                    if (edit_done) {
                        entity->model.model = golf_data_get_model(entity->model.model_path);
                    }
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
            }

            bool edit_done = false;
            _golf_editor_undoable_igInputText("Name", entity->name, GOLF_MAX_NAME_LEN, &edit_done, NULL, 0, "Modify entity name");

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
            if (geo) {
                mat4 model_mat = mat4_identity();
                if (transform) {
                    model_mat = golf_transform_get_model_mat(*transform);
                }
                _golf_editor_edit_geo(geo, model_mat);
            }

            if (igButton("Delete Entity", (ImVec2){0, 0})) {
                _golf_editor_start_action_with_data(&entity->active, sizeof(entity->active), "Delete entity");
                entity->active = false;
                _golf_editor_commit_action();
            }
        }
        igEnd();
    }

    if (editor.gi_state.open_popup) {
        igOpenPopup_Str("Lightmap Generator Running", ImGuiPopupFlags_None);
        editor.gi_state.open_popup = false;
    }
    if (igBeginPopupModal("Lightmap Generator Running", NULL, ImGuiWindowFlags_None)) {
        igText("Progress: 1 / 10");
        if (editor.gi_running && !golf_gi_is_running(&editor.gi)) {
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
        mat4 model_mat = editor.edit_mode.model_mat;

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

                vec3 pos_avg = vec3_scale(vec3_add(pos0, pos1), 0.5f);
                float radius = vec3_distance(pos_avg, renderer->cam_pos) / CFG_NUM(editor_cfg, "edit_mode_line_size");

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
            float radius = vec3_distance(pos, renderer->cam_pos) / CFG_NUM(editor_cfg, "edit_mode_sphere_size");
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

        if (sphere_t < line_t && sphere_t < triangle_t) {
            editor.edit_mode.is_entity_hovered = true;
            int idx = point_idxs.data[sphere_idx];
            editor.edit_mode.hovered_entity = golf_edit_mode_entity_point(idx);
        }
        else if (line_t < sphere_t && line_t < triangle_t) {
            editor.edit_mode.is_entity_hovered = true;
            int idx0 = line_p0_idx.data[line_idx];
            int idx1 = line_p1_idx.data[line_idx];
            editor.edit_mode.hovered_entity = golf_edit_mode_entity_line(idx0, idx1);
        }
        else if (triangle_t < sphere_t && triangle_t < line_t) {
            editor.edit_mode.is_entity_hovered = true;
            int idx = face_idxs.data[triangle_idx];
            editor.edit_mode.hovered_entity = golf_edit_mode_entity_face(idx);
        }
        else {
            editor.edit_mode.is_entity_hovered = false;
        }

        if (!IO->WantCaptureMouse && !editor.mouse_down_in_imgui && inputs->mouse_clicked[SAPP_MOUSEBUTTON_LEFT]) {
            _golf_editor_edit_mode_select_entity(editor.edit_mode.hovered_entity);
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
        vec_vec3_t triangles;
        vec_init(&triangles, "editor");

        vec_int_t entity_idxs;
        vec_init(&entity_idxs, "editor");

        for (int i = 0; i < editor.level->entities.length; i++) {
            golf_entity_t *entity = &editor.level->entities.data[i];
            if (!entity->active) continue;

            golf_model_t *model = golf_entity_get_model(entity);
            golf_transform_t *transform = golf_entity_get_transform(entity);
            if (model && transform) {
                mat4 model_mat = golf_transform_get_model_mat(*transform);
                for (int j = 0; j < model->positions.length; j++) {
                    vec3 p0 = vec3_apply_mat4(model->positions.data[j + 0], 1, model_mat);
                    vec3 p1 = vec3_apply_mat4(model->positions.data[j + 1], 1, model_mat);
                    vec3 p2 = vec3_apply_mat4(model->positions.data[j + 2], 1, model_mat);
                    vec_push(&triangles, p0);
                    vec_push(&triangles, p1);
                    vec_push(&triangles, p2);
                    vec_push(&entity_idxs, i);
                }
            }
        }

        editor.hovered_idx = -1;
        float t;
        int idx;
        if (ray_intersect_triangles(inputs->mouse_ray_orig, inputs->mouse_ray_dir, triangles.data, triangles.length, &t, &idx)) {
            editor.hovered_idx = entity_idxs.data[idx];
        }

        if (!IO->WantCaptureMouse && !editor.mouse_down_in_imgui && inputs->mouse_clicked[SAPP_MOUSEBUTTON_LEFT]) {
            if (!inputs->button_down[SAPP_KEYCODE_LEFT_SHIFT]) {
                editor.selected_idxs.length = 0;
            }

            if (editor.hovered_idx >= 0) {
                int idx = -1;
                vec_find(&editor.selected_idxs, editor.hovered_idx, idx);
                if (idx == -1) {
                    vec_push(&editor.selected_idxs, editor.hovered_idx);
                }
            }

            if (editor.selected_idxs.length == 1) {
                int idx = editor.selected_idxs.data[0];
                golf_entity_t *entity = &editor.level->entities.data[idx];
                editor.gizmo.model_mat = golf_transform_get_model_mat(entity->model.transform);
            }
            else {
            }
        }

        vec_deinit(&triangles);
        vec_deinit(&entity_idxs);
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

                golf_lightmap_image_t *lightmap_image = gi_entity->lightmap_image;
                char name[GOLF_MAX_NAME_LEN];
                snprintf(name, GOLF_MAX_NAME_LEN, "%s", lightmap_image->name);
                _golf_editor_action_push_data(&action, lightmap_image, sizeof(golf_lightmap_image_t));
                golf_lightmap_image_init(lightmap_image, name, res, w, h, time_length, num_samples, data);
                golf_free(data);

                for (int i = 0; i < gi_entity->gi_lightmap_sections.length; i++) {
                    golf_gi_lightmap_section_t *gi_lightmap_section = &gi_entity->gi_lightmap_sections.data[i];
                    golf_lightmap_section_t *lightmap_section = gi_lightmap_section->lightmap_section;
                    _golf_editor_action_push_data(&action, lightmap_section, sizeof(golf_lightmap_section_t));

                    char name[GOLF_MAX_NAME_LEN];
                    snprintf(name, GOLF_MAX_NAME_LEN, "%s", lightmap_section->lightmap_name);
                    golf_lightmap_section_init(lightmap_section, name, gi_lightmap_section->lightmap_uvs, 0, gi_lightmap_section->lightmap_uvs.length);
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
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT]) {
            renderer->cam_azimuth_angle += 0.2f * dt * inputs->mouse_delta.x;
            renderer->cam_inclination_angle -= 0.2f * dt * inputs->mouse_delta.y;
        }
    }

    float cam_speed = 8.0f;
    if (!IO->WantCaptureKeyboard) {
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT] && inputs->button_down[SAPP_KEYCODE_W]) {
            renderer->cam_pos.x += cam_speed * dt * cosf(renderer->cam_azimuth_angle);
            renderer->cam_pos.z += cam_speed * dt * sinf(renderer->cam_azimuth_angle);
        }
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT] && inputs->button_down[SAPP_KEYCODE_S]) {
            renderer->cam_pos.x -= cam_speed * dt * cosf(renderer->cam_azimuth_angle);
            renderer->cam_pos.z -= cam_speed * dt * sinf(renderer->cam_azimuth_angle);
        }
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT] && inputs->button_down[SAPP_KEYCODE_D]) {
            renderer->cam_pos.x += cam_speed * dt * cosf(renderer->cam_azimuth_angle + 0.5f * MF_PI);
            renderer->cam_pos.z += cam_speed * dt * sinf(renderer->cam_azimuth_angle + 0.5f * MF_PI);
        }
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT] && inputs->button_down[SAPP_KEYCODE_A]) {
            renderer->cam_pos.x -= cam_speed * dt * cosf(renderer->cam_azimuth_angle + 0.5f * MF_PI);
            renderer->cam_pos.z -= cam_speed * dt * sinf(renderer->cam_azimuth_angle + 0.5f * MF_PI);
        }
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT] && inputs->button_down[SAPP_KEYCODE_Q]) {
            renderer->cam_pos.y -= cam_speed * dt;
        }
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT] && inputs->button_down[SAPP_KEYCODE_E]) {
            renderer->cam_pos.y += cam_speed * dt;
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
    bool in_edit_mode = editor.in_edit_mode;
    bool is_entity_hovered = editor.edit_mode.is_entity_hovered;
    golf_edit_mode_entity_t hovered_entity = editor.edit_mode.hovered_entity;
    return in_edit_mode && is_entity_hovered && golf_editor_edit_entities_compare(hovered_entity, entity);
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
