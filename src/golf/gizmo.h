#ifndef _GOLF_GIZMO_H
#define _GOLF_GIZMO_H

#include "golf/level.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"

typedef enum golf_gizmo_operation {
    GOLF_GIZMO_TRANSLATE,
    GOLF_GIZMO_ROTATE,
    GOLF_GIZMO_SCALE,
} golf_gizmo_operation;

typedef enum golf_gizmo_mode {
    GOLF_GIZMO_LOCAL,
    GOLF_GIZMO_WORLD,
} golf_gizmo_mode;

typedef struct golf_gizmo {
    bool is_on, is_active, is_hovered;
    golf_gizmo_operation operation;
    golf_gizmo_mode mode;
    golf_transform_t transform, delta_transform;

    struct {
        float snap;
        vec3 start_position;
        int axis;
        vec3 start_point;
    } translate;

    struct {
        float snap;
        quat start_rotation;
        int axis;
        vec3 axis_rotation;
        vec3 start_point; 
    } rotate;

    struct {
        float snap;
        vec3 start_scale;
        int axis;
        vec3 start_point;
    } scale;
} golf_gizmo_t;

void golf_gizmo_init(golf_gizmo_t *gizmo);
void golf_gizmo_update(golf_gizmo_t *gizmo, ImDrawList *draw_list);
void golf_gizmo_set_operation(golf_gizmo_t *gizmo, golf_gizmo_operation operation);
void golf_gizmo_set_mode(golf_gizmo_t *gizmo, golf_gizmo_mode mode);
void golf_gizmo_set_transform(golf_gizmo_t *gizmo, golf_transform_t transform);

#endif
