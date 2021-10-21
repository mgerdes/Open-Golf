#ifndef _GOLF_UI_H
#define _GOLF_UI_H

#include "mcore/maths.h"
#include "mcore/mstring.h"

typedef vec_t(mstring_t) vec_mstring_t;

typedef struct golf_ui_pixel_pack_square {
    mstring_t ui_pixel_pack;
    mstring_t square_name;
    vec2 pos;
    vec2 size;
    float tile_screen_size;
} golf_ui_pixel_pack_square_t;

typedef struct golf_ui_text {
    mstring_t font;
    mstring_t string;
    vec2 pos;
    float size;
    mstring_t horiz_align;
    mstring_t vert_align;
    vec4 color;
} golf_ui_text_t;

typedef enum golf_ui_button_state {
    GOLF_UI_BUTTON_UP,
    GOLF_UI_BUTTON_DOWN,
    GOLF_UI_BUTTON_CLICKED,
} golf_ui_button_state_t;

typedef struct golf_ui_button {
    golf_ui_button_state_t state;
    vec2 pos;
    vec2 size;
    vec_mstring_t up_entities;
    vec_mstring_t down_entities;
} golf_ui_button_t;

typedef struct golf_ui_scroll_list {
    vec2 pos;
    vec2 size;
    vec_mstring_t entities;
} golf_ui_scroll_list_t;

typedef enum golf_ui_entity_type {
    GOLF_UI_PIXEL_PACK_SQUARE,
    GOLF_UI_TEXT,
    GOLF_UI_BUTTON,
    GOLF_UI_SCROLL_LIST,
} golf_ui_entity_type_t;

typedef struct golf_ui_entity {
    golf_ui_entity_type_t type; 
    union {
        golf_ui_text_t text;
        golf_ui_pixel_pack_square_t pixel_pack_square;
        golf_ui_button_t button;
        golf_ui_scroll_list_t scroll_list;
    };
} golf_ui_entity_t;

typedef map_t(golf_ui_entity_t) map_golf_ui_entity_t;
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

    vec2 entity_pos_offset;
    vec2 mouse_input_offset;
    //golf_ui_scroll_list_state_t scroll_list_state;
    vec_golf_ui_entity_t entities;
    map_golf_ui_entity_t entities_map;
} golf_ui_t;

golf_ui_t *golf_ui_get(void);
void golf_ui_init(void);
void golf_ui_update(float dt);

#endif
