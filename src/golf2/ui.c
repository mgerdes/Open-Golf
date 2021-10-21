#include "golf2/ui.h"

#include <assert.h>
#include "3rd_party/parson/parson.h"
#include "golf2/config.h"
#include "golf2/inputs.h"
#include "mcore/mlog.h"
#include "mcore/mparson.h"
#include "mcore/mstring.h"

static golf_ui_t ui;

static bool _load_ui(const char *path, mdata_t data) {
    assert(data.type == MDATA_UI);

    JSON_Value *val = data.ui->json_val;
    JSON_Object *obj = json_value_get_object(val); 
    if (!obj) {
        mlog_warning("Unable to parse json for file %s", path);
        return NULL;
    }

    JSON_Array *entities_array = json_object_get_array(obj, "entities");
    for (int i = 0; i < json_array_get_count(entities_array); i++) {
        JSON_Object *entity_obj = json_array_get_object(entities_array, i);

        const char *type = json_object_get_string(entity_obj, "type");
        if (!type) {
            mlog_warning("No type on entity in UI file");
            continue;
        }

        const char *name = json_object_get_string(entity_obj, "name");
        if (!name) {
            mlog_warning("No name on entity in UI file");
            continue;
        }

        if (strcmp(type, "pixel_pack_square") == 0) {
            const char *pixel_pack = json_object_get_string(entity_obj, "pixel_pack");
            const char *square_name = json_object_get_string(entity_obj, "square_name");
            vec2 pos = json_object_get_vec2(entity_obj, "pos");
            vec2 size = json_object_get_vec2(entity_obj, "size");
            float tile_screen_size = json_object_get_number(entity_obj, "tile_screen_size");

            golf_ui_pixel_pack_square_t pixel_pack_square;
            mstring_init(&pixel_pack_square.ui_pixel_pack, pixel_pack);
            mstring_init(&pixel_pack_square.square_name, square_name);
            pixel_pack_square.pos = pos;
            pixel_pack_square.size = size;
            pixel_pack_square.tile_screen_size = tile_screen_size;

            golf_ui_entity_t entity;
            entity.type = GOLF_UI_PIXEL_PACK_SQUARE;
            entity.pixel_pack_square = pixel_pack_square;
            map_set(&ui.entities_map, name, entity); 
        }
        else if (strcmp(type, "text") == 0) {
            const char *string = json_object_get_string(entity_obj, "string");
            const char *font = json_object_get_string(entity_obj, "font");
            vec2 pos = json_object_get_vec2(entity_obj, "pos");
            float size = json_object_get_number(entity_obj, "size");
            vec4 color = json_object_get_vec4(entity_obj, "color");
            const char *horiz_align = json_object_get_string(entity_obj, "horiz_align");
            const char *vert_align = json_object_get_string(entity_obj, "vert_align");

            golf_ui_text_t text;
            mstring_init(&text.font, font);
            mstring_init(&text.string, string);
            text.pos = pos;
            text.size = size;
            text.color = color;
            mstring_init(&text.horiz_align, horiz_align);
            mstring_init(&text.vert_align, vert_align);

            golf_ui_entity_t entity;
            entity.type = GOLF_UI_TEXT;
            entity.text = text;
            map_set(&ui.entities_map, name, entity); 
        }
        else if (strcmp(type, "button") == 0) {
            golf_ui_button_t button;
            button.pos = json_object_get_vec2(entity_obj, "pos");
            button.size = json_object_get_vec2(entity_obj, "size");

            vec_init(&button.up_entities);
            JSON_Array *up_entities = json_object_get_array(entity_obj, "up_entities");
            for (int i = 0; i < json_array_get_count(up_entities); i++) {
                mstring_t entity_name;
                mstring_init(&entity_name, json_array_get_string(up_entities, i));
                vec_push(&button.up_entities, entity_name);
            }

            vec_init(&button.down_entities);
            JSON_Array *down_entities = json_object_get_array(entity_obj, "down_entities");
            for (int i = 0; i < json_array_get_count(down_entities); i++) {
                mstring_t entity_name;
                mstring_init(&entity_name, json_array_get_string(down_entities, i));
                vec_push(&button.down_entities, entity_name);
            }

            golf_ui_entity_t entity;
            entity.type = GOLF_UI_BUTTON;
            entity.button = button;
            map_set(&ui.entities_map, name, entity); 
        }
        else if (strcmp(type, "scroll_list") == 0) {
            golf_ui_scroll_list_t scroll_list;
            scroll_list.pos = json_object_get_vec2(entity_obj, "pos");
            scroll_list.size = json_object_get_vec2(entity_obj, "size");
            vec_init(&scroll_list.entities);

            golf_ui_entity_t entity;
            entity.type = GOLF_UI_SCROLL_LIST;
            entity.scroll_list = scroll_list;
            map_set(&ui.entities_map, name, entity);
        }
        else {
            mlog_warning("Unknown type in UI file %s", type);
            continue;
        }
    }
}

