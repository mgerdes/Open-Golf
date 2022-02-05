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
    game.state = GOLF_GAME_STATE_TITLE_SCREEN;
    graphics = golf_graphics_get();
}

void golf_game_update(float dt) {
    switch (game.state) {
        case GOLF_GAME_STATE_TITLE_SCREEN: {
            break;
        }
    }

    graphics->cam_azimuth_angle += dt;
}
