#ifndef _GOLF_INPUTS_H
#define _GOLF_INPUTS_H

#include "3rd_party/sokol/sokol_app.h"
#include "golf/maths.h"

void golf_inputs_init(void);
void golf_inputs_update(void);
void golf_inputs_handle_event(const sapp_event *event);

vec2 golf_inputs_window_mouse_pos(void);
bool golf_inputs_button_down(sapp_keycode keycode);
bool golf_inputs_button_clicked(sapp_keycode keycode);
bool golf_inputs_mouse_down(void);
bool golf_inputs_mouse_clicked(void);

#endif
