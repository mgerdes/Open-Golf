#define _CRT_SECURE_NO_WARNINGS

#include "game.h"

#include <assert.h>
#include <float.h>
#include <string.h>

#include "assets.h"
#include "audio.h"
#include "config.h"
#include "game_editor.h"
#include "hole.h"
#include "lightmaps.h"
#include "log.h"
#include "maths.h"
#include "profiler.h"
#include "renderer.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include "thread.h"

static bool game_get_camera_zone_angle(struct game *game, vec3 position, float *angle) {
    for (int i = 0; i < game->hole.camera_zone_entities.length; i++) {
        struct camera_zone_entity entity = game->hole.camera_zone_entities.data[i];
        vec3 p = entity.position;
        vec2 s = entity.size;
        if (position.x <= p.x + s.x && position.x >= p.x - s.x
               && position.z <= p.z + s.y && position.z >= p.z - s.y) {

            if (entity.look_towards_cup) {
                vec2 p0 = V2(game->hole.cup_entity.position.x, game->hole.cup_entity.position.z);
                vec2 p1 = V2(position.x, position.z);
                vec2 dir = vec2_normalize(vec2_sub(p0, p1));
                float theta = acosf(dir.x);
                if (dir.y <= 0.0f) {
                    theta *= -1.0f;
                }
                *angle = theta + MF_PI;
            }
            else {
                vec3 axis;
                quat_get_axis_angle(entity.orientation, &axis, angle);
                *angle = *angle + MF_PI;
            }

            return true;
        }
    }

    return false;
}

static void game_physics_create_grid(struct game *game) {
    float min_x = FLT_MAX, max_x = -FLT_MAX;
    float min_z = FLT_MAX, max_z = -FLT_MAX;
    int num_triangles = game->physics.environment_triangles.length;
    vec3 *triangle_points = malloc(sizeof(vec3)*num_triangles*3);
    bool *is_inside = malloc(sizeof(bool)*num_triangles);

    for (int i = 0; i < game->physics.environment_triangles.length; i++) {
        struct physics_triangle tri = game->physics.environment_triangles.data[i];

        if (tri.a.x < min_x) min_x = tri.a.x;
        if (tri.a.x > max_x) max_x = tri.a.x;
        if (tri.a.z < min_z) min_z = tri.a.z;
        if (tri.a.z > max_z) max_z = tri.a.z;

        if (tri.b.x < min_x) min_x = tri.b.x;
        if (tri.b.x > max_x) max_x = tri.b.x;
        if (tri.b.z < min_z) min_z = tri.b.z;
        if (tri.b.z > max_z) max_z = tri.b.z;

        if (tri.c.x < min_x) min_x = tri.c.x;
        if (tri.c.x > max_x) max_x = tri.c.x;
        if (tri.c.z < min_z) min_z = tri.c.z;
        if (tri.c.z > max_z) max_z = tri.c.z;

        triangle_points[3*i + 0] = tri.a;
        triangle_points[3*i + 1] = tri.b;
        triangle_points[3*i + 2] = tri.c;
    }

    float cell_size = game->physics.grid.cell_size;
    game->physics.grid.corner_pos = V3(min_x, 0.0f, min_z);
    game->physics.grid.num_cols = (int) ceilf((max_x - min_x)/cell_size);
    game->physics.grid.num_rows = (int) ceilf((max_z - min_z)/cell_size);

    vec3 pos = game->physics.grid.corner_pos;
    int num_cols = game->physics.grid.num_cols;
    int num_rows = game->physics.grid.num_rows;

    for (int i = 0; i < game->physics.grid.cells.length; i++) {
        array_deinit(&game->physics.grid.cells.data[i].triangle_idxs);
    }
    game->physics.grid.cells.length = 0;
    for (int col = 0; col < num_cols; col++) {
        for (int row = 0; row < num_rows; row++) {
            vec3 box_center;
            box_center.x = pos.x + (col + 0.5f)*cell_size;
            box_center.y = 0.0f;
            box_center.z = pos.z + (row + 0.5f)*cell_size;

            vec3 box_half_lengths;
            box_half_lengths.x = 0.5f*cell_size;
            box_half_lengths.y = 9999.0f;
            box_half_lengths.z = 0.5f*cell_size;

            triangles_inside_box(triangle_points, num_triangles, box_center, box_half_lengths, is_inside);

            struct physics_cell cell;
            array_init(&cell.triangle_idxs);
            for (int i = 0; i < num_triangles; i++) {
                if (is_inside[i]) {
                    array_push(&cell.triangle_idxs, i);
                }
            }
            array_push(&game->physics.grid.cells, cell);
        }
    }

    free(triangle_points);
    free(is_inside);
}

void game_physics_load_triangles(struct game *game) {
    {
        game->physics.hole_triangles.length = 0;
        for (int i = 0; i < game->hole.terrain_entities.length; i++) {
            struct terrain_entity *entity = &game->hole.terrain_entities.data[i];
            struct terrain_model *model = &entity->terrain_model;
            mat4 transform = terrain_entity_get_transform(entity);
            for (int i = 0; i < model->faces.length; i++) {
                struct terrain_model_face face = model->faces.data[i];
                assert(face.num_points == 3 || face.num_points == 4);

                vec3 p0 = vec3_apply_mat4(model->points.data[face.x], 1.0f, transform);
                vec3 p1 = vec3_apply_mat4(model->points.data[face.y], 1.0f, transform);
                vec3 p2 = vec3_apply_mat4(model->points.data[face.z], 1.0f, transform);
                struct physics_triangle tri = physics_triangle_create(p0, p1, p2, face.cor, face.friction,
                        face.vel_scale);
                array_push(&game->physics.hole_triangles, tri);
                if (face.num_points == 4) {
                    vec3 p3 = vec3_apply_mat4(model->points.data[face.w], 1.0f, transform);
                    struct physics_triangle tri = physics_triangle_create(p2, p3, p0, face.cor, face.friction,
                            face.vel_scale);
                    array_push(&game->physics.hole_triangles, tri);
                }
            }
        }
    }

    {
        for (int i = 0; i < game->hole.multi_terrain_entities.length; i++) {
            struct multi_terrain_entity *entity = &game->hole.multi_terrain_entities.data[i];
            struct terrain_model *static_model = &entity->static_terrain_model;
            struct terrain_model *moving_model = &entity->moving_terrain_model;
            mat4 static_transform = multi_terrain_entity_get_static_transform(entity);

            for (int i = 0; i < static_model->faces.length; i++) {
                struct terrain_model_face face = static_model->faces.data[i];
                assert(face.num_points == 3 || face.num_points == 4);

                vec3 p0 = vec3_apply_mat4(static_model->points.data[face.x], 1.0f, static_transform);
                vec3 p1 = vec3_apply_mat4(static_model->points.data[face.y], 1.0f, static_transform);
                vec3 p2 = vec3_apply_mat4(static_model->points.data[face.z], 1.0f, static_transform);
                struct physics_triangle tri = physics_triangle_create(p0, p1, p2, face.cor, face.friction,
                        face.vel_scale);
                array_push(&game->physics.hole_triangles, tri);
                if (face.num_points == 4) {
                    vec3 p3 = vec3_apply_mat4(static_model->points.data[face.w], 1.0f, static_transform);
                    struct physics_triangle tri = physics_triangle_create(p2, p3, p0, face.cor, face.friction,
                            face.vel_scale);
                    array_push(&game->physics.hole_triangles, tri);
                }
            }
        }
    }

    {
        game->physics.cup_triangles.length = 0;
        struct model *cup_model = asset_store_get_model("hole");
        mat4 cup_transform = cup_entity_get_transform(&game->hole.cup_entity);
        for (int i = 0; i < cup_model->num_points; i += 3) {
            vec3 a = vec3_apply_mat4(cup_model->positions[i + 0], 1.0f, cup_transform);
            vec3 b = vec3_apply_mat4(cup_model->positions[i + 1], 1.0f, cup_transform);
            vec3 c = vec3_apply_mat4(cup_model->positions[i + 2], 1.0f, cup_transform);
            float cor = 0.5f;
            float friction = 0.5f;
            float vel_scale = 1.0f;
            struct physics_triangle tri = physics_triangle_create(a, b, c, cor, friction, vel_scale);
            array_push(&game->physics.cup_triangles, tri);
        }
    }

    {
        game->physics.water_triangles.length = 0;
        game->physics.water_triangles_dir.length = 0;
        for (int i = 0; i < game->hole.water_entities.length; i++) {
            struct water_entity *entity = &game->hole.water_entities.data[i];
            mat4 transform = water_entity_get_transform(entity);
            struct terrain_model *model = &entity->model;

            for (int i = 0; i < model->faces.length; i++) {
                struct terrain_model_face face = model->faces.data[i];
                assert(face.num_points == 3 || face.num_points == 4);

                vec3 p0 = vec3_apply_mat4(model->points.data[face.x], 1.0f, transform);
                vec3 p1 = vec3_apply_mat4(model->points.data[face.y], 1.0f, transform);
                vec3 p2 = vec3_apply_mat4(model->points.data[face.z], 1.0f, transform);
                vec3 p3;

                struct physics_triangle tri = physics_triangle_create(p0, p1, p2, face.cor, face.friction,
                        face.vel_scale);
                array_push(&game->physics.water_triangles, tri);
                if (face.num_points == 4) {
                    p3 = vec3_apply_mat4(model->points.data[face.w], 1.0f, transform);
                    struct physics_triangle tri = physics_triangle_create(p2, p3, p0, face.cor, face.friction,
                            face.vel_scale);
                    array_push(&game->physics.water_triangles, tri);
                }

                vec3 water_dir;
                if (face.num_points == 3) {
                    water_dir = vec3_normalize(vec3_sub(p1, p2));
                }
                else if (face.num_points == 4) {
                    water_dir = vec3_normalize(vec3_add(vec3_sub(p3, p0), vec3_sub(p2, p1)));
                }
                array_push(&game->physics.water_triangles_dir, water_dir);
                if (face.num_points == 4) {
                    array_push(&game->physics.water_triangles_dir, water_dir);
                }
            }
        }
    }

    {
        game->physics.environment_triangles.length = 0;
        for (int i = 0; i < game->hole.environment_entities.length; i++) {
            struct environment_entity *entity = &game->hole.environment_entities.data[i];
            mat4 transform = environment_entity_get_transform(entity);
            struct model *model = entity->model;
            for (int i = 0; i < model->num_points; i += 3) {
                vec3 a = vec3_apply_mat4(model->positions[i + 0], 1.0f, transform);
                vec3 b = vec3_apply_mat4(model->positions[i + 1], 1.0f, transform);
                vec3 c = vec3_apply_mat4(model->positions[i + 2], 1.0f, transform);
                float cor = 0.4f;
                float friction = 0.3f;
                float vel_scale = 1.0f;
                struct physics_triangle tri = physics_triangle_create(a, b, c, cor, friction, vel_scale);
                array_push(&game->physics.environment_triangles, tri);
            }
        }
        game_physics_create_grid(game);
    }
}

