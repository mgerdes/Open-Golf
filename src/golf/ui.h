#ifndef _GOLF_UI_H
#define _GOLF_UI_H

#include <stdbool.h>

#include "golf/data.h"

typedef enum golf_ui_entity_type {
    GOLF_UI_PIXEL_PACK_SQUARE,
} golf_ui_entity_type_t;

typedef struct golf_ui_pixel_pack_square {
    golf_data_pixel_pack_t *pixel_pack; 
    golf_data_pixel_pack_square_t *square;
    vec2 pos, size;
} golf_ui_pixel_pack_square_t;

typedef struct golf_ui_entity {
    golf_ui_entity_type_t type;
    union {
        golf_ui_pixel_pack_square_t pixel_pack_square;
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
    vec_golf_ui_entity_t entities;
} golf_ui_t;

golf_ui_t *golf_ui_get(void);
void golf_ui_init(void);
void golf_ui_update(float dt);

#endif
