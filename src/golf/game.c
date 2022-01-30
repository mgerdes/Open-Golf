#include "golf/game.h"

#include "golf/renderer.h"

static golf_game_t game;
static golf_renderer_t *renderer;

golf_game_t *golf_game_get(void) {
    return &game;
}

void golf_game_init(void) {
    golf_data_load("data/levels/level-1.level");
    game.level = golf_data_get_level("data/levels/level-1.level");

    renderer = golf_renderer_get();
}

void golf_game_update(float dt) {
    renderer->cam_azimuth_angle += dt;
}
