#ifndef _GOLF_EDITOR_H
#define _GOLF_EDITOR_H

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include "common/bvh.h"
#include "common/file.h"
#include "common/level.h"
#include "common/maths.h"
#include "common/vec.h"
#include "editor/gi.h"
#include "editor/gizmo.h"

typedef enum golf_edit_mode_entity_type {
    GOLF_EDIT_MODE_ENTITY_FACE,
    GOLF_EDIT_MODE_ENTITY_LINE,
    GOLF_EDIT_MODE_ENTITY_POINT,
} golf_edit_mode_entity_type_t;

typedef struct golf_edit_mode_entity {
    golf_edit_mode_entity_type_t type; 
    int idx, idx2;
} golf_edit_mode_entity_t;
typedef vec_t(golf_edit_mode_entity_t) vec_golf_edit_mode_entity_t;
golf_edit_mode_entity_t golf_edit_mode_entity_face(int face_idx);
golf_edit_mode_entity_t golf_edit_mode_entity_line(int point0_idx, int point1_idx);
golf_edit_mode_entity_t golf_edit_mode_entity_point(int point_idx);

typedef struct golf_editor_action_data {
    int size;
    char *ptr;
    char *copy;
} golf_editor_action_data_t;
typedef vec_t(golf_editor_action_data_t) vec_golf_editor_action_data_t;

typedef struct golf_editor_action {
    const char *name;
    vec_golf_editor_action_data_t datas;
} golf_editor_action_t;
typedef vec_t(golf_editor_action_t) vec_golf_editor_action_t;

typedef struct golf_editor {
    char level_path[GOLF_FILE_MAX_PATH];
    golf_level_t *level;

    bool started_action, has_queued_action, has_queued_commit, has_queued_decommit;
    golf_editor_action_t cur_action, queued_action;
    vec_golf_editor_action_t undo_actions, redo_actions;

    bool mouse_down, mouse_down_in_imgui;

    golf_bvh_t bvh;
    int hovered_idx;
    vec_int_t selected_idxs;

    bool gi_running;
    golf_gi_t gi;

    bool in_edit_mode;
    struct {
        golf_transform_t transform;
        golf_geo_t *geo;

        bool is_entity_hovered;
        golf_edit_mode_entity_t hovered_entity;
        vec_golf_edit_mode_entity_t selected_entities;

        vec_vec3_t starting_positions;
        vec_int_t point_idxs;
    } edit_mode;

    golf_gizmo_t gizmo;
    vec_golf_transform_t starting_transforms;

    struct {
        bool creating_hole;
        bool open_popup;
        int num_iterations, 
            num_dilates,
            num_smooths,
            hemisphere_size,
            interpolation_passes;
        float gamma,
              z_near,
              z_far,
              interpolation_threshold,
              camera_to_surface_distance_modifier;
    } gi_state;

    struct {
        float azimuth_angle, inclination_angle;
    } camera;

    struct {
        bool open_popup;
        char search[GOLF_FILE_MAX_PATH];
        golf_data_type_t type;
        char *path;
        void **data;
    } file_picker;

    struct {
        bool is_open;
        vec2 p0, p1;
        vec_golf_edit_mode_entity_t hovered_entities;
    } select_box;

    struct {
        bool gi_on;
    } renderer;

    bool open_save_as_popup;
    vec_golf_entity_t copied_entities;
    vec2 viewport_pos, viewport_size;
} golf_editor_t;

golf_editor_t *golf_editor_get(void);
void golf_editor_init(void);
void golf_editor_update(float dt);
bool golf_editor_edit_entities_compare(golf_edit_mode_entity_t entity0, golf_edit_mode_entity_t entity1);
bool golf_editor_is_edit_entity_hovered(golf_edit_mode_entity_t entity);
bool golf_editor_is_edit_entity_selected(golf_edit_mode_entity_t entity);
void golf_editor_edit_mode_geo_add_point(golf_geo_point_t point);
void golf_editor_edit_mode_geo_add_face(golf_geo_face_t face);

#endif