static thread_ptr_t load_hole_thread;

static int thread_proc_game_load_hole(void *user_data) {
    struct game *game = (struct game *)user_data;
    game_physics_load_triangles(game);
    return 0;
}

void game_load_hole(struct game *game, struct game_editor *ed, int hole_num) {
    m_logf("num_holes: %d\nhole_num: %d\n", config_get_int("game_num_holes"), hole_num);
    assert(hole_num >= 0 && hole_num < config_get_int("game_num_holes"));

    game->cur_hole = hole_num;
    char filename[MFILE_MAX_PATH + 1];
    snprintf(filename, MFILE_MAX_PATH, "assets/holes/hole%d.hole", hole_num + 1);
    filename[MFILE_MAX_PATH] = 0;
    mfile_t file = mfile(filename);
    hole_load(&game->hole, &file);
    hole_update_buffers(&game->hole);

    load_hole_thread = thread_create0(thread_proc_game_load_hole, game, NULL, THREAD_STACK_SIZE_DEFAULT); 

    {
        game_init_ball_entity(&game->player_ball);
        game->player_ball.position = game->hole.ball_start_entity.position;
        game->player_ball.position.y += game->player_ball.radius;
        game->player_ball.draw_position = game->player_ball.position;

        ed->history_idx = 0;
        ed->history.length = 0;
        game_editor_push_game_history_rest(ed, game->player_ball.position);
    }

    {

        game->begin_hole.t = 0.0f;
    }

    {
        float end_azimuth_angle;
        game_get_camera_zone_angle(game, game->hole.ball_start_entity.position, &end_azimuth_angle);

        vec3 start_pos = game->hole.beginning_camera_animation_entity.start_position;
        vec3 end_pos = game->player_ball.position;
        end_pos = vec3_add(end_pos, vec3_rotate_y(config_get_vec3("game_cam_delta"), -end_azimuth_angle));

        vec3 start_dir = vec3_normalize(vec3_sub(game->hole.cup_entity.position, start_pos));
        vec3 end_dir = vec3_normalize(vec3_sub(game->player_ball.position, end_pos));

        game->beginning_cam_animation.t = 0.0f;
        game->beginning_cam_animation.start_pos = start_pos;
        game->beginning_cam_animation.end_pos = end_pos;
        game->beginning_cam_animation.start_dir = start_dir;
        game->beginning_cam_animation.end_dir = end_dir;
    }

    if (hole_num == 0) {
        scoreboard_init(&game->ui.scoreboard);
    }
}

void game_init(struct game *game, struct game_editor *ed, struct renderer *renderer) {
    profiler_push_section("game_init");
    game->state = GAME_STATE_NOTHING;
    game->t = 0.0f;

    {
        game->aim.active = false;
        game->aim.green_color = config_get_vec4("aim_green_color");;
        game->aim.yellow_color = config_get_vec4("aim_yellow_color");;
        game->aim.red_color = config_get_vec4("aim_red_color");;
        game->aim.dark_red_color = config_get_vec4("aim_dark_red_color");;
        game->aim.min_power = config_get_float("aim_min_power");
        game->aim.max_power = config_get_float("aim_max_power");
        game->aim.min_power_length = config_get_float("aim_min_power_length");
        game->aim.max_power_length = config_get_float("aim_max_power_length");
        game->aim.green_power = config_get_float("aim_green_power");
        game->aim.yellow_power = config_get_float("aim_yellow_power");
        game->aim.red_power = config_get_float("aim_red_power");
        game->aim.icon_bezier_point[3] = config_get_vec2("aim_icon_bezier_point_3");
        game->aim.icon_bezier_point[2] = config_get_vec2("aim_icon_bezier_point_2");
        game->aim.icon_bezier_point[1] = config_get_vec2("aim_icon_bezier_point_1");
        game->aim.icon_bezier_point[0] = config_get_vec2("aim_icon_bezier_point_0");
        game->aim.icon_offset = config_get_vec2("aim_icon_offset"); 
        game->aim.line_offset = config_get_vec2("aim_line_offset");
        game->aim.num_line_points = 0;
        game->aim.circle_pos = V2(0.5f*renderer->game_fb_width, 0.5f*renderer->game_fb_height);
        game->aim.circle_radius = config_get_float("aim_circle_radius");
        renderer_update_game_icon_buffer(renderer, game->aim.icon_offset, 
                game->aim.icon_bezier_point[0], game->aim.icon_bezier_point[1],
                game->aim.icon_bezier_point[2], game->aim.icon_bezier_point[3]);
    }

    {
        game->cam.pos = V3(0.0f, 0.0f, 0.0f);
        game->cam.azimuth_angle = 0.0f;
        game->cam.inclination_angle = 0.5f * MF_PI;
        game->cam.auto_rotate = false;
    }

    {
        game->physics.tick_idx = 0;
        game->physics.fixed_dt = 1.0f / config_get_float("physics_hz");
        game->physics.time_behind = 0.0;
        game->physics.cup_cor = config_get_float("physics_cup_cor");
        game->physics.cup_friction = config_get_float("physics_cup_friction");
        game->physics.cup_vel_scale = config_get_float("physics_cup_vel_scale");
        array_init(&game->physics.hole_triangles);
        array_init(&game->physics.water_triangles);
        array_init(&game->physics.water_triangles_dir);
        array_init(&game->physics.cup_triangles);
        array_init(&game->physics.environment_triangles);
        array_init(&game->physics.grid.cells);
        game->physics.grid.cell_size = config_get_float("physics_grid_cell_size");
        game->physics.cup_force = config_get_float("physics_cup_force");
    }

    {
        game->begin_hole.t = 0.0f;
    }

    {
        game->beginning_cam_animation.t = 0.0f;
        game->beginning_cam_animation.start_pos = V3_ZERO;
        game->beginning_cam_animation.end_pos = V3_ZERO;
        game->beginning_cam_animation.start_dir = V3_ZERO;
        game->beginning_cam_animation.end_dir = V3_ZERO;
    }

    {
        game->drawing.water_ripple_t_length = config_get_float("water_ripple_t_length");
        for (int i = 0; i < GAME_MAX_NUM_WATER_RIPPLES; i++) {
            game->drawing.water_ripples[i].pos = V3(0.0f, 0.0f, 0.0f);
            game->drawing.water_ripples[i].t = FLT_MAX;
        }
        game->drawing.water_t = 0.0f;
        game->drawing.is_blink = false;
        game->drawing.blink_t = 0.0f;
        game->drawing.blink_t_length = config_get_float("drawing_blink_length");
    }

    {
        game->ui.is_scoreboard_open = false;
        scoreboard_init(&game->ui.scoreboard);

        vec2 next_hole_button_pos = config_get_vec2("next_hole_button_pos");
        vec2 next_hole_button_size = config_get_vec2("next_hole_button_size");
        ui_button_init(&game->ui.next_hole_button, next_hole_button_pos, next_hole_button_size);
    }

    {
        game->audio.start_ball_impact_sound = false;
        game->audio.time_since_ball_impact_sound = 0.0f;
    }

    {
        game_init_ball_entity(&game->player_ball);
        hole_init(&game->hole);
    }

    profiler_pop_section();
}

