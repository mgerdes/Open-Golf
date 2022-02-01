#ifndef _GOLF_GAME_H
#define _GOLF_GAME_H

#include "common/level.h"

typedef struct golf_game {
    golf_level_t *level;
} golf_game_t;

golf_game_t *golf_game_get(void);
void golf_game_init(void);
void golf_game_update(float dt);
void golf_game_draw(void);

#endif
