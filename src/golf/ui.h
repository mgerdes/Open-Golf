#ifndef _GOLF_UI_H
#define _GOLF_UI_H

#include <stdbool.h>

#include "golf/data.h"

typedef enum golf_ui_button_state {
    GOLF_UI_BUTTON_UP, 
    GOLF_UI_BUTTON_DOWN, 
    GOLF_UI_BUTTON_CLICKED, 
} golf_ui_button_state_t;

typedef struct golf_ui_pixel_pack_square {
    golf_data_pixel_pack_t *pixel_pack; 
    golf_data_pixel_pack_square_t *square;
    vec2 pos, size;
    float tile_screen_size;
    vec4 overlay_color;
} golf_ui_pixel_pack_square_t;

typedef struct golf_ui_pixel_pack_icon {
    golf_data_pixel_pack_t *pixel_pack;
    golf_data_pixel_pack_icon_t *icon;
    vec2 pos, size;
    vec4 overlay_color;
} golf_ui_pixel_pack_icon_t;

typedef struct golf_ui_text {
    golf_data_font_t *font;
    char *string;
    vec2 pos;
    float size;
    int horiz_align, vert_align;
    vec4 color;
} golf_ui_text_t;

typedef struct golf_ui_scroll_list {
    vec2 pos, size;
} golf_ui_scroll_list_t;

typedef struct golf_ui_context {
    vec2 pos;
} golf_ui_context_t;

typedef enum golf_ui_entity_type {
    GOLF_UI_PIXEL_PACK_SQUARE,
    GOLF_UI_PIXEL_PACK_ICON,
    GOLF_UI_TEXT,
    GOLF_UI_SCROLL_LIST_BEGIN,
    GOLF_UI_SCROLL_LIST_END,
} golf_ui_entity_type_t;

typedef struct golf_ui_entity {
    golf_ui_entity_type_t type;
    union {
        golf_ui_pixel_pack_square_t pixel_pack_square;
        golf_ui_pixel_pack_icon_t pixel_pack_icon;
        golf_ui_text_t text;
        golf_ui_scroll_list_t scroll_list;
    };
} golf_ui_entity_t;
typedef vec_t(golf_ui_entity_t) vec_golf_ui_entity_t;

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

    bool has_context;
    golf_ui_context_t context;
    float scroll_list_y;
    bool scroll_list_moving;
    vec_golf_ui_entity_t entities;
} golf_ui_t;

golf_ui_t *golf_ui_get(void);
void golf_ui_init(void);
void golf_ui_update(float dt);

#endif