int ball_contact_cmp(const void *a, const void *b) {
    const struct ball_contact *bc0 = (struct ball_contact *)a;
    const struct ball_contact *bc1 = (struct ball_contact *)b;

    if (bc1->distance > bc0->distance) {
        return -1;
    }
    else if (bc1->distance < bc0->distance) {
        return 1;
    }
    else if (bc1->vel_scale > bc0->vel_scale) {
        return 1;
    }
    else if (bc1->vel_scale < bc0->vel_scale) {
        return -1;
    }
    else if (bc1->restitution > bc0->restitution) {
        return -1;
    }
    else if (bc1->restitution < bc0->restitution) {
        return 1;
    }
    else {
        return 0;
    }
}

static bool game_physics_create_ball_contact(struct physics_triangle triangle, vec3 ball_pos, vec3 ball_vel,
        float ball_radius, struct ball_contact *contact) {
    vec3 a = triangle.a;
    vec3 b = triangle.b;
    vec3 c = triangle.c;
    float cor = triangle.cor;
    float friction = triangle.friction;
    float vel_scale = triangle.vel_scale;

    enum triangle_contact_type type;
    vec3 cp = closest_point_point_triangle(ball_pos, a, b, c, &type);
    float dist = vec3_distance(cp, ball_pos);
    if (dist < ball_radius) {
        contact->type = type;
        contact->is_ignored = false;
        contact->is_cup = false;
        contact->is_environment = false;
        contact->distance = dist;
        contact->penetration = ball_radius - dist;
        contact->restitution = cor;
        contact->friction = friction;
        contact->impulse = V3_ZERO;
        contact->impulse_mag = 0.0f;
        contact->position = cp;
        contact->triangle_a = a;
        contact->triangle_b = b;
        contact->triangle_c = c;
        contact->triangle_normal = vec3_normalize(vec3_cross(vec3_sub(b, a), vec3_sub(c, a)));
        contact->cull_dot = 0.0f;
        contact->vel_scale = vel_scale;
        contact->v0 = V3_ZERO;
        contact->v1 = V3_ZERO;

        contact->normal = V3_ZERO;
        if (type == TRIANGLE_CONTACT_FACE) {
            contact->normal = contact->triangle_normal;
        }
        else if (type == TRIANGLE_CONTACT_AB || type == TRIANGLE_CONTACT_AC || type == TRIANGLE_CONTACT_BC) {
            contact->normal = vec3_normalize(vec3_sub(ball_pos, cp));
        }
        else if (type == TRIANGLE_CONTACT_A || type == TRIANGLE_CONTACT_B || type == TRIANGLE_CONTACT_C) {
            contact->normal = vec3_normalize(vec3_sub(ball_pos, cp));
        }
        contact->velocity = V3(0.0f, 0.0f, 0.0f);
        return true;
    }
    return false;
}

static void game_physics_grid_cell_create_contacts(struct game *game, int cell_col, int cell_row,
        struct ball_contact *ball_contacts, int *num_ball_contacts, vec3 bp, vec3 bv, float br) {
    int cell_idx = cell_col*game->physics.grid.num_rows + cell_row;
    if (cell_idx >= 0 && cell_idx < game->physics.grid.cells.length) {
        struct physics_cell cell = game->physics.grid.cells.data[cell_idx];
        for (int i = 0; i < cell.triangle_idxs.length; i++) {
            int idx = cell.triangle_idxs.data[i];

            struct ball_contact contact;
            if (game_physics_create_ball_contact(game->physics.environment_triangles.data[idx], 
                        bp, bv, br, &contact)) {
                contact.is_environment = true;
                if (*num_ball_contacts < GAME_MAX_NUM_BALL_CONTACTS) {
                    ball_contacts[(*num_ball_contacts)++] = contact;
                }
            }
        }
    }
}

