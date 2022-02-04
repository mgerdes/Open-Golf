#include "golf/game.h"

#include "common/log.h"
#include "common/graphics.h"

static golf_game_t game;
static golf_graphics_t *graphics;

golf_game_t *golf_game_get(void) {
    return &game;
}

void golf_game_init(void) {
    golf_data_load("data/levels/level-1.level", false);
    game.level = golf_data_get_level("data/levels/level-1.level");

    graphics = golf_graphics_get();
}

void golf_game_update(float dt) {
    graphics->cam_azimuth_angle += dt;
}
