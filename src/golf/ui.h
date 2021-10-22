#ifndef _GOLF_UI_H
#define _GOLF_UI_H

#include <stdbool.h>

typedef enum golf_ui_state {
    GOLF_UI_MAIN_MENU,
} golf_ui_state_t;

typedef struct golf_ui {
    golf_ui_state_t state;
    union {
        struct {
            bool is_level_select_open;
        } main_menu;
    };
} golf_ui_t;

golf_ui_t *golf_ui_get(void);
void golf_ui_init(void);
void golf_ui_update(float dt);

#endif