static void game_physics_tick(struct game *game, float dt, struct renderer *renderer, struct game_editor *ed) {
    profiler_push_section("physics_tick");

    struct ball_entity *ball = &game->player_ball;
    vec3 bp = ball->position;
    vec3 bv = ball->velocity;
    vec3 bp0 = bp;
    vec3 bv0 = bv;
    float bs = vec3_length(bv);
    float br = ball->radius;
    vec3 hole_pos = game->hole.cup_entity.position;
    float hole_radius = game->hole.cup_entity.radius;

    int num_ball_contacts = 0;
    struct ball_contact ball_contacts[GAME_MAX_NUM_BALL_CONTACTS];

    {
        profiler_push_section("physics_create_contacts");
        vec2 hole_pos_2 = V2(hole_pos.x, hole_pos.z);
        vec2 ball_pos_2 = V2(bp.x, bp.z);
        if (vec2_distance(hole_pos_2, ball_pos_2) < hole_radius) {
            for (int i = 0; i < game->physics.cup_triangles.length; i++) {
                struct ball_contact contact;
                if (game_physics_create_ball_contact(game->physics.cup_triangles.data[i], bp, bv, br, 
                            &contact)) {
                    if (contact.type == TRIANGLE_CONTACT_AB || contact.type == TRIANGLE_CONTACT_AC ||
                            contact.type == TRIANGLE_CONTACT_BC) {
                        contact.restitution = game->physics.cup_cor;
                        if (bs > 2.0f) {
                            contact.friction = game->physics.cup_friction;
                            contact.vel_scale = game->physics.cup_vel_scale;
                        }
                        else {
                            contact.friction = 0.0f;
                            contact.vel_scale = 1.0f;
                        }
                    }
                    contact.is_cup = true;

                    if (num_ball_contacts < GAME_MAX_NUM_BALL_CONTACTS) {
                        ball_contacts[num_ball_contacts++] = contact;
                    }
                }
            }
        }
        else {
            profiler_push_section("physics_environment_triangles");
            vec3 grid_pos = game->physics.grid.corner_pos;
            float cell_size = game->physics.grid.cell_size;
            float cell_dx = bp.x - grid_pos.x - 0.5f*cell_size;
            float cell_dz = bp.z - grid_pos.z - 0.5f*cell_size; 
            int col0 = (int) floorf(cell_dx/cell_size);
            int row0 = (int) floorf(cell_dz/cell_size);
            int col1 = (int) ceilf(cell_dx/cell_size);
            int row1 = (int) ceilf(cell_dz/cell_size);
            game_physics_grid_cell_create_contacts(game, col0, row0, ball_contacts, &num_ball_contacts,
                    bp, bv, br);
            game_physics_grid_cell_create_contacts(game, col0, row1, ball_contacts, &num_ball_contacts,
                    bp, bv, br);
            game_physics_grid_cell_create_contacts(game, col1, row0, ball_contacts, &num_ball_contacts,
                    bp, bv, br);
            game_physics_grid_cell_create_contacts(game, col1, row1, ball_contacts, &num_ball_contacts,
                    bp, bv, br);
            profiler_pop_section();

            profiler_push_section("physics_triangles");
            for (int i = 0; i < game->physics.hole_triangles.length; i++) {
                struct ball_contact contact;
                if (game_physics_create_ball_contact(game->physics.hole_triangles.data[i], 
                            bp, bv, br, &contact)) {
                    if (num_ball_contacts < GAME_MAX_NUM_BALL_CONTACTS) {
                        ball_contacts[num_ball_contacts++] = contact;
                    }
                }
            }
            profiler_pop_section();

            profiler_push_section("physics_moving_triangles");
            for (int i = 0; i < game->hole.multi_terrain_entities.length; i++) {
                struct multi_terrain_entity *entity = &game->hole.multi_terrain_entities.data[i];
                struct terrain_model *moving_model = &entity->moving_terrain_model;
                mat4 model_mat = multi_terrain_entity_get_moving_transform(entity, game->t);
                struct ball_contact contact;

                for (int i = 0; i < moving_model->faces.length; i++) {
                    struct terrain_model_face face = moving_model->faces.data[i];
                    assert(face.num_points == 3 || face.num_points == 4);

                    vec3 p0 = vec3_apply_mat4(moving_model->points.data[face.x], 1.0f, model_mat);
                    vec3 p1 = vec3_apply_mat4(moving_model->points.data[face.y], 1.0f, model_mat);
                    vec3 p2 = vec3_apply_mat4(moving_model->points.data[face.z], 1.0f, model_mat);
                    struct physics_triangle tri = physics_triangle_create(p0, p1, p2, face.cor, face.friction,
                            face.vel_scale);
                    if (game_physics_create_ball_contact(tri, bp, bv, br, &contact)) {
                        if (num_ball_contacts < GAME_MAX_NUM_BALL_CONTACTS) {
                            contact.velocity = multi_terrain_entity_get_moving_velocity(entity,
                                    game->t, contact.position);
                            ball_contacts[num_ball_contacts++] = contact;
                        }
                    }
                    if (face.num_points == 4) {
                        vec3 p3 = vec3_apply_mat4(moving_model->points.data[face.w], 1.0f, model_mat);
                        struct physics_triangle tri = physics_triangle_create(p2, p3, p0, face.cor, face.friction,
                                face.vel_scale);
                        if (game_physics_create_ball_contact(tri, bp, bv, br, &contact)) {
                            if (num_ball_contacts < GAME_MAX_NUM_BALL_CONTACTS) {
                                contact.velocity = multi_terrain_entity_get_moving_velocity(entity,
                                        game->t, contact.position);
                                ball_contacts[num_ball_contacts++] = contact;
                            }
                        }
                    }
                }

            }
            profiler_pop_section();
        }
        qsort(ball_contacts, num_ball_contacts, sizeof(struct ball_contact), ball_contact_cmp);
        profiler_pop_section();
    }

    {
        profiler_push_section("physics_filter_contacts");
        if (num_ball_contacts > 0) {
            game_editor_push_physics_collision_data(ed, game->physics.tick_idx, bp0, bv0); 

            {
                float eps = 0.001f;
                int num_processed_vertices = 0;
                vec3 processed_vertices[9 * GAME_MAX_NUM_BALL_CONTACTS];

                // All face contacts are used
                for (int i = 0; i < num_ball_contacts; i++) {
                    struct ball_contact *contact = &ball_contacts[i];
                    if (contact->is_ignored || contact->type != TRIANGLE_CONTACT_FACE) {
                        continue;
                    }

                    assert(num_processed_vertices + 3 <= 9 * GAME_MAX_NUM_BALL_CONTACTS);
                    processed_vertices[num_processed_vertices++] = contact->triangle_a;
                    processed_vertices[num_processed_vertices++] = contact->triangle_b;
                    processed_vertices[num_processed_vertices++] = contact->triangle_c;
                }

                // Remove unecessary edge contacts
                for (int i = 0; i < num_ball_contacts; i++) {
                    struct ball_contact *contact = &ball_contacts[i];
                    if (contact->is_ignored ||
                            (contact->type != TRIANGLE_CONTACT_AB &&
                             contact->type != TRIANGLE_CONTACT_AC &&
                             contact->type != TRIANGLE_CONTACT_BC)) {
                        continue;
                    }

                    vec3 e0 = V3_ZERO, e1 = V3_ZERO;
                    if (contact->type == TRIANGLE_CONTACT_AB) {
                        e0 = contact->triangle_a;
                        e1 = contact->triangle_b;
                    }
                    else if (contact->type == TRIANGLE_CONTACT_AC) {
                        e0 = contact->triangle_a;
                        e1 = contact->triangle_c;
                    }
                    else if (contact->type == TRIANGLE_CONTACT_BC) {
                        e0 = contact->triangle_b;
                        e1 = contact->triangle_c;
                    }

                    for (int j = 0; j < num_processed_vertices; j += 3) {
                        vec3 a = processed_vertices[j + 0];
                        vec3 b = processed_vertices[j + 1];
                        vec3 c = processed_vertices[j + 2];
                        if (vec3_line_segments_on_same_line(a, b, e0, e1, eps) ||
                                vec3_line_segments_on_same_line(a, c, e0, e1, eps) ||
                                vec3_line_segments_on_same_line(b, c, e0, e1, eps)) {
                            contact->is_ignored = true;
                            break;
                        }
                    }

                    assert(num_processed_vertices + 3 <= 9 * GAME_MAX_NUM_BALL_CONTACTS);
                    processed_vertices[num_processed_vertices++] = contact->triangle_a;
                    processed_vertices[num_processed_vertices++] = contact->triangle_b;
                    processed_vertices[num_processed_vertices++] = contact->triangle_c;
                }

                // Remove unecessary point contacts
                for (int i = 0; i < num_ball_contacts; i++) {
                    struct ball_contact *contact = &ball_contacts[i];
                    if (contact->is_ignored || 
                            (contact->type != TRIANGLE_CONTACT_A &&
                             contact->type != TRIANGLE_CONTACT_B &&
                             contact->type != TRIANGLE_CONTACT_C)) {
                        continue;
                    }

                    vec3 p = V3_ZERO;
                    if (contact->type == TRIANGLE_CONTACT_A) {
                        p = contact->triangle_a;
                    }
                    else if (contact->type == TRIANGLE_CONTACT_B) {
                        p = contact->triangle_b;
                    }
                    else if (contact->type == TRIANGLE_CONTACT_C) {
                        p = contact->triangle_c;
                    }

                    for (int j = 0; j < num_processed_vertices; j += 3) {
                        vec3 a = processed_vertices[j + 0];
                        vec3 b = processed_vertices[j + 1];
                        vec3 c = processed_vertices[j + 2];
                        if (vec3_point_on_line_segment(p, a, b, eps) ||
                                vec3_point_on_line_segment(p, a, c, eps) ||
                                vec3_point_on_line_segment(p, b, c, eps)) {
                            contact->is_ignored = true;
                            break;
                        }
                    }

                    assert(num_processed_vertices + 3 <= 9 * GAME_MAX_NUM_BALL_CONTACTS);
                    processed_vertices[num_processed_vertices++] = contact->triangle_a;
                    processed_vertices[num_processed_vertices++] = contact->triangle_b;
                    processed_vertices[num_processed_vertices++] = contact->triangle_c;
                }
            }

            for (int i = 0; i < num_ball_contacts; i++) {
                struct ball_contact *contact = &ball_contacts[i];
                vec3 vr = vec3_sub(bv, contact->velocity);
                contact->cull_dot = vec3_dot(contact->normal, vec3_normalize(vr));
                if (contact->cull_dot > 0.01f) {
                    contact->is_ignored = true;
                }
                if (contact->is_ignored) {
                    continue;
                }

                vec3 n = contact->normal;
                float e = contact->restitution;
                float v_scale = contact->vel_scale;
                float imp = -(1.0f + e) * vec3_dot(vr, n);
                contact->impulse_mag = imp;
                contact->impulse = vec3_scale(n, imp);
                contact->v0 = bv;

                bv = vec3_add(bv, vec3_scale(n, imp)); 
                bv = vec3_scale(bv, v_scale);

                ball->rotation_velocity = vec3_length(bv) / (2.0f * MF_PI * ball->radius);
                ball->rotation_vec = vec3_normalize(vec3_cross(n, bv));

                vec3 t = vec3_sub(bv, vec3_scale(n, vec3_dot(bv, n)));
                if (vec3_length(t) > 0.0001f) {
                    t = vec3_normalize(t);

                    float jt = -vec3_dot(vr, t);
                    if (fabsf(jt) > 0.0001f) {
                        float friction = contact->friction;
                        if (jt > imp * friction) {
                            jt = imp * friction;
                        }
                        else if (jt < -imp * friction) {
                            jt = -imp * friction;
                        }

                        bv = vec3_add(bv, vec3_scale(t, jt));
                    }
                }

                contact->v1 = bv;

                if (contact->impulse_mag > 1.0f && contact->cull_dot < -0.15f) {
                    game->audio.start_ball_impact_sound = true;
                }
            }
        }
        profiler_pop_section();
    }

    {
        profiler_push_section("physics_resolve_contacts");
        bv = vec3_add(bv, vec3_scale(config_get_vec3("physics_gravity"), dt));
        bp = vec3_add(bp, vec3_scale(bv, dt));

        for (int i = 0; i < num_ball_contacts; i++) {
            struct ball_contact contact = ball_contacts[i];
            if (contact.is_ignored) {
                continue;
            }
            if (contact.is_environment && !ball->is_out_of_bounds) {
                ball->is_out_of_bounds = true;
                ball->time_out_of_bounds = 0.0f;
            }

            float pen = fmaxf(contact.penetration, 0.0f);
            vec3 correction = vec3_scale(contact.normal, pen*config_get_float("physics_correction_amount"));
            bp = vec3_add(bp, correction);
        }

        for (int i = 0; i < num_ball_contacts; i++) {
            struct ball_contact contact = ball_contacts[i];
            game_editor_push_physics_contact_data(ed, contact);
        }
        profiler_pop_section();
    }

    ball->in_water = false;
    for (int i = 0; i < game->physics.water_triangles.length; i++) {
        struct ball_contact contact;
        if (game_physics_create_ball_contact(game->physics.water_triangles.data[i], bp, bv, br, &contact)) {
            vec3 water_dir = game->physics.water_triangles_dir.data[i];
            vec3 water_vel = vec3_scale(water_dir, config_get_float("physics_water_max_speed"));
            bv = vec3_add(bv, vec3_scale(vec3_sub(water_vel, bv), config_get_float("physics_water_speed")*dt));
            ball->in_water = true;
            ball->time_out_of_water = 0.0f;
            break;
        }
    }
    if (!ball->in_water) {
        ball->time_out_of_water += dt;
    }

    float distance_to_hole = vec3_distance(ball->position, hole_pos);
    if (distance_to_hole < config_get_float("physics_hole_force_distance") && num_ball_contacts > 0) {
        float cup_force = game->physics.cup_force;
        bv = vec3_add(bv, vec3_scale(vec3_normalize(vec3_sub(hole_pos, ball->position)), cup_force));
    }

    if (vec3_length(bv) < config_get_float("physics_ball_slow_speed")) {
        ball->time_going_slow += dt;
    }
    else {
        ball->time_going_slow = 0.0f;
    }

    if (!ball->is_moving && vec3_length(bv) > config_get_float("physics_ball_slow_speed")) {
        ball->is_moving = true;
    }
    if (ball->is_moving) {
        ball->position = bp;
        ball->velocity = bv;
        ball->rotation_velocity = ball->rotation_velocity -
            dt*config_get_float("physics_ball_rotation_scale")*ball->rotation_velocity;
        ball->orientation = quat_multiply(
                quat_create_from_axis_angle(ball->rotation_vec, ball->rotation_velocity * dt),
                ball->orientation);
        if (ball->time_going_slow > config_get_float("physics_ball_slow_speed_time")) {
            game_editor_push_game_history_rest(ed, ball->position);
            ball->is_moving = false;
        }
    }

    {
        vec3 p = vec3_add(game->hole.cup_entity.position, game->hole.cup_entity.in_hole_delta);
        if (vec3_distance(p, ball->position) < game->hole.cup_entity.in_hole_radius) {
            ball->is_in_cup = true;
        }
    }

    // Check if ball fell through the map or got outside of the mountains 
    if ((ball->position.y < config_get_float("physics_out_of_bounds_y")) && !ball->is_out_of_bounds) {
        ball->is_out_of_bounds = true;
        ball->time_out_of_bounds = 0.0f;
    }

    game->physics.tick_idx++;
    profiler_section_add_var("num_contacts", num_ball_contacts);
    profiler_pop_section();
}

