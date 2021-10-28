#include "golf/inputs.h"

#include <string.h>

#include "golf/maths.h"
#include "golf/renderer.h"

static golf_inputs_t inputs;

static void _get_world_ray_from_window_pos(vec2 mouse_pos, vec3 *world_ro, vec3 *world_rd) {
    golf_renderer_t *renderer = golf_renderer_get();
    mat4 inv_proj = mat4_inverse(renderer->proj_mat);
    mat4 inv_view = mat4_inverse(renderer->view_mat);
    float x = -1.0f + 2.0f * mouse_pos.x / renderer->viewport_size.x;
    float y = -1.0f + 2.0f * mouse_pos.y / renderer->viewport_size.y;
    vec4 clip_space = V4(x, y, -1.0f, 1.0f);
    vec4 eye_space = vec4_apply_mat(clip_space, inv_proj);
    eye_space = V4(eye_space.x, eye_space.y, -1.0f, 0.0f);
    vec4 world_space_4 = vec4_apply_mat(eye_space, inv_view);
    vec3 world_space = V3(world_space_4.x, world_space_4.y, world_space_4.z);
    *world_ro = renderer->cam_pos;
    *world_rd = vec3_normalize(world_space);
}

golf_inputs_t *golf_inputs_get(void) {
    return &inputs;
}

void golf_inputs_init(void) {
    memset(&inputs, 0, sizeof(golf_inputs_t));
}

void golf_inputs_begin_frame(void) {
    inputs.mouse_delta = vec2_sub(inputs.mouse_pos, inputs.prev_mouse_pos);
    inputs.prev_mouse_pos = inputs.mouse_pos;
}

void golf_inputs_end_frame(void) {
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
}

void golf_inputs_handle_event(const sapp_event *event) {
    if (event->type == SAPP_EVENTTYPE_MOUSE_DOWN ||
            event->type == SAPP_EVENTTYPE_MOUSE_UP ||
            event->type == SAPP_EVENTTYPE_MOUSE_MOVE) {
    }
    if (event->type == SAPP_EVENTTYPE_MOUSE_DOWN) {
        inputs.mouse_down[event->mouse_button] = true;
    }
    else if (event->type == SAPP_EVENTTYPE_MOUSE_UP) {
        inputs.mouse_down[event->mouse_button] = false;
        inputs.mouse_clicked[event->mouse_button] = true;
    }
    //inputs.window_mouse_pos = V2(event->mouse_x, 720.0f - event->mouse_y);
    //inputs.mouse_pos = inputs.window_mouse_pos;

    golf_renderer_t *renderer = golf_renderer_get();

    inputs.mouse_pos.x = event->mouse_x - renderer->viewport_pos.x;
    inputs.mouse_pos.y = renderer->viewport_size.y - (event->mouse_y - renderer->viewport_pos.y);
    _get_world_ray_from_window_pos(inputs.mouse_pos, &inputs.mouse_ray_orig, &inputs.mouse_ray_dir);

    if (event->type == SAPP_EVENTTYPE_KEY_DOWN) {
        inputs.button_down[event->key_code] = true;
    }
    else if (event->type == SAPP_EVENTTYPE_KEY_UP) {
        inputs.button_down[event->key_code] = false;
        inputs.button_clicked[event->key_code] = true;
    }
}
