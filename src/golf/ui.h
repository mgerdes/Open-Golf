#ifndef _GOLF_UI_H
#define _GOLF_UI_H

#include <stdbool.h>

#include "common/data.h"
#include "common/string.h"

typedef enum golf_ui_draw_entity_type {
    GOLF_UI_DRAW_TEXTURE,
    GOLF_UI_DRAW_APPLY_VIEWPORT,
    GOLF_UI_DRAW_UNDO_APPLY_VIEWPORT,
} golf_ui_draw_entity_type_t;

typedef struct golf_ui_draw_entity {
    golf_ui_draw_entity_type_t type;
    sg_image image;
    vec2 pos, size, uv0, uv1;
    float angle, is_font, alpha;
    vec4 overlay_color;
} golf_ui_draw_entity_t;
typedef vec_t(golf_ui_draw_entity_t) vec_golf_ui_draw_entity_t;

typedef struct golf_ui {
    union {
        struct {
            bool is_level_select_open;
        } main_menu;
    };

    struct {
        vec4 aimer_color;
        bool is_set;
        vec2 viewport_size_when_set;
        vec2 pos;
        float size;
    } aim_circle;

    struct {
        int level;
        bool active, to_main_menu, to_loading_level, to_retry;
        float t;
    } fade_out;

    struct {
        bool just_saw_tutorial_0;
    } tutorial;

    vec_golf_ui_draw_entity_t draw_entities;
} golf_ui_t;

golf_ui_t *golf_ui_get(void);
void golf_ui_init(void);
void golf_ui_update(float dt);

#endif
