#ifndef _GOLF_GAME_H
#define _GOLF_GAME_H

#include "golf/level.h"

typedef struct golf_game {
    golf_level_t *level;
} golf_game_t;

golf_game_t *golf_game_get(void);
void golf_game_init(void);
void golf_game_update(float dt);

#endif
