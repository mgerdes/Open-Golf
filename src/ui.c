#include "ui.h"

#include "audio.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "config.h"

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
    scoreboard->hole_par[0] = config_get_int("game_hole_1_par");
    scoreboard->hole_par[1] = config_get_int("game_hole_2_par");
    scoreboard->hole_par[2] = config_get_int("game_hole_3_par");
    scoreboard->hole_par[3] = config_get_int("game_hole_4_par");
    scoreboard->hole_par[4] = config_get_int("game_hole_5_par");
}

void scoreboard_reset(struct scoreboard *scoreboard) {
    for (int i = 0; i < 18; i++) {
        scoreboard->hole_score[i] = -1;
        scoreboard->hole_par[i] = 0;
    }
}
