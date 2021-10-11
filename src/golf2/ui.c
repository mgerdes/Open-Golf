#include "golf2/ui.h"

#include "3rd_party/map/map.h"
#include "3rd_party/parson/parson.h"
#include "3rd_party/vec/vec.h"
#include "mcore/maths.h"
#include "mcore/mimport.h"
#include "mcore/mlog.h"
#include "golf2/inputs.h"
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

golf_ui_entity_t golf_ui_text_entity(const char *text, const char *font, vec2 pos, float size, const char *horizontal_alignment, const char *vertical_alignment, vec4 color) {
    golf_ui_entity_t entity;
    entity.type = GOLF_UI_ENTITY_TEXT;
    entity.text.text = text;
    entity.text.font = font;
    entity.text.pos = pos;
    entity.text.size = size;
    entity.text.horizontal_alignment = horizontal_alignment;
    entity.text.vertical_alignment = vertical_alignment;
    entity.text.color = color;
    return entity;
}

golf_ui_entity_t golf_ui_button_sprite_atlas_entity(const char *name, vec2 pos, vec2 size, vec2 down_text_delta, golf_ui_entity_t text, golf_ui_entity_t up_sprite_atlas, golf_ui_entity_t down_sprite_atlas) {
    golf_ui_entity_t entity;
    entity.type = GOLF_UI_ENTITY_BUTTON_SPRITE_ATLAS;
    entity.button_sprite_atlas.name = name;
    entity.button_sprite_atlas.pos = pos;
    entity.button_sprite_atlas.size = size;
    entity.button_sprite_atlas.down_text_delta = down_text_delta;
    entity.button_sprite_atlas.text = text.text;
    entity.button_sprite_atlas.up_sprite_atlas = up_sprite_atlas.sprite_atlas;
    entity.button_sprite_atlas.down_sprite_atlas = down_sprite_atlas.sprite_atlas;
    return entity;
}

vec2 _parse_json_array_vec2(JSON_Array *array) {
    return V2((float)json_array_get_number(array, 0),
            (float)json_array_get_number(array, 1));
}

vec3 _parse_json_array_vec3(JSON_Array *array) {
    return V3((float)json_array_get_number(array, 0),
            (float)json_array_get_number(array, 1),
            (float)json_array_get_number(array, 2));
}

vec4 _parse_json_array_vec4(JSON_Array *array) {
    return V4((float)json_array_get_number(array, 0),
            (float)json_array_get_number(array, 1),
            (float)json_array_get_number(array, 2),
            (float)json_array_get_number(array, 3));
}

