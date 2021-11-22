#ifndef _GOLF_EDITOR_H
#define _GOLF_EDITOR_H

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include "cimguizmo/cimguizmo.h"
#include "vec/vec.h"
#include "golf/file.h"
#include "golf/level.h"
#include "golf/lightmap.h"
#include "golf/maths.h"

typedef struct golf_editor_action_data {
    int size;
    char *ptr;
    char *copy;
} golf_editor_action_data_t;
typedef vec_t(golf_editor_action_data_t) vec_golf_editor_action_data_t;

typedef struct golf_editor_action {
    vec_golf_editor_action_data_t datas;
} golf_editor_action_t;
typedef vec_t(golf_editor_action_t) vec_golf_editor_action_t;

typedef struct golf_editor {
    golf_level_t *level;

    bool started_action, has_queued_action, has_queued_commit, has_queued_decommit;
    golf_editor_action_t cur_action, queued_action;
    vec_golf_editor_action_t actions;

    int hovered_idx;
    vec_int_t selected_idxs;

    bool lightmap_generator_running;
    golf_lightmap_generator_t lightmap_generator;

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
    } gi;
} golf_editor_t;

golf_editor_t *golf_editor_get(void);
void golf_editor_init(void);
void golf_editor_update(float dt);

#endif
