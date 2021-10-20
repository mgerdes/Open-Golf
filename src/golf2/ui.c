#include "golf2/ui.h"

#include "golf2/config.h"
#include "golf2/inputs.h"
#include "mcore/mstring.h"

static golf_ui_t ui;

static golf_ui_button_state_t _pixel_pack_square_button(const char *ui_pixel_pack, const char *down_square_name, const char *up_square_name, vec2 pos, vec2 size, float tile_screen_size) {
    golf_ui_pixel_pack_square_button_t button;
    button.ui_pixel_pack = ui_pixel_pack;
    button.down_square_name = down_square_name;
    button.up_square_name = up_square_name;
    button.button.pos = pos;
    button.button.size = size;
    button.tile_screen_size = tile_screen_size;

    golf_ui_button_state_t state;
    vec2 mp = vec2_sub(golf_inputs_window_mouse_pos(), ui.mouse_input_offset);
    if (mp.x > pos.x - 0.5f*size.x && mp.x < pos.x + 0.5f*size.x && 
            mp.y > pos.y - 0.5f*size.y && mp.y < pos.y + 0.5f*size.y) {
        if (golf_inputs_mouse_clicked()) {
            state = GOLF_UI_BUTTON_CLICKED;
        }
        else {
            state = GOLF_UI_BUTTON_DOWN;
        }
    }
    else {
        state = GOLF_UI_BUTTON_UP;
    }
    button.button.state = state;

    golf_ui_entity_t entity;
    entity.type = GOLF_UI_PIXEL_PACK_SQUARE_BUTTON;
    entity.pixel_pack_square_button = button;
    vec_push(&ui.entities, entity);

    return state;
}

static golf_ui_button_state_t _pixel_pack_square_button_from_config(const char *name) {
    mstring_t pixel_pack;
    mstring_initf(&pixel_pack, "%s_button_pixel_pack", name);

    mstring_t square_down;
    mstring_initf(&square_down, "%s_button_pixel_pack_square_down", name);

    mstring_t square_up;
    mstring_initf(&square_up, "%s_button_pixel_pack_square_up", name);

    mstring_t button_pos;
    mstring_initf(&button_pos, "%s_button_pos", name);

    mstring_t button_size;
    mstring_initf(&button_size, "%s_button_size", name);

    mstring_t tile_screen_size;
    mstring_initf(&tile_screen_size, "%s_button_pixel_pack_tile_screen_size", name);

    golf_ui_button_state_t state = _pixel_pack_square_button(CFG_STR(pixel_pack.cstr), CFG_STR(square_down.cstr), CFG_STR(square_up.cstr), CFG_V2(button_pos.cstr), CFG_V2(button_size.cstr), CFG_NUM(tile_screen_size.cstr));

    mstring_deinit(&pixel_pack);
    mstring_deinit(&square_down);
    mstring_deinit(&square_up);
    mstring_deinit(&button_pos);
    mstring_deinit(&button_size);
    mstring_deinit(&tile_screen_size);

    return state;
}

static void _pixel_pack_square(const char *ui_pixel_pack, const char *square_name, vec2 pos, vec2 size, float tile_screen_size) {
    golf_ui_pixel_pack_square_t pixel_pack_square;
    pixel_pack_square.ui_pixel_pack = ui_pixel_pack;
    pixel_pack_square.square_name = square_name;
    pixel_pack_square.pos = pos;
    pixel_pack_square.size = size;
    pixel_pack_square.tile_screen_size = tile_screen_size;

    golf_ui_entity_t entity;
    entity.type = GOLF_UI_PIXEL_PACK_SQUARE;
    entity.pixel_pack_square = pixel_pack_square;
    vec_push(&ui.entities, entity);
}

