#include "common/inputs.h"

#include <string.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"

#include "common/graphics.h"
#include "common/maths.h"

static golf_inputs_t inputs;

static void _get_world_ray_from_window_pos(vec2 mouse_pos, vec3 *world_ro, vec3 *world_rd) {
    golf_graphics_t *graphics = golf_graphics_get();
    mat4 inv_proj = mat4_inverse(graphics->proj_mat);
    mat4 inv_view = mat4_inverse(graphics->view_mat);
    float x = -1.0f + 2.0f * mouse_pos.x / graphics->viewport_size.x;
    float y = -1.0f + 2.0f * (graphics->viewport_size.y - mouse_pos.y) / graphics->viewport_size.y;
    vec4 clip_space = V4(x, y, -1.0f, 1.0f);
    vec4 eye_space = vec4_apply_mat(clip_space, inv_proj);
    eye_space = V4(eye_space.x, eye_space.y, -1.0f, 0.0f);
    vec4 world_space_4 = vec4_apply_mat(eye_space, inv_view);
    vec3 world_space = V3(world_space_4.x, world_space_4.y, world_space_4.z);
    *world_ro = graphics->cam_pos;
    *world_rd = vec3_normalize(world_space);
}

golf_inputs_t *golf_inputs_get(void) {
    return &inputs;
}

void golf_inputs_init(void) {
    memset(&inputs, 0, sizeof(inputs));
#if GOLF_PLATFORM_ANDROID | GOLF_PLATFORM_IOS
    inputs.is_touch = true;
#else
    inputs.is_touch = false;
#endif
}

void golf_inputs_begin_frame(void) {
    inputs.mouse_delta = vec2_sub(inputs.mouse_pos, inputs.prev_mouse_pos);
    if (inputs.mouse_down[SAPP_MOUSEBUTTON_LEFT]) {
        inputs.mouse_down_delta = vec2_sub(inputs.mouse_pos, inputs.mouse_down_pos);
        inputs.screen_mouse_down_delta = vec2_sub(inputs.screen_mouse_pos, inputs.screen_mouse_down_pos);
    }
}

void golf_inputs_end_frame(void) {
    inputs.frame_num++;
    if (inputs.touch_ended) {
        inputs.touch_down = false;
    }
    inputs.touch_began = false;
    inputs.touch_ended = false;
    inputs.prev_mouse_pos = inputs.mouse_pos;
    inputs.prev_touch_pos = inputs.touch_pos;
    for (int i = 0; i < SAPP_MAX_KEYCODES; i++) {
        if (inputs.button_clicked[i]) {
            inputs.button_clicked[i] = false;
        }
    }
    for (int i = 0; i < SAPP_MAX_MOUSEBUTTONS; i++) {
        if (inputs.mouse_clicked[i]) {
            inputs.mouse_clicked[i] = false;
        }
    }
    inputs.mouse_scroll_delta = V2(0, 0);
}

void golf_inputs_handle_event(const sapp_event *event) {
    golf_graphics_t *graphics = golf_graphics_get();
    inputs.screen_mouse_pos.x = event->mouse_x;
    inputs.screen_mouse_pos.y = graphics->window_size.y - event->mouse_y;
    inputs.mouse_pos.x = event->mouse_x - graphics->viewport_pos.x;
    inputs.mouse_pos.y = event->mouse_y - graphics->viewport_pos.y;
    _get_world_ray_from_window_pos(inputs.mouse_pos, &inputs.mouse_ray_orig, &inputs.mouse_ray_dir);


    if (event->type == SAPP_EVENTTYPE_TOUCHES_BEGAN) {
        if (event->num_touches > 0) {
            inputs.touch_pos = V2(event->touches[0].pos_x, event->touches[0].pos_y);
        }
        inputs.frame_touch_began = inputs.frame_num;
        inputs.touch_began = true;
        inputs.touch_down = true;
        inputs.touch_down_pos = inputs.touch_pos;
        inputs.prev_touch_pos = inputs.touch_pos;
    }
    else if (event->type == SAPP_EVENTTYPE_TOUCHES_MOVED) {
        if (event->num_touches > 0) {
            inputs.touch_pos = V2(event->touches[0].pos_x, event->touches[0].pos_y);
        }
    }
    else if (event->type == SAPP_EVENTTYPE_TOUCHES_ENDED) {
        inputs.frame_touch_ended = inputs.frame_num;
        inputs.touch_ended = true;
    }
    else if (event->type == SAPP_EVENTTYPE_TOUCHES_CANCELLED) {
        inputs.frame_touch_ended = inputs.frame_num;
        inputs.touch_ended = true;
    }

    if (event->type == SAPP_EVENTTYPE_MOUSE_DOWN ||
            event->type == SAPP_EVENTTYPE_MOUSE_UP ||
            event->type == SAPP_EVENTTYPE_MOUSE_MOVE) {
    }
    if (event->type == SAPP_EVENTTYPE_MOUSE_DOWN) {
        inputs.mouse_down_pos = inputs.mouse_pos;
        inputs.screen_mouse_down_pos = inputs.screen_mouse_pos;
        inputs.mouse_down[event->mouse_button] = true;
    }
    else if (event->type == SAPP_EVENTTYPE_MOUSE_UP) {
        inputs.mouse_down[event->mouse_button] = false;
        inputs.mouse_clicked[event->mouse_button] = true;
    }
    else if (event->type == SAPP_EVENTTYPE_MOUSE_SCROLL) {
        inputs.mouse_scroll_delta = V2(event->scroll_x, event->scroll_y);
    }

    if (event->type == SAPP_EVENTTYPE_KEY_DOWN) {
        inputs.button_down[event->key_code] = true;
    }
    else if (event->type == SAPP_EVENTTYPE_KEY_UP) {
        inputs.button_down[event->key_code] = false;
        inputs.button_clicked[event->key_code] = true;
    }
}
