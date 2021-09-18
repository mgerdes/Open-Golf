#ifndef _CONTROLS_H
#define _CONTROLS_H

#include <stdbool.h>

#include "3rd_party/sokol/sokol_app.h"
#include "golf/maths.h"

struct button_inputs {
    bool button_down[SAPP_MAX_KEYCODES];
    bool button_clicked[SAPP_MAX_KEYCODES];
    bool mouse_down[SAPP_MAX_MOUSEBUTTONS];
    bool mouse_clicked[SAPP_MAX_MOUSEBUTTONS];

    vec2 window_mouse_pos;
    vec2 mouse_pos, prev_mouse_pos, mouse_delta, 
         mouse_down_pos, mouse_down_delta;
    vec3 mouse_ray_orig, mouse_ray_dir,
         mouse_down_ray_orig, mouse_down_ray_dir;
};

#endif
