#ifndef _GOLF_GAME_H
#define _GOLF_GAME_H

#include "common/bvh.h"
#include "common/level.h"

#define MAX_NUM_CONTACTS 8
#define MAX_AIM_LINE_POINTS 5

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

    bool debug_inputs;

    struct {
        vec3 start_pos, pos, vel, draw_pos, rot_vec;
        quat orientation;
        float time_going_slow, radius, rot_vel;
        bool is_moving, is_in_hole;
    } ball;

    struct {
        float angle, start_angle_velocity, angle_velocity;
    } cam;

    struct {
        golf_bvh_t static_bvh, dynamic_bvh;
        float time_behind;
        bool debug_draw_collisions;
        vec_golf_collision_data_t collision_history; 
    } physics;

    struct {
        float power;
        vec2 aim_delta;
        vec2 offset;
        int num_points;
        vec3 points[MAX_AIM_LINE_POINTS];
    } aim_line;
} golf_game_t;

golf_game_t *golf_game_get(void);
void golf_game_init(void);
void golf_game_update(float dt);
void golf_game_start_level(void);
void golf_game_start_aiming(void);
void golf_game_hit_ball(vec2 aim_delta);

#endif