static void physics_update(struct game *game, float dt, struct renderer *renderer, 
        struct game_editor *ed) {
    profiler_push_section("physics_update");
    int num_ticks = 0;
    game->physics.time_behind += dt;

    struct ball_entity *player_ball = &game->player_ball;
    vec3 bp_prev = player_ball->position;
    while (game->physics.time_behind >= 0.0) {
        bp_prev = player_ball->position;
        game_physics_tick(game, game->physics.fixed_dt, renderer, ed);

        game->physics.time_behind -= game->physics.fixed_dt;
        num_ticks++;
    }
    float alpha = (float) (-game->physics.time_behind / game->physics.fixed_dt);
    vec3 bp_next = player_ball->position;

    vec3 draw_pos = vec3_add(vec3_scale(bp_next, 1.0f - alpha), vec3_scale(bp_prev, alpha));
    vec3 delta = vec3_sub(draw_pos, player_ball->draw_position);
    player_ball->draw_position = vec3_add(player_ball->draw_position, vec3_scale(delta, 0.5f));

    profiler_section_add_var("num_ticks", num_ticks);
    profiler_pop_section();
}

static void game_create_aim_line(struct game *game, float dt) {
    game->aim.num_line_points = 0;
    vec3 cur_point = game->player_ball.position;
    vec3 cur_direction = game->aim.direction;
    float t = 0.0f;
    float max_length = config_get_float("aim_line_min_length") +
        game->aim.power*config_get_float("aim_line_max_length");
    while (true) {
        if (game->aim.num_line_points == GAME_MAX_AIM_LINE_POINTS) {
            break;
        }

        int idx = game->aim.num_line_points++;
        game->aim.line_points[idx] = cur_point;

        if (t >= max_length) {
            break;
        }

        vec3 hit_tri_a, hit_tri_b, hit_tri_c;
        float hit_cor;

        float closest_t = FLT_MAX;
        for (int i = 0; i < game->physics.hole_triangles.length; i++) {
            struct physics_triangle triangle = game->physics.hole_triangles.data[i];
            vec3 points[3];
            points[0] = triangle.a;
            points[1] = triangle.b;
            points[2] = triangle.c;

            float t;
            int idx;
            if (ray_intersect_triangles(cur_point, cur_direction, points, 3, mat4_identity(), &t, &idx)) {
                if (t < closest_t) {
                    closest_t = t;
                    hit_tri_a = triangle.a;
                    hit_tri_b = triangle.b;
                    hit_tri_c = triangle.c;
                    hit_cor = triangle.cor;
                }
            }
        }
        for (int i = 0; i < game->hole.multi_terrain_entities.length; i++) {
            struct multi_terrain_entity *entity = &game->hole.multi_terrain_entities.data[i];
            struct terrain_model *model = &entity->moving_terrain_model;
            mat4 transform = multi_terrain_entity_get_moving_transform(entity, game->t);
            for (int i = 0; i < model->faces.length; i++) {
                struct terrain_model_face face = model->faces.data[i];
                assert(face.num_points == 3 || face.num_points == 4);

                {
                    vec3 points[3];
                    points[0] = vec3_apply_mat4(model->points.data[face.x], 1.0f, transform);
                    points[1] = vec3_apply_mat4(model->points.data[face.y], 1.0f, transform);
                    points[2] = vec3_apply_mat4(model->points.data[face.z], 1.0f, transform);

                    float t;
                    int idx;
                    if (ray_intersect_triangles(cur_point, cur_direction, points, 3, mat4_identity(), &t, &idx)) {
                        if (t < closest_t) {
                            closest_t = t;
                            hit_tri_a = points[0];
                            hit_tri_b = points[1];
                            hit_tri_c = points[2];
                            hit_cor = face.cor;
                        }
                    }
                }

                if (face.num_points == 4) {
                    vec3 points[3];
                    points[0] = vec3_apply_mat4(model->points.data[face.z], 1.0f, transform);
                    points[1] = vec3_apply_mat4(model->points.data[face.w], 1.0f, transform);
                    points[2] = vec3_apply_mat4(model->points.data[face.x], 1.0f, transform);

                    float t;
                    int idx;
                    if (ray_intersect_triangles(cur_point, cur_direction, points, 3, mat4_identity(), &t, &idx)) {
                        if (t < closest_t) {
                            closest_t = t;
                            hit_tri_a = points[0];
                            hit_tri_b = points[1];
                            hit_tri_c = points[2];
                            hit_cor = face.cor;
                        }
                    }
                }
            }
        }

        if (closest_t < FLT_MAX) {
            if (t + closest_t > max_length) {
                closest_t = max_length - t;
                t = max_length;
            }

            vec3 hit_normal = vec3_normalize(vec3_cross(vec3_sub(hit_tri_b, hit_tri_a), 
                        vec3_sub(hit_tri_c, hit_tri_a)));

            cur_point = vec3_add(cur_point, vec3_scale(cur_direction, closest_t));
            cur_point = vec3_add(cur_point, vec3_scale(cur_direction, -0.095f));
            cur_direction = vec3_reflect_with_restitution(cur_direction, hit_normal, hit_cor);
            t += closest_t;
        }
        else {
            cur_point = vec3_add(cur_point, vec3_scale(cur_direction, max_length - t));
            t = max_length;
        }
    }
    game->aim.line_offset.x += -config_get_float("aim_line_offset_delta")*dt;
}

