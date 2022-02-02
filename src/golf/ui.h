#ifndef _GOLF_UI_H
#define _GOLF_UI_H

#include <stdbool.h>

#include "common/data.h"
#include "common/string.h"

typedef struct golf_ui_draw_entity {
    sg_image image;
    vec2 pos, size, uv0, uv1;
    float is_font;
    vec4 overlay_color;
} golf_ui_draw_entity_t;
typedef vec_t(golf_ui_draw_entity_t) vec_golf_ui_draw_entity_t;

typedef enum golf_ui_state {
    GOLF_UI_MAIN_MENU,
} golf_ui_state_t;

typedef struct golf_ui {
    golf_ui_state_t state;
    union {
        struct {
            bool is_level_select_open;
        } main_menu;
    };

    float scroll_list_y;
    bool scroll_list_moving;
    vec_golf_ui_draw_entity_t draw_entities;
} golf_ui_t;

golf_ui_t *golf_ui_get(void);
void golf_ui_init(void);
void golf_ui_update(float dt);

#endif
