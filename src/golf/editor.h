#ifndef _GOLF_EDITOR_H
#define _GOLF_EDITOR_H

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "3rd_party/cimgui/cimgui.h"
#include "3rd_party/cimguizmo/cimguizmo.h"
#include "3rd_party/vec/vec.h"
#include "golf/file.h"
#include "golf/level.h"
#include "golf/maths.h"

typedef enum golf_editor_action_type {
    MODIFY_DATA_ACTION,
} golf_editor_action_type_t;

typedef struct golf_editor_action {
    golf_editor_action_type_t type;
    union {
        struct {
            int data_size;
            void *data;
            void *data_copy;
        };
    };
} golf_editor_action_t;
typedef vec_t(golf_editor_action_t) vec_golf_editor_action_t;

typedef struct golf_editor {
    vec_bool_t entity_active;
    vec_golf_entity_t entities;

    bool started_modify_data_action;
    golf_editor_action_t modify_data_action;
    vec_golf_editor_action_t actions;

    int hovered_entity_idx;
    int selected_entity_idx;
    char model_path_temp[GOLF_FILE_MAX_PATH];

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
} golf_editor_t;

golf_editor_t *golf_editor_get(void);
void golf_editor_init(void);
void golf_editor_update(float dt);

#endif
