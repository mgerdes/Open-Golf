#include "golf2/ui.h"

#include "3rd_party/map/map.h"
#include "3rd_party/parson/parson.h"
#include "3rd_party/vec/vec.h"
#include "mcore/maths.h"
#include "mcore/mimport.h"
#include "mcore/mlog.h"

typedef enum _ui_entity_type {
    _UI_ENTITY_SPRITE,
    _UI_ENTITY_BUTTON,
} _ui_entity_type_t;

typedef struct _ui_button {
    const char *name;
    const char *text;
    vec2 pos;
    vec2 size;

    bool is_clicked;
    bool is_hovered;
} _ui_button_t;

typedef struct _ui_sprite {
    const char *name;
    vec2 pos;
    vec2 size;
} _ui_sprite_t;

typedef struct _ui_entity {
    _ui_entity_type_t type;
    union {
        _ui_button_t button;
        _ui_sprite_t sprite;
    };
} _ui_entity_t;

typedef vec_t(_ui_entity_t) _vec_ui_entity_t;
typedef map_t(_ui_entity_t) _map_ui_entity_t;

_ui_entity_t _ui_button_entity(const char *name, const char *text, vec2 pos, vec2 size) {
    _ui_entity_t entity;
    entity.type = _UI_ENTITY_BUTTON;
    entity.button.name = name;
    entity.button.text = text;
    entity.button.pos = pos;
    entity.button.size = size;
    entity.button.is_clicked = false;
    entity.button.is_hovered = false;
    return entity;
}

_ui_entity_t _ui_sprite_entity(const char *name, vec2 pos, vec2 size) {
    _ui_entity_t entity;
    entity.type = _UI_ENTITY_SPRITE;
    entity.sprite.name = name;
    entity.sprite.pos = pos;
    entity.sprite.size = size;
    return entity;
}

typedef struct _ui_menu {
    _vec_ui_entity_t entity_vec; 
    _map_ui_entity_t entity_map; 
} _ui_menu_t;

typedef map_t(_ui_menu_t) _map_ui_menu_t;

typedef struct _ui {
    _map_ui_menu_t ui_menu_map;
} _ui_t;

static _ui_t _ui;

vec2 _parse_json_array_vec2(JSON_Array *array) {
    return V2((float)json_array_get_number(array, 0),
            (float)json_array_get_number(array, 1));
}

void _ui_menu_import(mdatafile_t *file, void *udata) {
    unsigned char *data;
    int data_len;
    if (!mdatafile_get_data(file, "data", &data, &data_len)) {
        mlog_warning("Could not find 'data' property on ui_menu file");
        return;
    }

    _ui_menu_t menu;
    vec_init(&menu.entity_vec);
    map_init(&menu.entity_map);

    JSON_Value *val = json_parse_string(data);
    JSON_Array *array = json_value_get_array(val);
    if (array) {
        int count = (int)json_array_get_count(array);
        for (int i = 0; i < count; i++) {
            JSON_Object *object = json_array_get_object(array, i);
            const char *type = json_object_get_string(object, "type");

            if (strcmp(type, "button") == 0) {
                const char *name = json_object_get_string(object, "name");
                const char *text = json_object_get_string(object, "text");
                vec2 pos = _parse_json_array_vec2(json_object_get_array(object, "pos"));
                vec2 size = _parse_json_array_vec2(json_object_get_array(object, "size"));

                _ui_entity_t entity = _ui_button_entity(name, text, pos, size);
                vec_push(&menu.entity_vec, entity);
                map_set(&menu.entity_map, name, entity);
            }
            else if (strcmp(type, "sprite") == 0) {
                const char *name = json_object_get_string(object, "name");
                vec2 pos = _parse_json_array_vec2(json_object_get_array(object, "pos"));
                vec2 size = _parse_json_array_vec2(json_object_get_array(object, "size"));

                _ui_entity_t entity = _ui_sprite_entity(name, pos, size);
                vec_push(&menu.entity_vec, entity);
                map_set(&menu.entity_map, name, entity);
            }
            else {
                mlog_warning("Invalid ui entity type %s", type);
            }
        }
    }
    json_value_free(val);

    map_set(&_ui.ui_menu_map, mdatafile_get_name(file), menu);
}

void golf_ui_init(void) {
    map_init(&_ui.ui_menu_map);
    mimport_add_importer(".ui_menu", _ui_menu_import, NULL);
}

void golf_ui_update(float dt) {
}

void golf_ui_draw(void) {
}

