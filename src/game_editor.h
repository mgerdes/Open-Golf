#ifndef _GAME_EDITOR_H
#define _GAME_EDITOR_H

#include "game_editor.h"

#include <stdbool.h>

#include "array.h"
#include "assets.h"
#include "data_stream.h"
#include "mfile.h"
#include "game.h"
#include "lightmaps.h"
#include "renderer.h"
#include "sokol_time.h"

enum editor_entity_type {
    EDITOR_ENTITY_WATER,
    EDITOR_ENTITY_MULTI_TERRAIN_MOVING,
    EDITOR_ENTITY_MULTI_TERRAIN_STATIC,
    EDITOR_ENTITY_TERRAIN,
    EDITOR_ENTITY_ENVIRONMENT,
    EDITOR_ENTITY_CUP,
    EDITOR_ENTITY_BALL_START,
    EDITOR_ENTITY_BEGINNING_CAMERA_ANIMATION,
    EDITOR_ENTITY_CAMERA_ZONE,
    EDITOR_ENTITY_POINT,
    EDITOR_ENTITY_FACE,
};
struct editor_entity {
    enum editor_entity_type type;
    int idx;
};
array_t(struct editor_entity, editor_entity_array);
struct editor_entity make_editor_entity(enum editor_entity_type type, int idx);
bool editor_entity_array_contains_entity(struct editor_entity_array *entity_array,
        struct editor_entity editor_entity);

enum object_modifier_mode {
    OBJECT_MODIFIER_MODE_TRANSLATE,
    OBJECT_MODIFIER_MODE_ROTATE,
    OBJECT_MODIFIER_MODE_SCALE,
};

#define OM_ROTATE_MODE_RES 30
struct object_modifier {
    enum object_modifier_mode mode;
    bool is_hovered, is_active, is_using, was_used, set_start_positions;
    vec3 pos;
    mat4 model_mat;
    struct {
        int axis;
        float cone_dist, cone_scale, line_radius;
        vec3 start_pos, delta_pos;
        bool cone_hovered[3];
        float clamp;
        bool start_delta_set;
        float start_delta;
    } translate_mode;
    struct {
        int axis;
        bool start_theta_set;
        float start_theta, end_theta, delta_theta, line_radius, circle_radius;
        vec3 circle_color[3], line_segment_p0[3 * OM_ROTATE_MODE_RES], line_segment_p1[3 * OM_ROTATE_MODE_RES];
        float clamp;
    } rotate_mode;
};

struct hole_editor_state {
    struct data_stream serialization;
    struct editor_entity_array selected_array;
};
array_t(struct hole_editor_state, hole_editor_state_array);

struct model_generator_script {
    mfiletime_t load_time;
    mfile_t file; 
    struct array_char_ptr params;
    struct array_float values;

    long long sc_env_loc;
    void *sc_env;
};
array_t(struct model_generator_script, array_model_generator_script);

struct hole_editor {
    bool open_save_as_dialog, open_load_dialog;

    struct {
        int idx;
        struct hole_editor_state_array state_array;
    } history;

    struct {
        vec3 pos;
        float azimuth_angle, inclination_angle;
    } cam;

    struct object_modifier object_modifier;
    struct editor_entity_array object_modifier_entities;
    struct array_vec3 object_modifier_start_positions; 
    struct array_quat object_modifier_start_orientations; 

    struct {
        bool is_open;
        vec2 p0, p1;
    } select_box;

    struct editor_entity_array selected_array;
    struct editor_entity_array hovered_array;
    struct hole *hole;

    struct {
        char path[MFILE_MAX_PATH];
        mdir_t holes_directory;
    } file;

    struct {
        struct {
            bool active;
            float line_dist;
        } helper_lines;
        struct {
            int draw_type;
        } terrain_entities;
    } drawing;

    struct {
        bool do_it, open_in_progress_dialog;  
#if (_WIN32)
        struct lightmap_generator_data *data;
#endif
        bool reset_lightmaps, create_uvs;
        float gamma;
        int num_dilates, num_smooths, num_iterations;
    } global_illumination;

    struct {
        struct {
            int lightmap_width, lightmap_height;
        } terrain;
        struct {
            mdir_t model_directory;
        } environment;
    } selected_entity;

    struct {
        bool is_moving;
    } multi_terrain_entities;

    struct {
        bool can_modify;
    } camera_zone_entities;

    struct {
        bool do_it;
        int new_num_images, new_width, new_height;
        struct lightmap *lightmap;
    } lightmap_update;

    struct {
        bool active;
        mat4 model_mat;
        struct terrain_model *model;
        struct editor_entity_array selected_array;
        struct editor_entity_array hovered_array;
        float selected_face_cor, selected_face_friction, selected_face_vel_scale;

        bool points_selectable, faces_selectable;
        char model_filename[MFILE_MAX_NAME];

        struct array_vec3 copied_points;
        struct array_terrain_model_face copied_faces;
    } edit_terrain_model;

    struct array_model_generator_script scripts;
    void *sc_state; 
    uint64_t sc_start_time;
    char sc_error[1024];
};

enum game_history_type {
    GAME_HISTORY_REST,
    GAME_HISTORY_HIT,
};
struct game_history {
    enum game_history_type type;
    struct {
        vec3 position;
    } rest;
    struct {
        float power;
        vec3 start_position, direction;
    } hit;
};
array_t(struct game_history, game_history_array);

struct physics_collision_data {
    int tick_idx;
    vec3 ball_pos, ball_vel;

    int num_ball_contacts;
    struct ball_contact ball_contacts[GAME_MAX_NUM_BALL_CONTACTS];
};
array_t(struct physics_collision_data, physics_collision_data_array);

struct game_editor {
    void *nk_ctx; 
    struct game *game;

    struct {
        char code[1024];
        vec3 start_position, direction;
        float power;
    } last_hit;

    struct {
        bool draw_triangles, draw_cup_debug, debug_collisions, draw_triangle_chunks;
        struct physics_collision_data_array collision_data_array;
        int selected_collision_data_idx;
    } physics;

    struct {
        map_int_t section_colors;
        int selected_frame_idx;
        struct profiler_section *selected_sub_section;
    } profiler;

    struct {
        bool paused;
        struct array_vec3 positions;
        struct array_int num_ticks;
    } ball_movement;

    int history_idx;
    struct game_history_array history;

    bool free_camera;
    int editing_hole;
    struct hole_editor hole_editor;
};

void game_editor_init(struct game_editor *editor, struct game *game, struct renderer *renderer);
void game_editor_input(const void *event);
void game_editor_update(struct game_editor *editor, float dt, struct button_inputs button_inputs, 
        struct renderer *renderer);
void game_editor_push_physics_contact_data(struct game_editor *ed, struct ball_contact contact);
void game_editor_push_physics_collision_data(struct game_editor *ed, int tick_idx, vec3 ball_pos, vec3 ball_vel); 
void game_editor_push_game_history_rest(struct game_editor *ed, vec3 position);
void game_editor_push_game_history_hit(struct game_editor *ed, float power, vec3 start_position, vec3 direction);
void game_editor_push_ball_movement(struct game_editor *ed, int num_ticks);
void game_editor_set_last_hit(struct game_editor *ed, vec3 start_position, vec3 direction, float power);

void game_editor_draw_warnings(struct game_editor *ed);

#endif
