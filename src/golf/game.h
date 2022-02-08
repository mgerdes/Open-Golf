#ifndef _GOLF_GAME_H
#define _GOLF_GAME_H

#include "common/level.h"

typedef enum golf_game_state {
    GOLF_GAME_STATE_MAIN_MENU,
    GOLF_GAME_STATE_WAITING_FOR_AIM,
} golf_game_state_t;

typedef struct golf_game {
    golf_game_state_t state;

    vec3 ball_pos;

    struct {
        float angle;
    } camera;
} golf_game_t;

golf_game_t *golf_game_get(void);
void golf_game_init(void);
void golf_game_update(float dt);
void golf_game_start_level(void);

#endif