static void game_update_camera(struct game *game, float dt, struct renderer *renderer, 
        struct button_inputs button_inputs) {
    if (button_inputs.button_down[SAPP_KEYCODE_A] || button_inputs.button_down[SAPP_KEYCODE_LEFT]) {
        game->cam.auto_rotate = false;
        game->cam.azimuth_angle += 1.0f * dt;
    }

    if (button_inputs.button_down[SAPP_KEYCODE_D] || button_inputs.button_down[SAPP_KEYCODE_RIGHT]) {
        game->cam.auto_rotate = false;
        game->cam.azimuth_angle -= 1.0f * dt;
    }

    float wanted_azimuth_angle;
    if (game->cam.auto_rotate &&
            game_get_camera_zone_angle(game, game->player_ball.draw_position, &wanted_azimuth_angle)) {
        float start_angle = fmodf(game->cam.azimuth_angle, 2.0f*MF_PI);
        float end_angle = fmodf(wanted_azimuth_angle, 2.0f*MF_PI);
        float delta_angle = end_angle - start_angle;
        if (delta_angle > MF_PI) {
            delta_angle = delta_angle - 2.0f*MF_PI;
        }
        if (delta_angle < -MF_PI) {
            delta_angle = delta_angle + 2.0f*MF_PI;
        }
        if (delta_angle < 0.0001f && delta_angle > -0.0001f) {
            game->cam.azimuth_angle = end_angle;
        } else {
            game->cam.azimuth_angle = game->cam.azimuth_angle + delta_angle*0.03f;
        }
    }

    vec3 cam_delta = vec3_rotate_y(config_get_vec3("game_cam_delta"), -game->cam.azimuth_angle);
    vec3 wanted_pos = vec3_add(game->player_ball.draw_position, cam_delta);
    vec3 diff = vec3_sub(wanted_pos, game->cam.pos);
    game->cam.pos = vec3_add(game->cam.pos, vec3_scale(diff, 0.5f));

    renderer->cam_pos = game->cam.pos;
    renderer->cam_dir = vec3_sub(game->player_ball.draw_position, game->cam.pos);
    renderer->cam_up = V3(0.0f, 1.0f, 0.0f);
}

static void game_update_player_ball(struct game *game, float dt, struct renderer *renderer,
        struct game_editor *ed) {
    struct ball_entity *player_ball = &game->player_ball;
    physics_update(game, dt, renderer, ed);

    //
    // Update Ball Impact Effects
    //
    if (game->audio.start_ball_impact_sound && game->audio.time_since_ball_impact_sound > 0.1f) {
        game->audio.time_since_ball_impact_sound = 0.0f;
        audio_start_sound("ball_impact", "footstep_grass_004.ogg", 1.0f, false, true);
    }
    game->audio.time_since_ball_impact_sound += dt;
    game->audio.start_ball_impact_sound = false;

    //
    // Update Water Effects
    //
    if (player_ball->time_out_of_water < 0.1f) {
        audio_start_sound("ball_in_water", "in_water.ogg", 0.1f, true, false);
    }
    else {
        audio_stop_sound("ball_in_water", 0.2f);
    }
    if (player_ball->in_water) {
        player_ball->time_since_water_ripple += dt;
        if (player_ball->time_since_water_ripple > config_get_float("water_ripple_frequency")) {
            player_ball->time_since_water_ripple = 0.0f;
            vec3 pos = player_ball->draw_position;
            pos.y -= player_ball->radius;
            pos.y += 0.011f;

            for (int i = 0; i < GAME_MAX_NUM_WATER_RIPPLES; i++) {
                if (game->drawing.water_ripples[i].t > game->drawing.water_ripple_t_length) {
                    game->drawing.water_ripples[i].t = 0.0f;
                    game->drawing.water_ripples[i].pos = pos;
                    break;
                }
            }
        }
    }
    else {
        player_ball->time_since_water_ripple = 1.0f;
    }
}