void _ui_menu_import(mdatafile_t *file, void *udata) {
    const char *name = mdatafile_get_name(file);

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
            else if (strcmp(type, "button_sprite_atlas") == 0) {
                const char *name = json_object_get_string(object, "name");
                vec2 pos = _parse_json_array_vec2(json_object_get_array(object, "pos"));
                vec2 size = _parse_json_array_vec2(json_object_get_array(object, "size"));
                const char *text = json_object_get_string(object, "text");
                float text_size = (float)json_object_get_number(object, "text_size");
                vec4 text_color = _parse_json_array_vec4(json_object_get_array(object, "text_color"));
                const char *font = json_object_get_string(object, "font");
                vec2 down_text_delta = _parse_json_array_vec2(json_object_get_array(object, "down_text_delta"));
                const char *texture = json_object_get_string(object, "texture");
                float tile_screen_size = (float)json_object_get_number(object, "tile_screen_size");
                float tile_size = (float)json_object_get_number(object, "tile_size");
                float tile_padding = (float)json_object_get_number(object, "tile_padding");
                vec2 up_tile_top = _parse_json_array_vec2(json_object_get_array(object, "up_tile_top"));
                vec2 up_tile_mid = _parse_json_array_vec2(json_object_get_array(object, "up_tile_mid"));
                vec2 up_tile_bot = _parse_json_array_vec2(json_object_get_array(object, "up_tile_bot"));
                vec2 down_tile_top = _parse_json_array_vec2(json_object_get_array(object, "down_tile_top"));
                vec2 down_tile_mid = _parse_json_array_vec2(json_object_get_array(object, "down_tile_mid"));
                vec2 down_tile_bot = _parse_json_array_vec2(json_object_get_array(object, "down_tile_bot"));

                golf_ui_entity_t text_entity = golf_ui_text_entity(text, font, pos, text_size, "center", "center", text_color);
                golf_ui_entity_t up_sprite_atlas_entity = golf_ui_sprite_atlas_entity(name, pos, size, texture, tile_screen_size, tile_size, tile_padding, up_tile_top, up_tile_mid, up_tile_bot);
                golf_ui_entity_t down_sprite_atlas_entity = golf_ui_sprite_atlas_entity(name, pos, size, texture, tile_screen_size, tile_size, tile_padding, down_tile_top, down_tile_mid, down_tile_bot);

                golf_ui_entity_t entity = golf_ui_button_sprite_atlas_entity(name, pos, size, down_text_delta, text_entity, up_sprite_atlas_entity, down_sprite_atlas_entity);
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
                const char *horizontal_alignment = json_object_get_string(object, "horizontal_alignment");
                const char *vertical_alignment = json_object_get_string(object, "vertical_alignment");
                vec4 color = _parse_json_array_vec4(json_object_get_array(object, "color"));

                golf_ui_entity_t entity = golf_ui_text_entity(text, font, pos, size, horizontal_alignment, vertical_alignment, color);
                vec_push(&menu.entity_vec, entity);
            }
            else {
                mlog_warning("Invalid ui entity type %s", type);
            }
        }
    }

    if (!val) {
        mlog_warning("Unable to parse .ui_menu json in %s", name);
    }

    menu.json_val = val;

    golf_ui_menu_t *existing_menu = map_get(&ui.ui_menu_map, name);
    if (existing_menu) {
        if (existing_menu->json_val) {
            json_value_free(existing_menu->json_val);
        }
        vec_deinit(&existing_menu->entity_vec);
    }

    map_set(&ui.ui_menu_map, name, menu);
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
    golf_ui_menu_t *main_menu = map_get(&ui.ui_menu_map, "data/ui/main_menu.ui_menu");
    if (!main_menu) {
        mlog_error("Could not find main_menu ui_menu");
    }

    for (int i = 0; i < main_menu->entity_vec.length; i++) {
        golf_ui_entity_t *entity = &main_menu->entity_vec.data[i];

        switch (entity->type) {
            case GOLF_UI_ENTITY_SPRITE:
            case GOLF_UI_ENTITY_SPRITE_ATLAS:
            case GOLF_UI_ENTITY_TEXT:
            case GOLF_UI_ENTITY_BUTTON:
                continue;
                break;
            case GOLF_UI_ENTITY_BUTTON_SPRITE_ATLAS:
                {
                    vec2 mp = golf_inputs_window_mouse_pos();
                    vec2 bp = entity->button_sprite_atlas.pos;
                    vec2 bs = entity->button_sprite_atlas.size;
                    float bx0 = bp.x - 0.5f * bs.x;
                    float bx1 = bp.x + 0.5f * bs.x;
                    float by0 = bp.y - 0.5f * bs.y;
                    float by1 = bp.y + 0.5f * bs.y;

                    vec2 tp = entity->button_sprite_atlas.text.pos;
                    vec2 tdelta = entity->button_sprite_atlas.down_text_delta;

                    if (mp.x >= bx0 && mp.x <= bx1 && mp.y >= by0 && mp.y <= by1) {
                        entity->button_sprite_atlas.is_hovered = true;
                        entity->button_sprite_atlas.text.pos = vec2_add(bp, tdelta);
                    }
                    else {
                        entity->button_sprite_atlas.is_hovered = false;
                        entity->button_sprite_atlas.text.pos = bp;
                    }
                }
                break;
        }
    }
}

bool golf_ui_get_button_state(const char *name, bool *is_down, bool *is_clicked) {
}