static bool _unload_ui(const char *path, mdata_t data) {
    assert(data.type == MDATA_UI);
}

static bool _reload_ui(const char *path, mdata_t data) {
    assert(data.type == MDATA_UI);

    map_deinit(&ui.entities_map);
    map_init(&ui.entities_map);
    _load_ui(path, data);
}

static bool _ui_get_entity(golf_ui_entity_t *out_entity, const char *name, golf_ui_entity_type_t type) {
    golf_ui_entity_t *entity = map_get(&ui.entities_map, name);
    if (entity && entity->type == type) {
        *out_entity = *entity;
        return true;
    }
    else {
        return false;
    }
}

static void _ui_entity(const char *name);

static void _ui_pixel_pack_square(const char *name) {
    golf_ui_entity_t entity;
    if (!_ui_get_entity(&entity, name, GOLF_UI_PIXEL_PACK_SQUARE)) {
        mlog_warning("Could not find pixel pack square entity %s", name);
        return;
    }

    entity.pixel_pack_square.pos = vec2_add(ui.entity_pos_offset, entity.pixel_pack_square.pos);
    vec_push(&ui.entities, entity);
}

static void _ui_text(const char *name) {
    golf_ui_entity_t entity;
    if (!_ui_get_entity(&entity, name, GOLF_UI_TEXT)) {
        mlog_warning("Could not find text entity %s", name);
        return;
    }

    entity.text.pos = vec2_add(ui.entity_pos_offset, entity.text.pos);
    vec_push(&ui.entities, entity);
}

static bool _ui_button(const char *name) {
    golf_ui_entity_t entity;
    if (!_ui_get_entity(&entity, name, GOLF_UI_BUTTON)) {
        mlog_warning("Could not find button entity %s", name);
        return false;
    }

    vec2 p = entity.button.pos;
    vec2 s = entity.button.size;
    vec2 mp = golf_inputs_window_mouse_pos();
    bool mouse_in_button = false;
    if (mp.x > p.x - 0.5f * s.x && mp.x < p.x + 0.5f * s.x &&
            mp.y > p.y - 0.5f * s.y && mp.y < p.y + 0.5f * s.y) {
        mouse_in_button = true;
    }

    entity.button.pos = vec2_add(ui.entity_pos_offset, entity.button.pos);
    vec_push(&ui.entities, entity);

    ui.entity_pos_offset = entity.button.pos;
    if (mouse_in_button) {
        for (int i = 0; i < entity.button.down_entities.length; i++) {
            _ui_entity(entity.button.down_entities.data[i].cstr);
        }
    }
    else {
        for (int i = 0; i < entity.button.up_entities.length; i++) {
            _ui_entity(entity.button.up_entities.data[i].cstr);
        }
    }
    ui.entity_pos_offset = V2(0.0f, 0.0f);

    return mouse_in_button && golf_inputs_mouse_clicked();
}

