#include "golf/game.h"

static golf_game_t game;

golf_game_t *golf_game_get(void) {
    return &game;
}

void golf_game_init(void) {
    golf_data_load("data/levels/level-1.level");
    game.level = golf_data_get_level("data/levels/level-1.level");
}

void golf_game_update(float dt) {

}
