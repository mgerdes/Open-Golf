#ifndef _GOLF_GAME_H
#define _GOLF_GAME_H

#include "common/level.h"

typedef enum golf_game_state {
    GOLF_GAME_STATE_TITLE_SCREEN,
} golf_game_state_t;

typedef struct golf_game {
    golf_game_state_t state;
    golf_level_t *level;
} golf_game_t;

golf_game_t *golf_game_get(void);
void golf_game_init(void);
void golf_game_update(float dt);

#endif
