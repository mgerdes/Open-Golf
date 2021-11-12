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
    }

    vec_init(&editor.actions);
    editor.started_action = false;

    editor.hovered_idx = -1;
    vec_init(&editor.selected_idxs);

    memset(&editor.gizmo, 0, sizeof(editor.gizmo));
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

static void _golf_editor_action_init(golf_editor_action_t *action) {
    vec_init(&action->datas);
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

static void _golf_editor_commit_action(void) {
    if (!editor.started_action) {
        golf_log_warning("Commiting action without one started");
        return;
    }

    editor.started_action = false;
    vec_push(&editor.actions, editor.cur_action);
}

static void _golf_editor_decommit_action(void) {
    if (!editor.started_action) {
        golf_log_warning("Commiting action without one started");
        return;
    }

    for (int i = 0; i < editor.cur_action.datas.length; i++) {
        free(editor.cur_action.datas.data[i].copy);
    }
    editor.started_action = false;
}

static void _golf_editor_fix_actions(char *new_ptr, char *old_ptr_start, char *old_ptr_end) {
    for (int i = 0; i < editor.actions.length; i++) {
        golf_editor_action_t *action = &editor.actions.data[i];
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
    if (editor.actions.length == 0) {
        return;
    }

    golf_editor_action_t *action = &vec_pop(&editor.actions); 
    for (int i = 0; i < action->datas.length; i++) {
        golf_editor_action_data_t *action_data = &action->datas.data[i];
        memcpy(action_data->ptr, action_data->copy, action_data->size);
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
            }
            if (igMenuItem_Bool("Duplicate", NULL, false, true)) {
                /*
                for (int i = 0; i < editor.selected_idxs.length; i++) {
                    int idx = editor.selected_idxs.data[i];
                    golf_entity_t selected_entity = editor.entities.data[idx];
                    if (selected_entity.type == MODEL_ENTITY) {
                        golf_model_entity_t model_entity = selected_entity.model;

                        golf_entity_t new_entity;
                        new_entity.type = MODEL_ENTITY;
                        memcpy(&new_entity.model, &model_entity, sizeof(golf_model_entity_t));

                        char *entity_active_ptr = (char*)editor.entity_active.data;
                        char *entity_ptr = (char*)editor.entities.data;
                        vec_push(&editor.entity_active, false);
                        vec_push(&editor.entities, new_entity);
                        if (entity_active_ptr != (char*)editor.entity_active.data) {
                            _golf_editor_fix_actions((char*)editor.entity_active.data, entity_active_ptr, entity_active_ptr + sizeof(bool) * (editor.entity_active.length - 1));
                        }
                        if (entity_ptr != (char*)editor.entities.data) {
                            _golf_editor_fix_actions((char*)editor.entities.data, entity_ptr, entity_ptr + sizeof(golf_entity_t) * (editor.entities.length - 1));
                        }

                        _golf_editor_start_action((char*)&vec_last(&editor.entity_active), sizeof(bool));
                        vec_last(&editor.entity_active) = true;
                        _golf_editor_commit_action();
                    }
                }
                */
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
            _golf_editor_action_init(&action);
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

        igEnd();
    }

    {
        igBegin("RightTop", NULL, ImGuiWindowFlags_NoTitleBar);
        if (igBeginTabBar("", ImGuiTabBarFlags_None)) {
            if (igBeginTabItem("Entities", NULL, ImGuiTabItemFlags_None)) {
                for (int i = 0; i < editor.level->entities.length; i++) {
                    golf_entity_t *entity = &editor.level->entities.data[i];
                    switch (entity->type) {
                        case MODEL_ENTITY: {
                            //golf_model_entity_t *model_entity = &entity->model;
                            igSelectable_Bool("Model entity", false, ImGuiSelectableFlags_None, (ImVec2){0, 0});
                            break;
                        }
                        case BALL_START_ENTITY:
                            break;
                    }
                }
                igEndTabItem();
            }

            if (igBeginTabItem("Materials", NULL, ImGuiTabItemFlags_None)) {
                for (int i = 0; i < editor.level->materials.length; i++) {
                    igPushID_Int(i);
                    golf_material_t *material = &editor.level->materials.data[i];
                    if (igTreeNodeEx_StrStr("material", ImGuiTreeNodeFlags_None, "%s", material->name)) {
                        igInputText("Name", material->name, GOLF_MATERIAL_NAME_MAX_LEN, ImGuiInputTextFlags_None, NULL, NULL);
                        if (igIsItemActivated()) {
                            golf_editor_action_t action;
                            _golf_editor_action_init(&action);
                            _golf_editor_action_push_data(&action, material->name, GOLF_MATERIAL_NAME_MAX_LEN);
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

                        igInputText("Texture", material->texture_path, GOLF_FILE_MAX_PATH, ImGuiInputTextFlags_None, NULL, NULL);
                        if (igIsItemActivated()) {
                            golf_editor_action_t action;
                            _golf_editor_action_init(&action);
                            _golf_editor_action_push_data(&action, material->texture_path, GOLF_FILE_MAX_PATH);
                            _golf_editor_action_push_data(&action, &material->texture, sizeof(material->texture));
                            _golf_editor_queue_start_action(action);
                        }
                        if (igIsItemDeactivated()) {
                            material->texture = golf_data_get_texture(material->texture_path);
                            if (igIsItemDeactivatedAfterEdit()) {
                                _golf_editor_queue_commit_action();
                            }
                            else {
                                _golf_editor_queue_decommit_action();
                            }
                        }

                        if (igButton("Delete Material", (ImVec2){0, 0})) {
                            vec_splice(&editor.level->materials, i, 1);
                            i--;
                        }
                        igTreePop();
                    }
                    igPopID();
                }

                if (igButton("Create Material", (ImVec2){0, 0})) {
                    golf_material_t new_material;
                    memset(&new_material, 0, sizeof(golf_material_t));
                    snprintf(new_material.name, GOLF_MATERIAL_NAME_MAX_LEN, "%s", "default");
                    snprintf(new_material.texture_path, GOLF_FILE_MAX_PATH, "%s", "data/textures/fallback.png");
                    new_material.texture = golf_data_get_texture(new_material.texture_path);
                    _golf_editor_vec_push_and_fix_actions(&editor.level->materials, new_material);
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
                    golf_model_entity_t *model_entity = &entity->model;
                    igText("TYPE: Model Entity");
                    igInputFloat3("Position", (float*)&model_entity->transform.position, "%.3f", ImGuiInputTextFlags_None);
                    if (igIsItemActivated()) {
                        golf_editor_action_t action;
                        _golf_editor_action_init(&action);
                        _golf_editor_action_push_data(&action, &model_entity->transform.position, sizeof(model_entity->transform.position));
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
                    break;
                }
                case BALL_START_ENTITY:
                    break;
            }
        }
        else {
        }
        igEnd();
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
            if (!entity->active) {
                continue;
            }

            switch (entity->type) {
                case MODEL_ENTITY: {
                    golf_model_entity_t *model_entity = &entity->model;
                    golf_model_t *model = model_entity->model;
                    mat4 model_mat = golf_transform_get_model_mat(model_entity->transform);
                    for (int j = 0; j < model->positions.length; j++) {
                        vec3 p0 = vec3_apply_mat4(model->positions.data[j + 0], 1, model_mat);
                        vec3 p1 = vec3_apply_mat4(model->positions.data[j + 1], 1, model_mat);
                        vec3 p2 = vec3_apply_mat4(model->positions.data[j + 2], 1, model_mat);
                        vec_push(&triangles, p0);
                        vec_push(&triangles, p1);
                        vec_push(&triangles, p2);
                        vec_push(&entity_idxs, i);
                    }
                    break;
                }
                case BALL_START_ENTITY:
                    break;
            }
        }

        editor.hovered_idx = -1;
        float t;
        int idx;
        if (ray_intersect_triangles(inputs->mouse_ray_orig, inputs->mouse_ray_dir, triangles.data, triangles.length, mat4_identity(), &t, &idx)) {
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

    if (!IO->WantCaptureMouse) {
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT]) {
            renderer->cam_azimuth_angle += 0.2f * dt * inputs->mouse_delta.x;
            renderer->cam_inclination_angle -= 0.2f * dt * inputs->mouse_delta.y;
        }
    }

    float cam_speed = 2.0f;
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
    }
}