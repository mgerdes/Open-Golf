#include "golf/ui.h"

#include <assert.h>
#include "3rd_party/parson/parson.h"
#include "golf/config.h"
#include "golf/data.h"
#include "golf/inputs.h"
#include "golf/log.h"

static golf_ui_t ui;

static vec2 _ui_pos(vec2 pos) {
    if (ui.has_context) {
        return vec2_add(pos, ui.context.pos);
    }
    else {
        return pos;
    }
}

static void _golf_ui_pixel_pack_square(const char *pixel_pack_name, const char *square_name, vec2 pos, vec2 size, float tile_screen_size, vec4 overlay_color) {
    golf_ui_pixel_pack_square_t pixel_pack_square;
    pixel_pack_square.pixel_pack = golf_data_get_pixel_pack(pixel_pack_name);
    pixel_pack_square.square = map_get(&pixel_pack_square.pixel_pack->squares, square_name);
    if (!pixel_pack_square.square) {
        golf_log_warning("Invalid pixel pack square %s in pixel pack %s", square_name, pixel_pack_name);
        return;
    }
    pixel_pack_square.pos = _ui_pos(pos);
    pixel_pack_square.size = size;
    pixel_pack_square.tile_screen_size = tile_screen_size;
    pixel_pack_square.overlay_color = overlay_color;

    golf_ui_entity_t entity;
    entity.type = GOLF_UI_PIXEL_PACK_SQUARE;
    entity.pixel_pack_square = pixel_pack_square;
    vec_push(&ui.entities, entity);
}

static void _golf_ui_pixel_pack_icon(const char *pixel_pack_name, const char *icon_name, vec2 pos, vec2 size, vec4 overlay_color) {
    golf_ui_pixel_pack_icon_t pixel_pack_icon;
    pixel_pack_icon.pixel_pack = golf_data_get_pixel_pack(pixel_pack_name);
    pixel_pack_icon.icon = map_get(&pixel_pack_icon.pixel_pack->icons, icon_name);
    if (!pixel_pack_icon.icon) {
        golf_log_warning("Invalid pixel pack icon %s in pixel pack %s", icon_name, pixel_pack_name);
        return;
    }
    pixel_pack_icon.pos = _ui_pos(pos);
    pixel_pack_icon.size = size;
    pixel_pack_icon.overlay_color = overlay_color;

    golf_ui_entity_t entity;
    entity.type = GOLF_UI_PIXEL_PACK_ICON;
    entity.pixel_pack_icon = pixel_pack_icon;
    vec_push(&ui.entities, entity);
}

static void _golf_ui_text(const char *font, vec2 pos, float size, int horiz_align, int vert_align, vec4 color, const char *format, ...) {
    golf_ui_text_t text;
    text.font = golf_data_get_font(font);
    text.pos = _ui_pos(pos);
    text.size = size;
    text.horiz_align = horiz_align;
    text.vert_align = vert_align;
    text.color = color;

    va_list args; 
    va_start(args, format); 
    int len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    text.string = malloc(len + 1);
    vsnprintf(text.string, len + 1, format, args);

    golf_ui_entity_t entity;
    entity.type = GOLF_UI_TEXT;
    entity.text = text;
    vec_push(&ui.entities, entity);
}

static golf_ui_button_state_t _golf_ui_button(vec2 pos, vec2 size) {
    pos = _ui_pos(pos);

    vec2 mp = golf_inputs_window_mouse_pos();
    if (mp.x > pos.x - 0.5f * size.x && mp.x < pos.x + 0.5f * size.x &&
            mp.y > pos.y - 0.5f * size.y && mp.y < pos.y + 0.5f * size.y) {
        if (golf_inputs_mouse_clicked()) {
            return GOLF_UI_BUTTON_CLICKED;
        }
        else {
            return GOLF_UI_BUTTON_DOWN;
        }
    }
    else {
        return GOLF_UI_BUTTON_UP;
    }
}

