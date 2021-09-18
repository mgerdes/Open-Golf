#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "3rd_party/cimgui/cimgui.h"

#include "golf/ui.h"
#include "golf/audio.h"
#include "golf/config.h"

static bool point_in_button(vec2 pt, struct ui_button *button) {
    vec2 p = button->pos;
    vec2 s = button->size;
    return (pt.x >= p.x - 0.5f*s.x) && (pt.x <= p.x + 0.5f*s.x) && 
        (pt.y >= p.y - 0.5f*s.y) && (pt.y <= p.y + 0.5f*s.y);
}

void ui_button_init(struct ui_button *button, vec2 pos, vec2 size) {
    button->pos = pos;
    button->size = size;
    button->is_clicked = false;
    button->is_hovered = false;
}

void ui_button_update(struct ui_button *button, struct button_inputs inputs) {
    button->is_hovered = point_in_button(inputs.mouse_pos, button);
    button->is_clicked = button->is_hovered && inputs.mouse_clicked[SAPP_MOUSEBUTTON_LEFT];
    if (button->is_clicked) {
        audio_start_sound("button_click", "drop_003.ogg", 1.0f, false, true);
    }
}

void main_menu_init(struct main_menu *main_menu) {
    {
        vec2 pos = config_get_vec2("main_menu_start_game_button_pos");
        vec2 size = config_get_vec2("main_menu_start_game_button_size");
        ui_button_init(&main_menu->start_game_button, pos, size);
    }
}

void main_menu_update(struct main_menu *main_menu, float dt, struct button_inputs inputs) {
    main_menu->start_game_button.pos = config_get_vec2("main_menu_start_game_button_pos");
    main_menu->start_game_button.size = config_get_vec2("main_menu_start_game_button_size");

    ui_button_update(&main_menu->start_game_button, inputs);
}

void scoreboard_init(struct scoreboard *scoreboard) {
    for (int i = 0; i < 18; i++) {
        scoreboard->hole_score[i] = -1;
        scoreboard->hole_par[i] = 0;
    }

    int num_holes = config_get_int("game_num_holes");
    for (int i = 0; i < num_holes; i++) {
        char key[256];
        snprintf(key, 255, "game_hole_%d_par", i + 1);
        key[255] = 0;
        scoreboard->hole_par[i] = config_get_int(key);
    }
}

void scoreboard_reset(struct scoreboard *scoreboard) {
    for (int i = 0; i < 18; i++) {
        scoreboard->hole_score[i] = -1;
        scoreboard->hole_par[i] = 0;
    }
}
