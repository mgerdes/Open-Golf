#include "golf/editor.h"

#include <string.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include "cimguizmo/cimguizmo.h"
#include "IconsFontAwesome5/IconsFontAwesome5.h"
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

    ImGuiIO *IO = igGetIO();
    IO->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    memset(&editor, 0, sizeof(editor));

    {
        editor.level = malloc(sizeof(golf_level_t));
        golf_file_t file = golf_file("data/levels/level-1/level-1.level");
        if (golf_file_load_data(&file)) {
            golf_level_load(editor.level, file.path, file.data, file.data_len);
            golf_file_free_data(&file);
        }
        else {
            golf_log_error("unable to load level file");
        }

        vec_init(&editor.undo_actions);
        vec_init(&editor.redo_actions);
        editor.started_action = false;

        editor.hovered_idx = -1;
        vec_init(&editor.selected_idxs);
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
        editor.gi.num_iterations = 1;
        editor.gi.num_dilates = 1;
        editor.gi.num_smooths = 1;
        editor.gi.gamma = 1;
        editor.gi.hemisphere_size = 128;
        editor.gi.z_near = 0.001f;
        editor.gi.z_far = 10.0f;
        editor.gi.interpolation_passes = 4;
        editor.gi.interpolation_threshold = 0.01f;
        editor.gi.camera_to_surface_distance_modifier = 0.0f;
    }
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
    vec_init(&action->datas);
}

static void _golf_editor_action_deinit(golf_editor_action_t *action) {
    for (int i = 0; i < action->datas.length; i++) {
        golf_editor_action_data_t data = action->datas.data[i];
        free(data.copy);
    }
    vec_deinit(&action->datas);
}