static void _ui_scroll_list_begin(const char *name) {
    golf_ui_entity_t entity;
    if (!_ui_get_entity(&entity, name, GOLF_UI_SCROLL_LIST)) {
        mlog_warning("Could not find scroll list entity %s", name);
        return;
    }
}

static void _ui_scroll_list_end() {

}

static void _ui_entity(const char *name) {
    golf_ui_entity_t *entity = map_get(&ui.entities_map, name);
    if (!entity) {
        mlog_warning("Could not find entity %s", name);
        return;
    }

    switch (entity->type) {
        case GOLF_UI_PIXEL_PACK_SQUARE:
            _ui_pixel_pack_square(name);
            break;
        case GOLF_UI_TEXT:
            _ui_text(name);
            break;
        case GOLF_UI_BUTTON:
            _ui_button(name);
            break;
    }
}

golf_ui_t *golf_ui_get(void) {
    return &ui;
}

void golf_ui_init(void) {
    memset(&ui, 0, sizeof(ui));
    ui.state = GOLF_UI_MAIN_MENU;
    map_init(&ui.entities_map);
    vec_init(&ui.entities);

    mdata_add_loader(MDATA_UI, _load_ui, _unload_ui, _reload_ui);
}

void golf_ui_update(float dt) {
    ui.entities.length = 0;

    if (ui.state == GOLF_UI_MAIN_MENU) {
        _ui_pixel_pack_square("background");
        _ui_text("main_text");
        _ui_button("play_button");
        if (_ui_button("levels_button")) {
            ui.main_menu.is_level_select_open = true;
        }

        if (ui.main_menu.is_level_select_open) {
            _ui_pixel_pack_square("levels_background");
            _ui_scroll_list_begin("levels_scroll_list");
            _ui_scroll_list_end();
        }
    }

    /*
    golf_config_push_cur_file("ui/main_menu.cfg");

    if (ui.state == GOLF_UI_MAIN_MENU) {
        _pixel_pack_square_from_config("background");
        _text_from_config("main", V2(0.0f, 0.0f));

        {
            golf_ui_button_state_t btn_state = _pixel_pack_square_button_from_config("play");
            vec2 text_offset = V2(0.0f, 0.0f);
            if (btn_state == GOLF_UI_BUTTON_DOWN) {
                text_offset = V2(0.0f, -5.0f);
            }
            _text_from_config("play", text_offset);

            if (btn_state == GOLF_UI_BUTTON_CLICKED) {
            }
        }

        {
            golf_ui_button_state_t btn_state = _pixel_pack_square_button_from_config("levels");
            vec2 text_offset = V2(0.0f, 0.0f);
            if (btn_state == GOLF_UI_BUTTON_DOWN) {
                text_offset = V2(0.0f, -5.0f);
            }
            _text_from_config("levels", text_offset);

            if (btn_state == GOLF_UI_BUTTON_CLICKED) {
                ui.main_menu.is_level_select_open = true;
            }
        }

        if (ui.main_menu.is_level_select_open) {
            _pixel_pack_square_from_config("level_select_background");
            _scroll_list_begin(V2(800, 400), V2(400, 300));

            {
                float padding = 16;
                vec2 size = V2(80, 80);
                vec2 pos = V2(size.x * 0.5f + padding, 300 - size.x * 0.5f - padding);

                for (int i = 0; i < 100; i++) {
                    _pixel_pack_square_button("data/textures/UIpackSheet_transparent.ui_pixel_pack", "blue_button_down", "blue_button_up", pos, size, 16);
                    _text("data/font/FantasqueSansMono-Bold.ttf", "100", pos, 32, "center", "center", V4(1, 1, 1, 1), V2(0, 0));
                    pos.x += size.x + padding;
                    if (i % 4 == 3) {
                        pos.x = size.x * 0.5f + padding;
                        pos.y -= size.x + padding;
                    }
                }
            }
            _scroll_list_end();
        }
    }

    golf_config_pop_cur_file();
    */
}
