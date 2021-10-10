#ifndef _GOLF_UI_H
#define _GOLF_UI_H

#include "3rd_party/map/map.h"
#include "3rd_party/vec/vec.h"
#include "mcore/maths.h"

typedef enum golf_ui_entity_type {
    GOLF_UI_ENTITY_SPRITE,
    GOLF_UI_ENTITY_SPRITE_ATLAS,
    GOLF_UI_ENTITY_BUTTON,
    GOLF_UI_ENTITY_TEXT,
} golf_ui_entity_type_t;

typedef struct golf_ui_button {
    const char *name;
    const char *text;
    vec2 pos;
    vec2 size;

    bool is_clicked;
    bool is_hovered;
} golf_ui_button_t;

typedef struct golf_ui_sprite {
    const char *name;
    vec2 pos;
    vec2 size;
} golf_ui_sprite_t;

typedef struct golf_ui_sprite_atlas {
    const char *name;
    vec2 pos;
    vec2 size;
    const char *texture;
    float tile_screen_size;
    float tile_size;
    float tile_padding;
    vec2 tile_top;
    vec2 tile_mid;
    vec2 tile_bot;
} golf_ui_sprite_atlas_t;

typedef struct golf_ui_text {
    const char *text;
    const char *font;
    vec2 pos;
    float size;
} golf_ui_text_t;

typedef struct golf_ui_entity {
    golf_ui_entity_type_t type;
    union {
        golf_ui_button_t button;
        golf_ui_sprite_t sprite;
        golf_ui_sprite_atlas_t sprite_atlas;
        golf_ui_text_t text;
    };
} golf_ui_entity_t;

typedef vec_t(golf_ui_entity_t) vec_golf_ui_entity_t;
typedef map_t(golf_ui_entity_t) map_golf_ui_entity_t;

golf_ui_entity_t golf_ui_button_entity(const char *name, const char *text, vec2 pos, vec2 size);
golf_ui_entity_t golf_ui_sprite_entity(const char *name, vec2 pos, vec2 size);
golf_ui_entity_t golf_ui_sprite_atlas_entity(const char *name, vec2 pos, vec2 size, const char *texture,
        float tile_screen_size, float tile_size, float tile_padding,
        vec2 tile_top, vec2 tile_mid, vec2 tile_bot); 
golf_ui_entity_t golf_ui_text_entity(const char *text, const char *font, vec2 pos, float size);

typedef struct golf_ui_menu {
    vec_golf_ui_entity_t entity_vec; 
} golf_ui_menu_t;

typedef vec_t(golf_ui_menu_t) vec_golf_ui_menu_t;
typedef map_t(golf_ui_menu_t) map_golf_ui_menu_t;

typedef struct golf_ui {
    vec_golf_ui_menu_t ui_menu_vec;
    map_golf_ui_menu_t ui_menu_map;
} golf_ui_t;

golf_ui_t *golf_ui_get(void);
void golf_ui_init(void);
void golf_ui_update(float dt);
void golf_ui_draw(void);

#endif
