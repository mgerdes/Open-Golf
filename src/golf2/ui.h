#ifndef _GOLF_UI_H
#define _GOLF_UI_H

#include "mcore/maths.h"

typedef struct golf_ui_pixel_pack_square {
    const char *ui_pixel_pack;
    const char *square_name;
    vec2 pos;
    vec2 size;
} golf_ui_pixel_pack_square_t;

typedef struct golf_ui_text {
    const char *font;
    const char *string;
    vec2 pos;
    float size;
    vec4 color;
} golf_ui_text_t;

typedef enum golf_ui_entity_type {
    GOLF_UI_PIXEL_PACK_SQUARE,
    GOLF_UI_TEXT,
} golf_ui_entity_type_t;

typedef struct golf_ui_entity {
    golf_ui_entity_type_t type; 
    union {
        golf_ui_text_t text;
        golf_ui_pixel_pack_square_t pixel_pack_square;
    };
} golf_ui_entity_t;

typedef vec_t(golf_ui_entity_t) vec_golf_ui_entity_t;

typedef struct golf_ui {
    vec_golf_ui_entity_t entities;
} golf_ui_t;

golf_ui_t *golf_ui_get(void);
void golf_ui_init(void);
void golf_ui_update(float dt);

#endif