static void _golf_ui_scroll_list_begin(vec2 pos, vec2 size, float total_height, const char *bar_pixel_pack_name, float bar_width, float bar_x,
        float inner_bar_width, float inner_bar_height, float inner_bar_pad) {
    if (ui.has_context) {
        golf_log_warning("There is already a context!"); 
        return;
    }

    {
        vec2 bar_pos = vec2_add(pos, V2(0.5f * size.x + 0.5f * bar_width + bar_x, 0));
        vec2 bar_size = V2(bar_width, size.y);
        _golf_ui_pixel_pack_square(bar_pixel_pack_name, "blue_border_background", bar_pos, bar_size, 16, V4(0, 0, 0, 0));

        float a = (ui.scroll_list_y / (total_height - size.y));
        float inner_bar_y = 0.5f * (size.y - inner_bar_pad) - 0.5f * inner_bar_height - a * (size.y - inner_bar_pad - inner_bar_height);
        vec2 bar_inner_pos = vec2_add(pos, V2(0.5f * size.x + 0.5f * bar_width + bar_x, inner_bar_y));
        vec2 bar_inner_size = V2(inner_bar_width, inner_bar_height);

        golf_ui_button_state_t button_state = _golf_ui_button(bar_inner_pos, bar_inner_size);
        if (button_state == GOLF_UI_BUTTON_UP && !ui.scroll_list_moving) {
            _golf_ui_pixel_pack_icon(bar_pixel_pack_name, "blue_tube", bar_inner_pos, bar_inner_size, V4(0, 0, 0, 0));
        }
        else {
            _golf_ui_pixel_pack_icon(bar_pixel_pack_name, "blue_tube", bar_inner_pos, bar_inner_size, V4(0, 0, 0, 0.1));
        }

        if (button_state == GOLF_UI_BUTTON_DOWN) {
            if (golf_inputs_mouse_down()) {
                ui.scroll_list_moving = true;
            }
        }

        if (ui.scroll_list_moving) {
            vec2 mp = golf_inputs_window_mouse_pos();
            float inner_bar_y0 = (bar_pos.y - 0.5f * bar_size.y + 0.5f * inner_bar_height + inner_bar_pad);
            float inner_bar_y1 = (bar_pos.y + 0.5f * bar_size.y - 0.5f * inner_bar_height - inner_bar_pad);
            float a = (mp.y - inner_bar_y1) / (inner_bar_y0 - inner_bar_y1);
            if (a < 0.0f) a = 0.0f;
            if (a > 1.0f) a = 1.0f;
            ui.scroll_list_y = (total_height - size.y) * a;

            if (!golf_inputs_mouse_down()) {
                ui.scroll_list_moving = false;
            }
        }
    }

    ui.has_context = true;
    ui.context.pos = V2(pos.x, pos.y + ui.scroll_list_y);

    if (golf_inputs_button_down(SAPP_KEYCODE_UP)) {
        ui.scroll_list_y -= 10.0f;
        if (ui.scroll_list_y < 0.0f) {
            ui.scroll_list_y = 0.0f;
        }
    }
    if (golf_inputs_button_down(SAPP_KEYCODE_DOWN)) {
        ui.scroll_list_y += 10.0f;
        if (ui.scroll_list_y > total_height - size.y) {
            ui.scroll_list_y = total_height - size.y;
        }
    }

    golf_ui_scroll_list_t scroll_list;
    scroll_list.pos = pos;
    scroll_list.size = size;

    golf_ui_entity_t entity;
    entity.type = GOLF_UI_SCROLL_LIST_BEGIN;
    entity.scroll_list = scroll_list;
    vec_push(&ui.entities, entity);
}

static void _golf_ui_scroll_list_end(void) {
    ui.has_context = false;

    golf_ui_entity_t entity;
    entity.type = GOLF_UI_SCROLL_LIST_END;
    vec_push(&ui.entities, entity);
}

golf_ui_t *golf_ui_get(void) {
    return &ui;
}

void golf_ui_init(void) {
    vec_init(&ui.entities);
    memset(&ui, 0, sizeof(ui));
    ui.state = GOLF_UI_MAIN_MENU;
}

