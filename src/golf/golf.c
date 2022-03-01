#include "golf/golf.h"

#include "common/debug_console.h"
#include "common/graphics.h"
#include "golf/game.h"
#include "golf/ui.h"

static golf_t golf;
static golf_graphics_t *graphics;

static const char *initial_level_path = "data/levels/level-4.level";

void golf_init(void) {
    golf_data_load(initial_level_path, true);
    golf.state = GOLF_STATE_TITLE_SCREEN;
    golf.title_screen.t = 0;
    graphics = golf_graphics_get();

    golf_game_init();
    golf_ui_init();
}

void golf_update(float dt) {
    switch (golf.state) {
        case GOLF_STATE_TITLE_SCREEN: {
            golf.title_screen.t += dt;
            if (golf_data_get_load_state(initial_level_path) == GOLF_DATA_LOADED) {
                golf.level = golf_data_get_level(initial_level_path);
                golf.state = GOLF_STATE_MAIN_MENU;
            }
            break;
        }
        case GOLF_STATE_MAIN_MENU: {
            break;
        }
        case GOLF_STATE_LOADING_LEVEL: {
            golf.loading_level.t += dt;
            if (golf_data_get_load_state(initial_level_path) == GOLF_DATA_LOADED) {
                golf.level = golf_data_get_level(initial_level_path);
                golf.state = GOLF_STATE_IN_GAME;
                golf_game_start_level();
            }
            break;
        }
        case GOLF_STATE_IN_GAME: {
            break;
        }
    }

    golf_game_update(dt);
    golf_ui_update(dt);
    golf_debug_console_update(dt);
}

golf_t *golf_get(void) {
    return &golf;
}

void golf_start_level(void) {
    golf.state = GOLF_STATE_LOADING_LEVEL;
}