static void _pixel_pack_square_from_config(const char *name) {
    mstring_t pos;
    mstring_initf(&pos, "%s_pixel_pack_square_pos", name);

    mstring_t size;
    mstring_initf(&size, "%s_pixel_pack_square_size", name);

    mstring_t tile_screen_size;
    mstring_initf(&tile_screen_size, "%s_pixel_pack_square_tile_screen_size", name);

    mstring_t pixel_pack;
    mstring_initf(&pixel_pack, "%s_pixel_pack_square_pixel_pack", name);

    mstring_t square_name;
    mstring_initf(&square_name, "%s_pixel_pack_square_square_name", name);

    _pixel_pack_square(CFG_STR(pixel_pack.cstr), CFG_STR(square_name.cstr), CFG_V2(pos.cstr), CFG_V2(size.cstr), CFG_NUM(tile_screen_size.cstr));

    mstring_deinit(&pos);
    mstring_deinit(&size);
    mstring_deinit(&tile_screen_size);
    mstring_deinit(&pixel_pack);
    mstring_deinit(&square_name);
}

static void _text(const char *font, const char *string, vec2 pos, float size, const char *horiz_align, const char *vert_align, vec4 color, vec2 pos_offset) {
    golf_ui_text_t text;
    text.font = font;
    text.string = string;
    text.pos = vec2_add(pos, pos_offset);
    text.size = size;
    text.horiz_align = horiz_align;
    text.vert_align = vert_align;
    text.color = color;

    golf_ui_entity_t entity;  
    entity.type = GOLF_UI_TEXT;
    entity.text = text;
    vec_push(&ui.entities, entity);
}

static void _text_from_config(const char *name, vec2 pos_offset) {
    mstring_t font;
    mstring_initf(&font, "%s_text_font", name);

    mstring_t string;
    mstring_initf(&string, "%s_text_string", name);

    mstring_t pos;
    mstring_initf(&pos, "%s_text_pos", name);

    mstring_t size;
    mstring_initf(&size, "%s_text_size", name);

    mstring_t horiz_align;
    mstring_initf(&horiz_align, "%s_text_horiz_align", name);

    mstring_t vert_align;
    mstring_initf(&vert_align, "%s_text_vert_align", name);

    mstring_t color;
    mstring_initf(&color, "%s_text_color", name);

    _text(CFG_STR(font.cstr), CFG_STR(string.cstr), CFG_V2(pos.cstr), CFG_NUM(size.cstr), CFG_STR(horiz_align.cstr), CFG_STR(vert_align.cstr), CFG_V4(color.cstr), pos_offset);

    mstring_deinit(&font);
    mstring_deinit(&string);
    mstring_deinit(&pos);
    mstring_deinit(&size);
    mstring_deinit(&horiz_align);
    mstring_deinit(&vert_align);
    mstring_deinit(&color);
}

static void _scroll_list_begin(vec2 pos, vec2 size) {
    golf_ui_scroll_list_state_t *state = &ui.scroll_list_state;
    if (golf_inputs_button_down(SAPP_KEYCODE_DOWN)) {
        state->offset -= 3.0f;
    }
    if (golf_inputs_button_down(SAPP_KEYCODE_UP)) {
        state->offset += 3.0f;
    }
    ui.mouse_input_offset = V2(pos.x - 0.5f * size.x, pos.y - 0.5f * size.y + state->offset);

    golf_ui_scroll_list_t scroll_list;
    scroll_list.pos = pos;
    scroll_list.size = size;
    scroll_list.offset = state->offset;

    golf_ui_entity_t entity;  
    entity.type = GOLF_UI_SCROLL_LIST_BEGIN;
    entity.scroll_list = scroll_list;
    vec_push(&ui.entities, entity);
}

static void _scroll_list_end(void) {
    ui.mouse_input_offset = V2(0.0f, 0.0f);;

    golf_ui_entity_t entity;  
    entity.type = GOLF_UI_SCROLL_LIST_END;
    vec_push(&ui.entities, entity);
}

static void _scroll_list_begin_from_config(const char *name) {
}

golf_ui_t *golf_ui_get(void) {
    return &ui;
}

void golf_ui_init(void) {
    memset(&ui, 0, sizeof(ui));
    ui.state = GOLF_UI_MAIN_MENU;
    vec_init(&ui.entities);
}

void golf_ui_update(float dt) {
    ui.entities.length = 0;

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
}
