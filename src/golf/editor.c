#include "golf/editor.h"

#include <string.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "3rd_party/cimgui/cimgui.h"
#include "3rd_party/cimguizmo/cimguizmo.h"
#include "3rd_party/IconsFontAwesome5/IconsFontAwesome5.h"
#include "golf/data.h"
#include "golf/inputs.h"
#include "golf/log.h"
#include "golf/renderer.h"

static golf_editor_t editor;
static golf_inputs_t *inputs;

golf_editor_t *golf_editor_get(void) {
    return &editor;
}

void golf_editor_init(void) {
    inputs = golf_inputs_get();
    ImGuiIO *IO = igGetIO();
    IO->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    vec_init(&editor.entity_active);
    vec_init(&editor.entities);

    {
        golf_model_entity_t model_entity;
        snprintf(model_entity.model_path, GOLF_FILE_MAX_PATH, "%s", "data/models/cube.obj");
        model_entity.model_mat = mat4_translation(V3(2, 0, 2));
        model_entity.bounds[0] = model_entity.bounds[1] = model_entity.bounds[2] = -1;
        model_entity.bounds[3] = model_entity.bounds[4] = model_entity.bounds[5] = 1;

        golf_entity_t entity;
        entity.type = MODEL_ENTITY;
        entity.model_entity = model_entity;

        vec_push(&editor.entity_active, true);
        vec_push(&editor.entities, entity);
    }

    {
        golf_model_entity_t model_entity;
        snprintf(model_entity.model_path, GOLF_FILE_MAX_PATH, "%s", "data/models/cube.obj");
        model_entity.model_mat = mat4_translation(V3(-2, 0, -2));
        model_entity.bounds[0] = model_entity.bounds[1] = model_entity.bounds[2] = -1;
        model_entity.bounds[3] = model_entity.bounds[4] = model_entity.bounds[5] = 1;

        golf_entity_t entity;
        entity.type = MODEL_ENTITY;
        entity.model_entity = model_entity;

        vec_push(&editor.entity_active, true);
        vec_push(&editor.entities, entity);
    }

    vec_init(&editor.actions);
    editor.started_modify_data_action = false;

    editor.hovered_entity_idx = -1;
    editor.selected_entity_idx = -1;

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

static void _golf_editor_start_modify_data_action(void *data, int data_size) {
    if (editor.started_modify_data_action) {
        golf_log_warning("Starting modify data action with one already started");
        return;
    }

    editor.started_modify_data_action = true;
    editor.modify_data_action.type = MODIFY_DATA_ACTION;
    editor.modify_data_action.data_size = data_size;
    editor.modify_data_action.data = data;
    editor.modify_data_action.data_copy = malloc(data_size);
    memcpy(editor.modify_data_action.data_copy, data, data_size);
}

static void _golf_editor_commit_modify_data_action(void) {
    if (!editor.started_modify_data_action) {
        golf_log_warning("Commiting modify data action without one started");
        return;
    }

    editor.started_modify_data_action = false;
    vec_push(&editor.actions, editor.modify_data_action);
}

static void _golf_editor_undo_action(void) {
    if (editor.actions.length == 0) {
        return;
    }

    golf_editor_action_t action = vec_pop(&editor.actions);	
    if (action.type == MODIFY_DATA_ACTION) {
        memcpy(action.data, action.data_copy, action.data_size);
        free(action.data_copy);
    }
    else {
        golf_log_warning("Undo action type not handled %d", action.type);
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
                if (editor.selected_entity_idx >= 0) {
                    golf_entity_t selected_entity = editor.entities.data[editor.selected_entity_idx];

                    if (selected_entity.type == MODEL_ENTITY) {
                        golf_model_entity_t model_entity = selected_entity.model_entity;

                        golf_entity_t new_entity;
                        new_entity.type = MODEL_ENTITY;
                        memcpy(&new_entity.model_entity, &model_entity, sizeof(golf_model_entity_t));

                        vec_push(&editor.entity_active, true);
                        vec_push(&editor.entities, new_entity);
                    }
                }
            }
            igEndMenu();
        }
        igEndMenuBar();
    }

    ImGuiID dock_main_id = igGetID_Str("dockspace");
    igDockSpace(dock_main_id, (ImVec2){0, 0}, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoTabBar, NULL);

    static bool built_dockspace = false;
    if (!built_dockspace) {
        built_dockspace = true;

        igDockBuilderRemoveNode(dock_main_id); 
        igDockBuilderAddNode(dock_main_id, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_DockSpace);
        igDockBuilderSetNodeSize(dock_main_id, viewport->Size);

        ImGuiID dock_id_right = igDockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.30f, NULL, &dock_main_id);
        ImGuiID dock_id_top = igDockBuilderSplitNode(dock_main_id, ImGuiDir_Up, 0.05f, NULL, &dock_main_id);

        igDockBuilderDockWindow("Viewport", dock_main_id);
        igDockBuilderDockWindow("Top", dock_id_top);
        igDockBuilderDockWindow("Right", dock_id_right);
        igDockBuilderFinish(dock_main_id);
    }

    ImGuiDockNode *central_node = igDockBuilderGetCentralNode(dock_main_id);
    golf_renderer_t *renderer = golf_renderer_get();
    renderer->viewport_pos = V2(central_node->Pos.x, central_node->Pos.y);
    renderer->viewport_size = V2(central_node->Size.x, central_node->Size.y);

    if (editor.selected_entity_idx >= 0 && 
            !editor.entity_active.data[editor.selected_entity_idx]) {
        editor.selected_entity_idx = -1;
    }

    if (editor.selected_entity_idx >= 0) {
        ImGuizmo_SetRect(renderer->viewport_pos.x, renderer->viewport_pos.y, renderer->viewport_size.x, renderer->viewport_size.y);
        mat4 view_mat_t = mat4_transpose(renderer->view_mat);
        mat4 proj_mat_t = mat4_transpose(renderer->proj_mat);

        golf_entity_t *entity = &editor.entities.data[editor.selected_entity_idx];
        float *bounds = NULL;
        //if (editor.gizmo.bounds_mode_on) {
            //bounds = entity->bounds;
        //}

        float snap_values[3];
        float *snap = NULL;
        float bounds_snap_values[3];
        float *bounds_snap = NULL;
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

            bounds_snap_values[0] = bounds_snap_values[1] = bounds_snap_values[2] = editor.gizmo.scale_snap;
            bounds_snap = bounds_snap_values;
        }

        mat4 *model_mat_ptr = golf_entity_get_model_mat_ptr(entity);
        mat4 model_mat = mat4_transpose(*model_mat_ptr);
        ImGuizmo_Manipulate(view_mat_t.m, proj_mat_t.m, editor.gizmo.operation, 
                editor.gizmo.mode, model_mat.m, NULL, snap, bounds, bounds_snap);
        golf_entity_set_model_mat(entity, mat4_transpose(model_mat));

        if (!editor.gizmo.is_using && ImGuizmo_IsUsing()) {
            _golf_editor_start_modify_data_action(model_mat_ptr, sizeof(mat4));
        }
        if (editor.gizmo.is_using && !ImGuizmo_IsUsing()) {
            _golf_editor_commit_modify_data_action();
        }
        editor.gizmo.is_using = ImGuizmo_IsUsing();
    }

    {
        igBegin("Top", NULL, ImGuiWindowFlags_NoTitleBar);
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
                igText("Rotate (E}");
                igEndTooltip();
            }
        }
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
        igBegin("Right", NULL, ImGuiWindowFlags_NoTitleBar);
        if (editor.selected_entity_idx >= 0) {
            golf_entity_t entity = editor.entities.data[editor.selected_entity_idx];

            /*
            golf_model_entity_t *entity = editor.model_entities.data[editor.selected_entity_idx];
            mat4 model_mat = mat4_transpose(entity->model_mat);
            float translation[3];
            float rotation[3];
            float scale[3];
            ImGuizmo_DecomposeMatrixToComponents(model_mat.m, translation, rotation, scale);
            bool modified = false;
            igInputFloat3("Translation", translation, "%.3f", ImGuiInputTextFlags_None);
            igInputFloat3("Rotation", rotation, "%.3f", ImGuiInputTextFlags_None);
            igInputFloat3("Scale", scale, "%.3f", ImGuiInputTextFlags_None);
            ImGuizmo_RecomposeMatrixFromComponents(translation, rotation, scale, model_mat.m);

            if (!mat4_equal(mat4_transpose(model_mat), entity->model_mat)) {
                //_golf_editor_start_modify_data_action(&entity->model_mat, sizeof(entity->model_mat));
                entity->model_mat = mat4_transpose(model_mat);
                _golf_editor_commit_modify_data_action();
            }

            igInputText("Model Path", editor.model_path_temp, GOLF_FILE_MAX_PATH,
                    ImGuiInputTextFlags_None, NULL, NULL);
            if (igIsItemDeactivatedAfterEdit()) {
                //_golf_editor_start_modify_data_action(entity->model_path, GOLF_FILE_MAX_PATH);
                snprintf(entity->model_path, GOLF_FILE_MAX_PATH, "%s", editor.model_path_temp);
                _golf_editor_commit_modify_data_action();
            }
            */
        }
        igEnd();
    }

    igEnd();

    {
        vec_vec3_t triangles;
        vec_init(&triangles);

        vec_int_t entity_idxs;
        vec_init(&entity_idxs);

        for (int i = 0; i < editor.entities.length; i++) {
            golf_entity_t entity = editor.entities.data[i];
            if (!editor.entity_active.data[i]) {
                continue;
            }

            if (entity.type == MODEL_ENTITY) {
                golf_model_entity_t model_entity = entity.model_entity;
                golf_data_model_t *model = golf_data_get_model(model_entity.model_path);

                for (int j = 0; j < model->positions.length; j += 3) {
                    vec3 p0 = vec3_apply_mat4(model->positions.data[j + 0], 1, model_entity.model_mat);
                    vec3 p1 = vec3_apply_mat4(model->positions.data[j + 1], 1, model_entity.model_mat);
                    vec3 p2 = vec3_apply_mat4(model->positions.data[j + 2], 1, model_entity.model_mat);
                    vec_push(&triangles, p0);
                    vec_push(&triangles, p1);
                    vec_push(&triangles, p2);
                    vec_push(&entity_idxs, i);
                }
            }
        }

        editor.hovered_entity_idx = -1;
        float t;
        int idx;
        if (ray_intersect_triangles(inputs->mouse_ray_orig, inputs->mouse_ray_dir, triangles.data, triangles.length, mat4_identity(), &t, &idx)) {
            editor.hovered_entity_idx = entity_idxs.data[idx];
        }

        if (!IO->WantCaptureMouse && inputs->mouse_clicked[SAPP_MOUSEBUTTON_LEFT]) {
            editor.selected_entity_idx = editor.hovered_entity_idx;
            if (editor.hovered_entity_idx >= 0) {
                golf_entity_t entity = editor.entities.data[editor.selected_entity_idx];
                if (entity.type == MODEL_ENTITY) {
                    golf_model_entity_t model_entity = entity.model_entity;
                    editor.gizmo.model_mat = mat4_transpose(model_entity.model_mat);
                    snprintf(editor.model_path_temp, GOLF_FILE_MAX_PATH, "%s", model_entity.model_path);
                }
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

    if (!IO->WantCaptureKeyboard) {
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT] && inputs->button_down[SAPP_KEYCODE_W]) {
            renderer->cam_pos.x += 8.0f * dt * cosf(renderer->cam_azimuth_angle);
            renderer->cam_pos.z += 8.0f * dt * sinf(renderer->cam_azimuth_angle);
        }
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT] && inputs->button_down[SAPP_KEYCODE_S]) {
            renderer->cam_pos.x -= 8.0f * dt * cosf(renderer->cam_azimuth_angle);
            renderer->cam_pos.z -= 8.0f * dt * sinf(renderer->cam_azimuth_angle);
        }
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT] && inputs->button_down[SAPP_KEYCODE_D]) {
            renderer->cam_pos.x += 8.0f * dt * cosf(renderer->cam_azimuth_angle + 0.5f * MF_PI);
            renderer->cam_pos.z += 8.0f * dt * sinf(renderer->cam_azimuth_angle + 0.5f * MF_PI);
        }
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT] && inputs->button_down[SAPP_KEYCODE_A]) {
            renderer->cam_pos.x -= 8.0f * dt * cosf(renderer->cam_azimuth_angle + 0.5f * MF_PI);
            renderer->cam_pos.z -= 8.0f * dt * sinf(renderer->cam_azimuth_angle + 0.5f * MF_PI);
        }
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT] && inputs->button_down[SAPP_KEYCODE_Q]) {
            renderer->cam_pos.y -= 8.0f * dt;
        }
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_RIGHT] && inputs->button_down[SAPP_KEYCODE_E]) {
            renderer->cam_pos.y += 8.0f * dt;
        }
        if (inputs->button_down[SAPP_KEYCODE_LEFT_CONTROL] && inputs->button_clicked[SAPP_KEYCODE_Z]) {
            _golf_editor_undo_action();
        }
    }
}
