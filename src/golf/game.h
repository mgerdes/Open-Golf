#ifndef _GOLF_GAME_H
#define _GOLF_GAME_H

#include "common/bvh.h"
#include "common/level.h"

#define MAX_NUM_CONTACTS 8

typedef struct golf_collision_data {
    bool is_highlighted;
    vec3 ball_pos;
    int num_contacts;
    golf_ball_contact_t contacts[MAX_NUM_CONTACTS]; 
} golf_collision_data_t;
typedef vec_t(golf_collision_data_t) vec_golf_collision_data_t;

typedef enum golf_game_state {
    GOLF_GAME_STATE_MAIN_MENU,
    GOLF_GAME_STATE_WAITING_FOR_AIM,
    GOLF_GAME_STATE_AIMING,
    GOLF_GAME_STATE_WATCHING_BALL,
} golf_game_state_t;

typedef struct golf_game {
    golf_game_state_t state;

    struct {
        vec3 pos, vel, draw_pos;
        float time_going_slow, radius;
        bool is_moving;
    } ball;

    golf_bvh_t bvh;

    struct {
        float angle, angle_velocity;
    } cam;

    struct {
        float time_behind;

        bool debug_draw_collisions;
        vec_golf_collision_data_t collision_history; 
    } physics;
} golf_game_t;

golf_game_t *golf_game_get(void);
void golf_game_init(void);
void golf_game_update(float dt);
void golf_game_start_level(void);
void golf_game_start_aiming(void);
void golf_game_hit_ball(vec2 aim_delta);

#endif
