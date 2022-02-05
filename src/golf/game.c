#include "golf/game.h"

#include "common/log.h"
#include "common/graphics.h"

static golf_game_t game;
static golf_graphics_t *graphics;

golf_game_t *golf_game_get(void) {
    return &game;
}

void golf_game_init(void) {
    golf_data_load("data/title_screen.static_data", false);
    golf_data_load("data/levels/level-1.level", true);
    game.state = GOLF_GAME_STATE_TITLE_SCREEN;
    graphics = golf_graphics_get();
}

void golf_game_update(float dt) {
    switch (game.state) {
        case GOLF_GAME_STATE_TITLE_SCREEN: {
            if (golf_data_get_load_state("data/levels/level-1.level") == GOLF_DATA_LOADED) {
                game.level = golf_data_get_level("data/levels/level-1.level");
                game.state = GOLF_GAME_STATE_MAIN_MENU;
            }
            break;
        }
        case GOLF_GAME_STATE_MAIN_MENU: {
            break;
        }
    }

    graphics->cam_azimuth_angle += dt;
}