void game_update(struct game *game, float dt, struct button_inputs button_inputs,
        struct renderer *renderer, struct game_editor *ed) {
    profiler_push_section("game_update");
    game->t += dt;

    //
    // Update Water
    //
    {
        game->drawing.water_t += dt;
        for (int i = 0; i < GAME_MAX_NUM_WATER_RIPPLES; i++) {
            if (game->drawing.water_ripples[i].t > game->drawing.water_ripple_t_length) {
                continue;
            }
            game->drawing.water_ripples[i].t += dt;
        }
    }

    //
    // Update Blink Effect
    //
    {
        if (game->drawing.is_blink) {
            game->drawing.blink_t += dt;
            if (game->drawing.blink_t > game->drawing.blink_t_length) {
                game->drawing.is_blink = false;
            }
        }
    }

    // 
    // Update Scoreboard
    //
    {
        game->ui.is_scoreboard_open = button_inputs.button_down[SAPP_KEYCODE_TAB];
    }

    //
    // Update Game
    //
    game->aim.active = false;
    if (ed->free_camera) {
        if (button_inputs.button_down[SAPP_KEYCODE_W]) {
            game->cam.pos.x += 8.0f * dt * cosf(game->cam.azimuth_angle);
            game->cam.pos.z += 8.0f * dt * sinf(game->cam.azimuth_angle);
        }

        if (button_inputs.button_down[SAPP_KEYCODE_S]) {
            game->cam.pos.x -= 8.0f * dt * cosf(game->cam.azimuth_angle);
            game->cam.pos.z -= 8.0f * dt * sinf(game->cam.azimuth_angle);
        }

        if (button_inputs.button_down[SAPP_KEYCODE_D]) {
            game->cam.pos.x += 8.0f * dt * cosf(game->cam.azimuth_angle + 0.5f * MF_PI);
            game->cam.pos.z += 8.0f * dt * sinf(game->cam.azimuth_angle + 0.5f * MF_PI);
        }

        if (button_inputs.button_down[SAPP_KEYCODE_A]) {
            game->cam.pos.x -= 8.0f * dt * cosf(game->cam.azimuth_angle + 0.5f * MF_PI);
            game->cam.pos.z -= 8.0f * dt * sinf(game->cam.azimuth_angle + 0.5f * MF_PI);
        }

        if (button_inputs.button_down[SAPP_KEYCODE_E] && !button_inputs.button_down[SAPP_KEYCODE_LEFT_CONTROL]) {
            game->cam.pos.y += 8.0f * dt;
        }

        if (button_inputs.button_down[SAPP_KEYCODE_Q] && !button_inputs.button_down[SAPP_KEYCODE_LEFT_CONTROL]) {
            game->cam.pos.y -= 8.0f * dt;
        }

        if (button_inputs.button_down[SAPP_KEYCODE_LEFT]) {
            game->cam.azimuth_angle -= 1.0f * dt;
        }

        if (button_inputs.button_down[SAPP_KEYCODE_RIGHT]) {
            game->cam.azimuth_angle += 1.0f * dt;
        }

        if (button_inputs.button_down[SAPP_KEYCODE_DOWN]) {
            game->cam.inclination_angle += 1.0f * dt;
        }

        if (button_inputs.button_down[SAPP_KEYCODE_UP]) {
            game->cam.inclination_angle -= 1.0f * dt;
        }

        if (button_inputs.mouse_down[SAPP_MOUSEBUTTON_MIDDLE]) {
            game->cam.azimuth_angle += 0.2f * dt * button_inputs.mouse_delta.x;
            game->cam.inclination_angle -= 0.2f * dt * button_inputs.mouse_delta.y;
        }

        float theta = game->cam.inclination_angle;
        float phi = game->cam.azimuth_angle;
        renderer->cam_pos = game->cam.pos;
        renderer->cam_dir.x = sinf(theta)*cosf(phi);
        renderer->cam_dir.y = cosf(theta);
        renderer->cam_dir.z = sinf(theta)*sinf(phi);
        renderer->cam_up = V3(0.0f, 1.0f, 0.0f);
    }
    else if (game->state == GAME_STATE_MAIN_MENU) {
        game->cam.pos = game->hole.beginning_camera_animation_entity.start_position;
        renderer->cam_pos = game->cam.pos;
        renderer->cam_dir = vec3_normalize(vec3_sub(game->hole.cup_entity.position, game->cam.pos));
        renderer->cam_up = V3(0.0f, 1.0f, 0.0f);
    }
    else if (game->state == GAME_STATE_BEGIN_HOLE) {
        game->cam.pos = game->hole.beginning_camera_animation_entity.start_position;
        renderer->cam_pos = game->cam.pos;
        renderer->cam_dir = vec3_normalize(vec3_sub(game->hole.cup_entity.position, game->cam.pos));
        renderer->cam_up = V3(0.0f, 1.0f, 0.0f);

        float begin_hole_t_length = config_get_float("begin_hole_t_length");
        game->begin_hole.t += dt;
        if (game->begin_hole.t > begin_hole_t_length) {
            thread_join(load_hole_thread);
            thread_destroy(load_hole_thread);
            game->state = GAME_STATE_BEGINNING_CAMERA_ANIMATION;
        }
    }
    else if (game->state == GAME_STATE_BEGINNING_CAMERA_ANIMATION) {
        vec3 start_pos = game->beginning_cam_animation.start_pos;
        vec3 end_pos = game->beginning_cam_animation.end_pos;
        vec3 start_dir = game->beginning_cam_animation.start_dir;
        vec3 end_dir = game->beginning_cam_animation.end_dir;
        float t = game->beginning_cam_animation.t;
        float t_length = config_get_float("beginning_camera_animation_t_length");
        float a = t/t_length;
        if (a > 1.0f) {
            a = 1.0f;
        }
        a = sinf(0.5f*MF_PI*a);

        renderer->cam_pos = vec3_add(vec3_scale(start_pos, 1.0f - a), vec3_scale(end_pos, a));
        renderer->cam_dir = vec3_normalize(vec3_add(vec3_scale(start_dir, 1.0f - a), vec3_scale(end_dir, a)));
        renderer->cam_up = V3(0.0f, 1.0f, 0.0f);

        if (t >= t_length) {
            game_get_camera_zone_angle(game, game->player_ball.position, &game->cam.azimuth_angle);
            game->state = GAME_STATE_WAITING_FOR_AIM;
            game->cam.pos = renderer->cam_pos;
        }
        else {
            game->beginning_cam_animation.t += dt;
        }
    }
    else if (game->state == GAME_STATE_CELEBRATION) {
        vec3 start_pos = game->celebration_cam_animation.start_pos;
        vec3 end_pos = game->celebration_cam_animation.end_pos;
        vec3 start_dir = game->celebration_cam_animation.start_dir;
        vec3 end_dir = game->celebration_cam_animation.end_dir;

        float t = game->celebration_cam_animation.t;
        float t_length = config_get_float("celebration_t_length");
        float a = sinf(0.5f*MF_PI*t/t_length);

        renderer->cam_pos = vec3_add(start_pos, vec3_scale(vec3_sub(end_pos, start_pos), a));
        renderer->cam_dir = vec3_add(start_dir, vec3_scale(vec3_sub(end_dir, start_dir), a));
        renderer->cam_up = V3(0.0f, 1.0f, 0.0f);

        game->celebration_cam_animation.t += dt;
        if (game->celebration_cam_animation.t > t_length) {
            game->state = GAME_STATE_HOLE_COMPLETE;
            ui_button_update(&game->ui.next_hole_button, button_inputs);
        }
    }
    else if (game->state == GAME_STATE_HOLE_COMPLETE) {
        game->ui.scoreboard.hole_score[game->cur_hole] = game->player_ball.stroke_num;
        game->ui.is_scoreboard_open = true;
        game->ui.next_hole_button.pos = config_get_vec2("next_hole_button_pos");
        game->ui.next_hole_button.size = config_get_vec2("next_hole_button_size");
        ui_button_update(&game->ui.next_hole_button, button_inputs);
    }
    else if (game->state == GAME_STATE_WAITING_FOR_AIM) {
        game_update_camera(game, dt, renderer, button_inputs);
        game_update_player_ball(game, dt, renderer, ed);
        bool in_circle = vec2_distance(button_inputs.mouse_pos, game->aim.circle_pos) < game->aim.circle_radius;
        if (in_circle && button_inputs.mouse_clicked[SAPP_MOUSEBUTTON_LEFT]) {
            game->state = GAME_STATE_BEGINNING_AIM;
            game->aim.length = 0.0f;
            game->aim.power = 0.0f;
            renderer->sokol.game_aim.color = game->aim.green_color;
        }
        if (game->player_ball.is_moving) {
            game->state = GAME_STATE_SIMULATING_BALL;
        }
    }
    else if (game->state == GAME_STATE_BEGINNING_AIM) {
        game_update_camera(game, dt, renderer, button_inputs);
        game_update_player_ball(game, dt, renderer, ed);
        game->aim.active = true;
        game->aim.end_pos = button_inputs.mouse_pos;
        game->aim.length = vec2_distance(game->aim.end_pos, game->aim.circle_pos);
        game->aim.delta = vec2_normalize(vec2_sub(game->aim.end_pos, game->aim.circle_pos));
        game->aim.angle = acosf(vec2_dot(ed->game->aim.delta, V2(0.0f, -1.0f)));
        if (game->aim.delta.x < 0.0f) game->aim.angle *= -1.0f;
        if (game->aim.length > game->aim.min_power_length) {
            game->state = GAME_STATE_AIMING;
            game->aim.num_line_points = 0;
        }
        if (button_inputs.mouse_clicked[SAPP_MOUSEBUTTON_LEFT]) {
            game->state = GAME_STATE_WAITING_FOR_AIM;
        }
        if (game->player_ball.is_moving) {
            game->state = GAME_STATE_SIMULATING_BALL;
        }
    }
    else if (game->state == GAME_STATE_AIMING) {
        game_update_camera(game, dt, renderer, button_inputs);
        game_update_player_ball(game, dt, renderer, ed);
        game->aim.active = true;
        game->aim.end_pos = button_inputs.mouse_pos;
        game->aim.length = vec2_distance(game->aim.end_pos, game->aim.circle_pos);
        game->aim.delta = vec2_normalize(vec2_sub(game->aim.end_pos, game->aim.circle_pos));
        game->aim.angle = acosf(vec2_dot(ed->game->aim.delta, V2(0.0f, -1.0f)));
        if (game->aim.delta.x < 0.0f) game->aim.angle *= -1.0f;
        if (game->aim.length < game->aim.min_power_length) {
            game->state = GAME_STATE_BEGINNING_AIM;
        }
        if (game->aim.length > game->aim.max_power_length) {
            game->aim.length = game->aim.max_power_length;
            game->aim.end_pos = vec2_add(game->aim.circle_pos, vec2_scale(game->aim.delta, game->aim.length));
        }
        float power = (game->aim.length - game->aim.min_power_length) /
            (game->aim.max_power_length - game->aim.min_power_length);
        vec3 aim_direction = V3(-game->aim.delta.x, 0.0f, game->aim.delta.y);
        aim_direction = vec3_rotate_y(aim_direction, -game->cam.azimuth_angle + 0.5f * MF_PI);

        float min_angle = config_get_float("aim_rotate_min_angle");
        float max_angle = config_get_float("aim_rotate_max_angle");
        float rotate_speed = config_get_float("aim_rotate_speed");
        if (game->aim.angle > min_angle) {
            float alpha = 1.0f - (max_angle - game->aim.angle)/(max_angle - min_angle);
            if (alpha > 1.0f) alpha = 1.0f;
            game->cam.azimuth_angle -= rotate_speed*alpha*dt;
            game->cam.auto_rotate = false;
        }
        if (game->aim.angle < -min_angle) {
            float alpha = 1.0f + (-max_angle - game->aim.angle)/(max_angle - min_angle);
            if (alpha > 1.0f) alpha = 1.0f;
            game->cam.azimuth_angle += rotate_speed*alpha*dt;
            game->cam.auto_rotate = false;
        }
        game->aim.power = power;
        game->aim.direction = aim_direction;
        game_create_aim_line(game, dt);
        if (button_inputs.mouse_clicked[SAPP_MOUSEBUTTON_LEFT]) {
            game->state = GAME_STATE_SIMULATING_BALL;
            game_hit_player_ball(game, aim_direction, power, ed);
            game_editor_push_game_history_hit(ed, power, game->player_ball.position, aim_direction);
            game->cam.auto_rotate = true;
        }
        if (game->player_ball.is_moving) {
            game->state = GAME_STATE_SIMULATING_BALL;
        }
    }
    else if (game->state == GAME_STATE_SIMULATING_BALL) {
        game_update_camera(game, dt, renderer, button_inputs);
        game_update_player_ball(game, dt, renderer, ed);
        struct ball_entity *player_ball = &game->player_ball;
        if (!player_ball->is_moving && !player_ball->is_out_of_bounds) {
            game->state = GAME_STATE_WAITING_FOR_AIM;
            if (player_ball->was_hit) {
                player_ball->was_hit = false;
                player_ball->stroke_num++;
            }
        }
        if (player_ball->is_out_of_bounds) {
            player_ball->time_out_of_bounds += dt;
            if (player_ball->time_out_of_bounds > 1.0f) {
                player_ball->velocity = V3(0.0f, 0.0f, 0.0f);
                player_ball->position = player_ball->position_before_hit;
                game->cam.azimuth_angle = game->cam.azimuth_angle_before_hit;
                game->cam.auto_rotate = false;
                game->drawing.blink_t = 0.0f;
                game->drawing.is_blink = true;
                game->state = GAME_STATE_WAITING_FOR_AIM;
                player_ball->stroke_num++;
                player_ball->is_out_of_bounds = false;
                audio_start_sound("ball_out_of_bounds", "error_008.ogg", 1.0f, false, true);
            }
        }
        if (player_ball->is_in_cup) {
            game->celebration_cam_animation.start_pos = game->cam.pos;
            game->celebration_cam_animation.end_pos =
                vec3_add(game->cam.pos, vec3_scale(renderer->cam_dir, -1.5f));
            game->celebration_cam_animation.start_dir = renderer->cam_dir;
            game->celebration_cam_animation.end_dir =
                vec3_normalize(vec3_sub(game->hole.cup_entity.position,
                            game->celebration_cam_animation.end_pos));
            game->celebration_cam_animation.t = 0.0f;
            game->state = GAME_STATE_CELEBRATION;
            player_ball->is_in_cup = false;
            player_ball->velocity = V3(0.0f, 0.0f, 0.0f);
            player_ball->position = V3(1000.0f, -1000.f, 1000.0f);
            audio_start_sound("ball_in_hole", "confirmation_002.ogg", 1.0f, false, true);
        }
    }

    profiler_pop_section();
}