void golf_ui_update(float dt) {
    for (int i = 0; i < ui.entities.length; i++) {
        golf_ui_entity_t entity = ui.entities.data[i];
        switch (entity.type) {
            case GOLF_UI_PIXEL_PACK_SQUARE:
                break;
            case GOLF_UI_TEXT:
                free(entity.text.string);
                break;
        }
    }
    ui.entities.length = 0;

    golf_data_config_t *cfg = golf_data_get_config("data/config/ui/main_menu.cfg");
    const char *font_name = CFG_STRING(cfg, "font_name");
    const char *pixel_pack_name = CFG_STRING(cfg, "pixel_pack_name");

    {
        vec2 bg_pos = CFG_VEC2(cfg, "background_pos");
        vec2 bg_size = CFG_VEC2(cfg, "background_size");
        _golf_ui_pixel_pack_square(pixel_pack_name, "blue_background", bg_pos, bg_size, 32, V4(0, 0, 0, 0));

        vec2 main_text_pos = CFG_VEC2(cfg, "main_text_pos");
        float main_text_size = CFG_NUM(cfg, "main_text_size");
        vec4 main_text_color = CFG_VEC4(cfg, "main_text_color");
        _golf_ui_text(font_name, main_text_pos, main_text_size, 0, 0, main_text_color, "OPEN GOLF");
    }

    {
        vec2 play_pos = CFG_VEC2(cfg, "play_button_pos");
        vec2 play_size = CFG_VEC2(cfg, "play_button_size");
        const char *play_text = "PLAY";

        vec2 levels_pos = CFG_VEC2(cfg, "levels_button_pos");
        vec2 levels_size = CFG_VEC2(cfg, "levels_button_size");
        const char *levels_text = "LEVELS";

        vec2 text_up_offset = CFG_VEC2(cfg, "button_text_up_offset");
        vec2 text_down_offset = CFG_VEC2(cfg, "button_text_down_offset");
        float text_size = CFG_NUM(cfg, "button_text_size");
        vec4 text_color = CFG_VEC4(cfg, "button_text_color");
        vec4 color_overlay = CFG_VEC4(cfg, "button_color_overlay");

        // Play button
        golf_ui_button_state_t play_button_state = _golf_ui_button(play_pos, play_size);
        if (play_button_state == GOLF_UI_BUTTON_UP) {
            _golf_ui_pixel_pack_square(pixel_pack_name, "blue_button_up", play_pos, play_size, 32, V4(0, 0, 0, 0));
            _golf_ui_text(font_name, vec2_add(play_pos, text_up_offset), text_size, 0, 0, text_color, play_text);
        }
        else {
            _golf_ui_pixel_pack_square(pixel_pack_name, "blue_button_down", play_pos, play_size, 32, color_overlay);
            _golf_ui_text(font_name, vec2_add(play_pos, text_down_offset), text_size, 0, 0, text_color, play_text);
        }

        if (play_button_state == GOLF_UI_BUTTON_CLICKED) {
        }

        // Levels button
        golf_ui_button_state_t levels_button_state = _golf_ui_button(levels_pos, levels_size);
        if (levels_button_state == GOLF_UI_BUTTON_UP) {
            _golf_ui_pixel_pack_square(pixel_pack_name, "blue_button_up", levels_pos, levels_size, 32, V4(0, 0, 0, 0));
            _golf_ui_text(font_name, vec2_add(levels_pos, text_up_offset), text_size, 0, 0, text_color, levels_text);
        }
        else {
            _golf_ui_pixel_pack_square(pixel_pack_name, "blue_button_down", levels_pos, levels_size, 32, color_overlay);
            _golf_ui_text(font_name, vec2_add(levels_pos, text_down_offset), text_size, 0, 0, text_color, levels_text);
        }

        if (levels_button_state == GOLF_UI_BUTTON_CLICKED) {
            ui.main_menu.is_level_select_open = true;
        }
    }

    if (ui.main_menu.is_level_select_open) {
        {
            vec2 bg_pos = CFG_VEC2(cfg, "levels_background_pos");
            vec2 bg_size = CFG_VEC2(cfg, "levels_background_size");
            _golf_ui_pixel_pack_square(pixel_pack_name, "blue_background", bg_pos, bg_size, 32, V4(0, 0, 0, 0));

            vec2 main_text_pos = CFG_VEC2(cfg, "levels_text_pos");
            float main_text_size = CFG_NUM(cfg, "levels_text_size");
            vec4 main_text_color = CFG_VEC4(cfg, "levels_text_color");
            _golf_ui_text(font_name, main_text_pos, main_text_size, 0, 0, main_text_color, "LEVELS");
        }

        {
            vec2 pos = CFG_VEC2(cfg, "levels_exit_pos");
            vec2 size = CFG_VEC2(cfg, "levels_exit_size");
            vec4 overlay_color = CFG_VEC4(cfg, "levels_exit_overlay_color");

            golf_ui_button_state_t button_state = _golf_ui_button(pos, size);
            if (button_state == GOLF_UI_BUTTON_UP) {
                _golf_ui_pixel_pack_icon(pixel_pack_name, "blue_checkbox", pos, size, V4(0, 0, 0, 0));
            }
            else {
                _golf_ui_pixel_pack_icon(pixel_pack_name, "blue_checkbox", pos, size, overlay_color);
            }

            if (button_state == GOLF_UI_BUTTON_CLICKED) {
                ui.main_menu.is_level_select_open = false;
            }
        }

        {
            int levels_grid_cols = CFG_NUM(cfg, "levels_grid_cols");
            vec2 sl_pos = CFG_VEC2(cfg, "levels_scroll_list_pos");
            vec2 sl_size = CFG_VEC2(cfg, "levels_scroll_list_size");
            float sl_bar_x = CFG_NUM(cfg, "levels_scroll_list_bar_x");
            float sl_bar_width = CFG_NUM(cfg, "levels_scroll_list_bar_width");
            float sl_in_bar_width = CFG_NUM(cfg, "levels_scroll_list_bar_inner_width");
            float sl_in_bar_height = CFG_NUM(cfg, "levels_scroll_list_bar_inner_height");
            float sl_in_bar_padding = CFG_NUM(cfg, "levels_scroll_list_bar_inner_padding");

            vec2 btn_size = CFG_VEC2(cfg, "levels_grid_button_size");
            float btn_pad = (sl_size.x - (levels_grid_cols * btn_size.x)) / (levels_grid_cols + 1);
            vec2 btn_pos = V2(-0.5f * sl_size.x + 0.5f * btn_size.x + btn_pad, 0.5f * sl_size.y - 0.5f * btn_size.y);

            vec2 text_down_offset = CFG_VEC2(cfg, "levels_grid_text_down_offset");
            vec2 text_up_offset = CFG_VEC2(cfg, "levels_grid_text_up_offset");
            float text_size = CFG_NUM(cfg, "levels_grid_text_size");
            vec4 text_color = CFG_VEC4(cfg, "levels_grid_text_color");
            vec4 color_overlay = CFG_VEC4(cfg, "levels_grid_color_overlay");

            float sl_total_height = (100 / levels_grid_cols) * (btn_size.y + btn_pad) - btn_pad;

            _golf_ui_scroll_list_begin(sl_pos, sl_size, sl_total_height, pixel_pack_name, sl_bar_width, sl_bar_x, 
                    sl_in_bar_width, sl_in_bar_height, sl_in_bar_padding);

            for (int i = 0; i < 100; i++) {
                golf_ui_button_state_t button_state = _golf_ui_button(btn_pos, btn_size);
                char str[16];
                snprintf(str, 16, "%d", i + 1);
                if (button_state == GOLF_UI_BUTTON_UP) {
                    _golf_ui_pixel_pack_square(pixel_pack_name, "blue_button_up", btn_pos, btn_size, 32, V4(0, 0, 0, 0));
                    _golf_ui_text(font_name, vec2_add(btn_pos, text_up_offset), text_size, 0, 0, text_color, str);
                }
                else {
                    _golf_ui_pixel_pack_square(pixel_pack_name, "blue_button_down", btn_pos, btn_size, 32, color_overlay);
                    _golf_ui_text(font_name, vec2_add(btn_pos, text_down_offset), text_size, 0, 0, text_color, str);
                }

                btn_pos.x += (btn_size.x + btn_pad);
                if (i % levels_grid_cols == levels_grid_cols - 1) {
                    btn_pos.y -= btn_size.y + btn_pad;
                    btn_pos.x = -0.5f * sl_size.x + 0.5f * btn_size.x + btn_pad;
                }
            }

            _golf_ui_scroll_list_end();
        }
    }

    if (ui.state == GOLF_UI_MAIN_MENU) {
    }
}
