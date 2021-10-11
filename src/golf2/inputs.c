#include "golf2/inputs.h"

#include <string.h>

static struct inputs {
    bool button_down[SAPP_MAX_KEYCODES];
    bool button_clicked[SAPP_MAX_KEYCODES];
    bool mouse_down[SAPP_MAX_MOUSEBUTTONS];
    bool mouse_clicked[SAPP_MAX_MOUSEBUTTONS];
    vec2 window_mouse_pos;
    vec2 mouse_pos, prev_mouse_pos, mouse_delta, 
         mouse_down_pos, mouse_down_delta;
} inputs;

void golf_inputs_init(void) {
    memset(&inputs, 0, sizeof(inputs));
}

void golf_inputs_update(void) {
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
    inputs.window_mouse_pos = V2(event->mouse_x, 720.0f - event->mouse_y);
    inputs.mouse_pos = inputs.window_mouse_pos;

    if (event->type == SAPP_EVENTTYPE_KEY_DOWN) {
        inputs.button_down[event->key_code] = true;
    }
    else if (event->type == SAPP_EVENTTYPE_KEY_UP) {
        inputs.button_down[event->key_code] = false;
        inputs.button_clicked[event->key_code] = true;
    }
}

vec2 golf_inputs_window_mouse_pos(void) {
    return inputs.window_mouse_pos;
}

bool golf_inputs_button_down(sapp_keycode keycode) {
    return inputs.button_down[keycode];
}

bool golf_inputs_button_clicked(sapp_keycode keycode) {
    return inputs.button_clicked[keycode];
}