void game_init_ball_entity(struct ball_entity *entity) {
    entity->time_going_slow = 0.0f;
    entity->draw_position = V3(0.0f, 0.0f, 0.0f);
    entity->position = V3(0.0f, 0.0f, 0.0f);
    entity->velocity = V3(0.0f, 0.0f, 0.0f);
    entity->radius = config_get_float("physics_ball_radius");
    entity->was_hit = true;
    entity->is_moving = false;
    entity->is_out_of_bounds = false;
    entity->time_out_of_bounds = 0.0f;
    entity->is_in_cup = false;
    entity->orientation = quat_create_from_axis_angle(V3(1.0f, 0.0f, 0.0f), 0.0f);
    entity->rotation_vec = V3(0.0f, 0.0f, 0.0f);
    entity->rotation_velocity = 0.0f;
    entity->in_water = false;
    entity->time_out_of_water = 100.0f;
    entity->time_since_water_ripple = 0.0f;
    entity->stroke_num = 1;
    entity->color = config_get_vec3("game_ball_color");
}

void game_hit_player_ball(struct game *game, vec3 direction, float power, struct game_editor *ed) {
    struct ball_entity *ball = &game->player_ball;
    float speed = game->aim.min_power + sqrtf(power) * (game->aim.max_power - game->aim.min_power);
    ball->velocity = vec3_scale(direction, speed);
    ball->position_before_hit = ball->position;
    ball->was_hit = true;
    game->cam.azimuth_angle_before_hit = game->cam.azimuth_angle;
    ed->game->state = GAME_STATE_SIMULATING_BALL;
    game->physics.tick_idx = 0;
    audio_start_sound("hit_ball", "impactPlank_medium_000.ogg", 1.0f, false, true);

    ed->physics.selected_collision_data_idx = -1;
    ed->physics.collision_data_array.length = 0;
    game_editor_set_last_hit(ed, ball->position, direction, power);
}

struct physics_triangle physics_triangle_create(vec3 a, vec3 b, vec3 c, float cor, float friction, 
        float vel_scale) {
    struct physics_triangle tri;
    tri.a = a;
    tri.b = b;
    tri.c = c;
    tri.cor = cor;
    tri.friction = friction;
    tri.vel_scale = vel_scale;
    return tri;
}
