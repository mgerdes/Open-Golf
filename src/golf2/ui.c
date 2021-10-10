#include "golf2/ui.h"

#include "3rd_party/map/map.h"
#include "3rd_party/parson/parson.h"
#include "3rd_party/vec/vec.h"
#include "mcore/maths.h"
#include "mcore/mimport.h"
#include "mcore/mlog.h"
#include "golf2/renderer.h"

static golf_ui_t ui;

golf_ui_entity_t golf_ui_button_entity(const char *name, const char *text, vec2 pos, vec2 size) {
    golf_ui_entity_t entity;
    entity.type = GOLF_UI_ENTITY_BUTTON;
    entity.button.name = name;
    entity.button.text = text;
    entity.button.pos = pos;
    entity.button.size = size;
    entity.button.is_clicked = false;
    entity.button.is_hovered = false;
    return entity;
}

golf_ui_entity_t golf_ui_sprite_entity(const char *name, vec2 pos, vec2 size) {
    golf_ui_entity_t entity;
    entity.type = GOLF_UI_ENTITY_SPRITE;
    entity.sprite.name = name;
    entity.sprite.pos = pos;
    entity.sprite.size = size;
    return entity;
}

golf_ui_entity_t golf_ui_sprite_atlas_entity(const char *name, vec2 pos, vec2 size, const char *texture,
        float tile_screen_size, float tile_size, float tile_padding,
        vec2 tile_top, vec2 tile_mid, vec2 tile_bot) {
    golf_ui_entity_t entity;
    entity.type = GOLF_UI_ENTITY_SPRITE_ATLAS;
    entity.sprite_atlas.name = name;
    entity.sprite_atlas.pos = pos;
    entity.sprite_atlas.size = size;
    entity.sprite_atlas.texture = texture;
    entity.sprite_atlas.tile_screen_size = tile_screen_size;
    entity.sprite_atlas.tile_size = tile_size;
    entity.sprite_atlas.tile_padding = tile_padding;
    entity.sprite_atlas.tile_top = tile_top;
    entity.sprite_atlas.tile_mid = tile_mid;
    entity.sprite_atlas.tile_bot = tile_bot;
    return entity;
}

golf_ui_entity_t golf_ui_text_entity(const char *text, const char *font, vec2 pos, float size) {
    golf_ui_entity_t entity;
    entity.type = GOLF_UI_ENTITY_TEXT;
    entity.text.text = text;
    entity.text.font = font;
    entity.text.pos = pos;
    entity.text.size = size;
    return entity;
}

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

    golf_ui_menu_t menu;
    vec_init(&menu.entity_vec);

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

                golf_ui_entity_t entity = golf_ui_button_entity(name, text, pos, size);
                vec_push(&menu.entity_vec, entity);
            }
            else if (strcmp(type, "sprite") == 0) {
                const char *name = json_object_get_string(object, "name");
                vec2 pos = _parse_json_array_vec2(json_object_get_array(object, "pos"));
                vec2 size = _parse_json_array_vec2(json_object_get_array(object, "size"));

                golf_ui_entity_t entity = golf_ui_sprite_entity(name, pos, size);
                vec_push(&menu.entity_vec, entity);
            }
            else if (strcmp(type, "sprite_atlas") == 0) {
                const char *name = json_object_get_string(object, "name");
                vec2 pos = _parse_json_array_vec2(json_object_get_array(object, "pos"));
                vec2 size = _parse_json_array_vec2(json_object_get_array(object, "size"));
                const char *texture = json_object_get_string(object, "texture");
                float tile_screen_size = (float)json_object_get_number(object, "tile_screen_size");
                float tile_size = (float)json_object_get_number(object, "tile_size");
                float tile_padding = (float)json_object_get_number(object, "tile_padding");
                vec2 tile_top = _parse_json_array_vec2(json_object_get_array(object, "tile_top"));
                vec2 tile_mid = _parse_json_array_vec2(json_object_get_array(object, "tile_mid"));
                vec2 tile_bot = _parse_json_array_vec2(json_object_get_array(object, "tile_bot"));

                golf_ui_entity_t entity = golf_ui_sprite_atlas_entity(name, pos, size, texture, 
                        tile_screen_size, tile_size, tile_padding, 
                        tile_top, tile_mid, tile_bot); 
                vec_push(&menu.entity_vec, entity);
            }
            else if (strcmp(type, "text") == 0) {
                const char *text = json_object_get_string(object, "text");
                const char *font = json_object_get_string(object, "font");
                vec2 pos = _parse_json_array_vec2(json_object_get_array(object, "pos"));
                float size = (float)json_object_get_number(object, "size");

                golf_ui_entity_t entity = golf_ui_text_entity(text, font, pos, size);
                vec_push(&menu.entity_vec, entity);
            }
            else {
                mlog_warning("Invalid ui entity type %s", type);
            }
        }
    }
    //json_value_free(val);

    map_set(&ui.ui_menu_map, mdatafile_get_name(file), menu);
}

golf_ui_t *golf_ui_get(void) {
    return &ui;
}

void golf_ui_init(void) {
    vec_init(&ui.ui_menu_vec);
    map_init(&ui.ui_menu_map);
    mimport_add_importer(".ui_menu", _ui_menu_import, NULL);
}

void golf_ui_update(float dt) {
}
