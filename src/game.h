#ifndef _GAME_H
#define _GAME_H

#include "array.h"
#include "assets.h"
#include "controls.h"
#include "hole.h"
#include "renderer.h"
#include "ui.h"

enum game_state {
    GAME_STATE_NOTHING,
    GAME_STATE_MAIN_MENU,
    GAME_STATE_BEGIN_HOLE,
    GAME_STATE_BEGINNING_CAMERA_ANIMATION,
    GAME_STATE_WAITING_FOR_AIM, 
    GAME_STATE_BEGINNING_AIM, 
    GAME_STATE_AIMING, 
    GAME_STATE_SIMULATING_BALL,
    GAME_STATE_CELEBRATION,
    GAME_STATE_HOLE_COMPLETE,
};

#define GAME_MAX_NUM_BALL_CONTACTS 16
struct ball_contact {
    bool is_ignored, is_cup, is_environment;
    vec3 position, normal, velocity;
    float distance, penetration, restitution, friction, impulse_mag, vel_scale, cull_dot;
    enum triangle_contact_type type;
    vec3 triangle_a, triangle_b, triangle_c, triangle_normal, impulse;
    vec3 v0, v1;
};

struct ball_entity {
    float time_going_slow;
    bool is_moving, is_in_cup;
    vec3 position, velocity, position_before_hit;
    vec3 draw_position;
    float radius;
    quat orientation;
    vec3 rotation_vec;
    float rotation_velocity;

    bool is_out_of_bounds;
    float time_out_of_bounds;

    bool in_water;
    float time_out_of_water, time_since_water_ripple;

    int stroke_num;

    vec3 color;
};
array_t(struct ball_entity, ball_entity_array)

struct physics_triangle {
    vec3 a, b, c;
    float cor, friction, vel_scale;
};
array_t(struct physics_triangle, physics_triangle_array)
struct physics_triangle physics_triangle_create(vec3 a, vec3 b, vec3 c,
        float cor, float friction, float vel_scale);

struct physics_cell {
    struct array_int triangle_idxs;
};
array_t(struct physics_cell, array_physics_cell);

struct game_editor;
#define GAME_MAX_AIM_LINE_POINTS 5
#define GAME_MAX_NUM_WATER_RIPPLES 64
struct game {
    enum game_state state;

    float t;
    struct ball_entity player_ball;
    int cur_hole;
    struct hole hole;

    struct {
        bool active;
        vec4 green_color, yellow_color, red_color, dark_red_color;
        float min_power, max_power,
              min_power_length, max_power_length, power,
              green_power, yellow_power, red_power;
        vec2 circle_pos;
        float circle_radius;
        float length;
        vec2 end_pos, delta;
        float angle;

        vec2 icon_bezier_point[4], icon_offset;
        float icon_width;

        vec3 direction;

        vec2 line_offset;
        int num_line_points;
        vec3 line_points[GAME_MAX_AIM_LINE_POINTS];
    } aim;

    struct {
        vec3 pos;
        float azimuth_angle, inclination_angle, azimuth_angle_before_hit;
        bool auto_rotate;
    } cam;

    struct {
        float t;
    } begin_hole;

    struct {
        float t;
        vec3 start_pos, end_pos, start_dir, end_dir;
    } beginning_cam_animation;

    struct {
        float t;
        vec3 start_pos, end_pos, start_dir, end_dir;
    } celebration_cam_animation;

    struct {
        int tick_idx;
        float time_behind;
        float fixed_dt;
        float cup_cor, cup_friction, cup_vel_scale, cup_force;

        struct array_vec3 water_triangles_dir;
        struct physics_triangle_array 
            hole_triangles, 
            cup_triangles, 
            water_triangles,
            environment_triangles;

        struct {
            struct array_physics_cell cells;
            vec3 corner_pos;
            float cell_size;
            int num_cols, num_rows;
        } grid;
    } physics;

    struct {
        float water_ripple_t_length;
        struct {
            vec3 pos;
            float t, radius;
        } water_ripples[GAME_MAX_NUM_WATER_RIPPLES];
        float water_t;
        bool is_blink;
        float blink_t, blink_t_length;
    } drawing;

    struct {
        bool is_scoreboard_open;
        struct scoreboard scoreboard;
        struct ui_button next_hole_button;
    } ui;

    struct {
        bool start_ball_impact_sound;
        float time_since_ball_impact_sound;
    } audio;
};

void game_init(struct game *game, struct game_editor *game_editor, struct renderer *renderer);
void game_update(struct game *game, float dt, struct button_inputs button_inputs, 
        struct renderer *renderer, struct game_editor *game_editor);
void game_physics_load_triangles(struct game *game);
void game_load_hole(struct game *game, struct game_editor *game_editor, int hole_num);
void game_hit_player_ball(struct game *game, vec3 direction, float power, struct game_editor *game_editor);
void game_init_ball_entity(struct ball_entity *entity);

#endif
