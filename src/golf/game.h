#ifndef _GOLF_GAME_H
#define _GOLF_GAME_H

#include "common/bvh.h"
#include "common/level.h"

typedef enum golf_game_state {
    GOLF_GAME_STATE_MAIN_MENU,
    GOLF_GAME_STATE_WAITING_FOR_AIM,
    GOLF_GAME_STATE_AIMING,
} golf_game_state_t;

typedef struct golf_game {
    golf_game_state_t state;

	struct {
		vec3 pos;
		float radius;
	} ball;

	golf_bvh_t bvh;

    struct {
        float angle, angle_velocity;
    } camera;
} golf_game_t;

golf_game_t *golf_game_get(void);
void golf_game_init(void);
void golf_game_update(float dt);
void golf_game_start_level(void);
void golf_game_start_aiming(void);
void golf_game_hit_ball(vec2 aim_delta);

#endif
