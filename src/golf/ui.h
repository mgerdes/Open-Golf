#ifndef _UI_H
#define _UI_H

#include "controls.h"
#include "maths.h"

struct ui_button {
    bool is_clicked, is_hovered;
    vec2 pos, size;
};

void ui_button_init(struct ui_button *button, vec2 pos, vec2 size);
void ui_button_update(struct ui_button *button, struct button_inputs inputs);

struct main_menu {
    struct ui_button start_game_button;
};

void main_menu_init(struct main_menu *main_menu);
void main_menu_update(struct main_menu *main_menu, float dt, struct button_inputs inputs);

struct scoreboard {
    int hole_score[18], hole_par[18]; 
};

void scoreboard_init(struct scoreboard *scoreboard);
void scoreboard_reset(struct scoreboard *scoreboard);

#endif
