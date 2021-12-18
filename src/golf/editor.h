#ifndef _GOLF_EDITOR_H
#define _GOLF_EDITOR_H

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include "cimguizmo/cimguizmo.h"
#include "golf/file.h"
#include "golf/gi.h"
#include "golf/level.h"
#include "golf/maths.h"
#include "golf/vec.h"

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
    golf_level_t *level;

    bool started_action, has_queued_action, has_queued_commit, has_queued_decommit;
    golf_editor_action_t cur_action, queued_action;
    vec_golf_editor_action_t undo_actions, redo_actions;

    bool mouse_down, mouse_down_in_imgui;

    int hovered_idx;
    vec_int_t selected_idxs;

    bool gi_running;
    golf_gi_t gi;

    bool in_edit_mode;
    struct {
        mat4 model_mat;
        golf_geo_t *geo;

        bool is_entity_hovered;
        golf_edit_mode_entity_t hovered_entity;
        vec_golf_edit_mode_entity_t selected_entities;

        vec_vec3_t starting_positions;
        mat4 local_model_mat;
        mat4 world_model_mat;

        mat4 starting_model_mat;
        vec_int_t point_idxs;
    } edit_mode;

    struct {
        bool is_using;
        bool bounds_mode_on;
        OPERATION operation;
        MODE mode;
        bool use_snap;
        float translate_snap;
        float scale_snap;
        float rotate_snap;
        mat4 model_mat;
    } gizmo;

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
} golf_editor_t;

golf_editor_t *golf_editor_get(void);
void golf_editor_init(void);
void golf_editor_update(float dt);
bool golf_editor_edit_entities_compare(golf_edit_mode_entity_t entity0, golf_edit_mode_entity_t entity1);
bool golf_editor_is_edit_entity_hovered(golf_edit_mode_entity_t entity);
bool golf_editor_is_edit_entity_selected(golf_edit_mode_entity_t entity);

#endif