static void _golf_editor_action_push_data(golf_editor_action_t *action, void *data, int data_size) {
    golf_editor_action_data_t action_data; 
    action_data.size = data_size;
    action_data.ptr = data;
    action_data.copy = malloc(data_size);
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

#define _golf_editor_vec_push_and_fix_actions(v, val)\
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

static void _golf_editor_duplicate_selected_entities(void) {
    for (int i = 0; i < editor.selected_idxs.length; i++) {
        int idx = editor.selected_idxs.data[i];
        golf_entity_t *selected_entity = &editor.level->entities.data[idx];
        golf_entity_t entity_copy = golf_entity_make_copy(selected_entity);
        _golf_editor_vec_push_and_fix_actions(&editor.level->entities, entity_copy);
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

static void _golf_editor_edit_transform(golf_transform_t *transform) {
    if (igTreeNode_Str("Transform")) {
        _golf_editor_undoable_igInputFloat3("Position", (float*)&transform->position, "Modify transform position");
        _golf_editor_undoable_igInputFloat3("Scale", (float*)&transform->scale, "Modify transform scale");
        _golf_editor_undoable_igInputFloat4("Rotation", (float*)&transform->rotation, "Modify transform rotation");
        igTreePop();
    }
}

static void _golf_editor_edit_lightmap(golf_lightmap_t *lightmap) {
    bool queue_action = false;
    bool queue_commit = false;
    bool queue_decommit = false;

    if (igTreeNode_Str("Lightmap")) {
        igInputInt("Resolution", &lightmap->resolution, 0, 0, ImGuiInputTextFlags_None);
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

        igText("Image Size <%d, %d>", lightmap->image_width, lightmap->image_height);
        igImage((ImTextureID)(uintptr_t)lightmap->sg_image.id, 
                (ImVec2){(float)lightmap->image_width, (float)lightmap->image_height},
                (ImVec2){0, 0},
                (ImVec2){1, 1}, 
                (ImVec4){1, 1, 1, 1},
                (ImVec4){1, 1, 1, 1});

        igTreePop();
    }

    if (queue_action) {
        golf_editor_action_t action;
        _golf_editor_action_init(&action, "Modify lightmap");
        _golf_editor_action_push_data(&action, lightmap, sizeof(golf_lightmap_t));
        _golf_editor_queue_start_action(action);
    }
    if (queue_commit) {
        unsigned char *data = malloc(lightmap->image_width * lightmap->image_height);  
        memset(data, 0xFF, lightmap->image_width * lightmap->image_height);
        golf_lightmap_init(lightmap, lightmap->resolution, lightmap->image_width, lightmap->image_height, data, lightmap->uvs);
        free(data);
        _golf_editor_queue_commit_action();
    }
    if (queue_decommit) {
        _golf_editor_queue_decommit_action();
    }
}

void golf_editor_update(float dt) {
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

    if (editor.selected_idxs.length > 0) {
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

                    _golf_editor_undoable_igInputInt("Num Iterations", &editor.gi.num_iterations, "Modify GI Settings - Num Iterations");
                    _golf_editor_undoable_igInputInt("Num Dilates", &editor.gi.num_dilates, "Modify GI Settings - Num Dilates");
                    _golf_editor_undoable_igInputInt("Num Smooths", &editor.gi.num_smooths, "Modify GI Settings - Num Smooths");
                    _golf_editor_undoable_igInputFloat("Gamma", &editor.gi.gamma, "Modify GI Settigs - Gamma");

                    _golf_editor_undoable_igInputInt("Hemisphere Size", &editor.gi.hemisphere_size, "Modify GI Settings - Hemisphere Size");
                    if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                        igBeginTooltip();
                        igText("Resolution of the hemisphere renderings. must be a power of two! typical: 64.");
                        igEndTooltip();
                    }

                    _golf_editor_undoable_igInputFloat("Z Near", &editor.gi.z_near, "Modify GI Settings - Z Near");
                    if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                        igBeginTooltip();
                        igText("Hemisphere min draw distances.");
                        igEndTooltip();
                    }

                    _golf_editor_undoable_igInputFloat("Z Far", &editor.gi.z_far, "Modify GI Settings - Z Far");
                    if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                        igBeginTooltip();
                        igText("Hemisphere max draw distances.");
                        igEndTooltip();
                    }

                    _golf_editor_undoable_igInputInt("Interpolation Passes", &editor.gi.interpolation_passes, "Modify GI Settings - Interpolation Passes");
                    if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                        igBeginTooltip();
                        igText("Hierarchical selective interpolation passes (0-8; initial step size = 2^passes).");
                        igEndTooltip();
                    }

                    _golf_editor_undoable_igInputFloat("Interpolation Threshold", &editor.gi.interpolation_threshold, "Modify GI Settings - Interpolation Threshold");
                    if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                        igBeginTooltip();
                        igText("Error value below which lightmap pixels are interpolated instead of rendered.");
                        igText("Use output image from LM_DEBUG_INTERPOLATION to determine a good value.");
                        igText("Values around and below 0.01 are probably ok.");
                        igText("The lower the value, the more hemispheres are rendered -> sower, but possibly better quality.");
                        igEndTooltip();
                    }

                    _golf_editor_undoable_igInputFloat("Camera to Surface Distance Modifier", &editor.gi.camera_to_surface_distance_modifier, "Modify GI Settings - Camera to Surface Distance Modifier");
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
                        if (!editor.lightmap_generator_running) {
                            golf_log_note("Lightmap Generator Started");
                            golf_lightmap_generator_t *generator = &editor.lightmap_generator;  
                            golf_lightmap_generator_init(generator, true, true, 
                                    editor.gi.gamma,
                                    editor.gi.num_iterations,
                                    editor.gi.num_dilates,
                                    editor.gi.num_smooths,
                                    editor.gi.hemisphere_size,
                                    editor.gi.z_near,
                                    editor.gi.z_far,
                                    editor.gi.interpolation_passes,
                                    editor.gi.interpolation_threshold,
                                    editor.gi.camera_to_surface_distance_modifier);
                            for (int i = 0; i < editor.level->entities.length; i++) {
                                golf_entity_t *entity = &editor.level->entities.data[i];
                                if (!entity->active) continue;
                                golf_lightmap_t *lightmap = golf_entity_get_lightmap(entity);
                                golf_transform_t *transform = golf_entity_get_transform(entity);
                                golf_model_t *model = golf_entity_get_model(entity);
                                if (lightmap && transform && model) {
                                    mat4 model_mat = golf_transform_get_model_mat(*transform);
                                    golf_lightmap_generator_add_entity(generator, model, model_mat, lightmap);
                                }
                            }
                            golf_lightmap_generator_start(generator);
                            editor.lightmap_generator_running = true;
                            editor.gi.open_popup = true;
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

    {
        igBegin("RightTop", NULL, ImGuiWindowFlags_NoTitleBar);
        if (igBeginTabBar("", ImGuiTabBarFlags_None)) {
            if (igBeginTabItem("Entities", NULL, ImGuiTabItemFlags_None)) {
                for (int i = 0; i < editor.level->entities.length; i++) {
                    golf_entity_t *entity = &editor.level->entities.data[i];
                    if (!entity->active) continue;
                    switch (entity->type) {
                        case MODEL_ENTITY: {
                            igSelectable_Bool("Model Entity", false, ImGuiSelectableFlags_None, (ImVec2){0, 0});
                            break;
                        }
                        case BALL_START_ENTITY: {
                            igSelectable_Bool("Ball Start Entity", false, ImGuiSelectableFlags_None, (ImVec2){0, 0});
                            break;
                        }
                        case HOLE_ENTITY: {
                            igSelectable_Bool("Hole Entity", false, ImGuiSelectableFlags_None, (ImVec2){0, 0});
                            break;
                        }
                    }
                }

                if (igButton("Create Model Entity", (ImVec2){0, 0})) {
                    golf_transform_t transform = golf_transform(V3(0, 0, 0), V3(1, 1, 1), QUAT(0, 0, 0, 1));

                    golf_lightmap_t lightmap;
                    {
                        const char *model_path = "data/models/cube.obj";
                        golf_model_t *model = golf_data_get_model(model_path);
                        unsigned char image_data[1] = { 0xFF };

                        vec_vec2_t uvs;
                        vec_init(&uvs);
                        for (int i = 0; i < model->positions.length; i++) {
                            vec_push(&uvs, V2(0, 0));
                        }

                        golf_lightmap_init(&lightmap, 256, 1, 1, image_data, uvs);
                        vec_deinit(&uvs);
                    }

                    golf_entity_t entity = golf_entity_model(transform, "data/models/cube.obj", lightmap);
                    _golf_editor_vec_push_and_fix_actions(&editor.level->entities, entity);
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
                        const char *items[] = { "Texture", "Color", "Diffuse" };
                        igCombo_Str_arr("Type", (int*)&material->type, items, 3, 0);
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
                                    break;;
                                }
                            }
                        }

                        _golf_editor_undoable_igInputText("Name", material->name, GOLF_MATERIAL_NAME_MAX_LEN, NULL, NULL, 0, "Modify material name");

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
                    snprintf(new_material.name, GOLF_MATERIAL_NAME_MAX_LEN, "%s", "new");
                    new_material.type = GOLF_MATERIAL_COLOR;
                    new_material.color = V3(1, 0, 0);
                    _golf_editor_vec_push_and_fix_actions(&editor.level->materials, new_material);

                    golf_material_t *material = &vec_last(&editor.level->materials);
                    material->active = false;
                    _golf_editor_start_action_with_data(&material->active, sizeof(material->active), "Create material");
                    material->active = true;
                    _golf_editor_commit_action();
                }
                igEndTabItem();
            }
            igEndTabBar();
        }
        igEnd();
    }

    {
        igBegin("RightBottom", NULL, ImGuiWindowFlags_NoTitleBar);
        if (editor.selected_idxs.length == 1) {
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
            }

            golf_transform_t *transform = golf_entity_get_transform(entity);
            if (transform) {
                _golf_editor_edit_transform(transform);
            }

            golf_lightmap_t *lightmap = golf_entity_get_lightmap(entity);
            if (lightmap) {
                _golf_editor_edit_lightmap(lightmap);
            }

            if (igButton("Delete Entity", (ImVec2){0, 0})) {
                _golf_editor_start_action_with_data(&entity->active, sizeof(entity->active), "Delete entity");
                entity->active = false;
                _golf_editor_commit_action();
            }
        }
        else {
        }
        igEnd();
    }

    if (editor.gi.open_popup) {
        igOpenPopup_Str("Lightmap Generator Running", ImGuiPopupFlags_None);
        editor.gi.open_popup = false;
    }
    if (igBeginPopupModal("Lightmap Generator Running", NULL, ImGuiWindowFlags_None)) {
        igText("Progress: 1 / 10");
        if (editor.lightmap_generator_running && 
                !golf_lightmap_generator_is_running(&editor.lightmap_generator)) {
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

    {
        vec_vec3_t triangles;
        vec_init(&triangles);

        vec_int_t entity_idxs;
        vec_init(&entity_idxs);

        for (int i = 0; i < editor.level->entities.length; i++) {
            golf_entity_t *entity = &editor.level->entities.data[i];
            if (!entity->active) continue;

            switch (entity->type) {
                case MODEL_ENTITY: 
                case BALL_START_ENTITY:
                case HOLE_ENTITY: {
                    golf_model_t *model = NULL;
                    if (entity->type == MODEL_ENTITY) {
                        model = entity->model.model;
                    }
                    else if (entity->type == BALL_START_ENTITY) {
                        model = golf_data_get_model("data/models/sphere.obj");
                    }
                    else if (entity->type == HOLE_ENTITY) {
                        model = golf_data_get_model("data/models/hole.obj");
                    }

                    golf_transform_t *transform = golf_entity_get_transform(entity);
                    if (!transform) {
                        golf_log_warning("Could not get transform for entity");
                    }
                    else {
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
            }
        }

        editor.hovered_idx = -1;
        float t;
        int idx;
        if (ray_intersect_triangles(inputs->mouse_ray_orig, inputs->mouse_ray_dir, triangles.data, triangles.length, &t, &idx)) {
            editor.hovered_idx = entity_idxs.data[idx];
        }

        if (!IO->WantCaptureMouse && inputs->mouse_clicked[SAPP_MOUSEBUTTON_LEFT]) {
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

    if (editor.lightmap_generator_running) {
        golf_lightmap_generator_t *generator = &editor.lightmap_generator;
        if (!golf_lightmap_generator_is_running(generator)) {
            golf_log_note("Lightmap Generator Finished");

            golf_editor_action_t action;
            _golf_editor_action_init(&action, "Create lightmap");
            for (int i = 0; i < generator->entities.length; i++) {
                golf_lightmap_entity_t *lm_entity = &generator->entities.data[i];

                unsigned char *data = malloc(lm_entity->image_width * lm_entity->image_height);
                for (int i = 0; i < lm_entity->image_width * lm_entity->image_height; i++) {
                    float a = lm_entity->image_data[i];
                    if (a > 1.0f) a = 1.0f;
                    if (a < 0.0f) a = 0.0f;
                    data[i] = (unsigned char)(0xFF * a);
                }

                _golf_editor_action_push_data(&action, lm_entity->lightmap, sizeof(golf_lightmap_t));
                golf_lightmap_init(lm_entity->lightmap, lm_entity->resolution, lm_entity->image_width, lm_entity->image_height, data, lm_entity->lightmap_uvs);
                free(data);
            }
            _golf_editor_start_action(action);
            _golf_editor_commit_action();

            golf_lightmap_generator_deinit(generator);

            editor.lightmap_generator_running = false;
        }
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
