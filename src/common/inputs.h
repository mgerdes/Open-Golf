#ifndef _GOLF_INPUTS_H
#define _GOLF_INPUTS_H

#include "sokol/sokol_app.h"
#include "common/maths.h"

typedef struct golf_inputs {
    bool touch_began, touch_down, touch_ended;
    vec2 touch_down_pos, touch_pos, prev_touch_pos;

    bool button_down[SAPP_MAX_KEYCODES];
    bool button_clicked[SAPP_MAX_KEYCODES];
    bool mouse_down[SAPP_MAX_MOUSEBUTTONS];
    bool mouse_clicked[SAPP_MAX_MOUSEBUTTONS];
    vec2 mouse_pos, prev_mouse_pos, mouse_delta, mouse_down_pos, mouse_down_delta; 
    vec2 screen_mouse_pos, screen_mouse_down_pos, screen_mouse_down_delta;
    vec3 mouse_ray_orig, mouse_ray_dir;
} golf_inputs_t;

golf_inputs_t *golf_inputs_get(void);
void golf_inputs_init(void);
void golf_inputs_begin_frame(void);
void golf_inputs_end_frame(void);
void golf_inputs_handle_event(const sapp_event *event);

#endif
