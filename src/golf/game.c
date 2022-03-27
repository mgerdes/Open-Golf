#include "golf/game.h"

#include <assert.h>
#include <float.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"

#include "common/audio.h"
#include "common/common.h"
#include "common/data.h"
#include "common/debug_console.h"
#include "common/graphics.h"
#include "common/inputs.h"
#include "common/log.h"
#include "common/storage.h"
#include "golf/golf.h"

static golf_game_t game;
static golf_t *golf;
static golf_graphics_t *graphics;
static golf_inputs_t *inputs;
static golf_config_t *game_cfg;

golf_game_t *golf_game_get(void) {
    return &game;
}

static void _golf_game_debug_tab(void) {
    static const char *contact_type_string[] = {
        "Point A", "Point B", "Point C",
        "Edge AB", "Edge AC", "Edge BC",
        "Face",
    };

    igCheckbox("Debug draw collisions", &game.physics.debug_draw_collisions);
    for (int i = 0; i < game.physics.collision_history.length; i++) {
        golf_collision_data_t *collision = &game.physics.collision_history.data[i]; 
        collision->is_highlighted = false;
        if (igTreeNodeEx_Ptr((void*)(intptr_t)i, ImGuiTreeNodeFlags_None, "Collision %d", i)) {
            collision->is_highlighted = true;
            for (int i = 0; i < collision->num_contacts; i++) {
                golf_ball_contact_t contact = collision->contacts[i];
                igText("Contact %d", i);
                igText("    Type: %s", contact_type_string[contact.type]);
                igText("    Ignored: %d", contact.is_ignored);
                igText("    Penetration: %0.2f", contact.penetration);
                igText("    Impulse Magnitude: %0.2f", contact.impulse_mag);
                igText("    Impulse: <%0.2f, %0.2f, %0.2f>", 
                        contact.impulse.x, contact.impulse.y, contact.impulse.z);
                igText("    Restitution: %0.2f", contact.restitution); 
                igText("    Velocity Scale: %0.2f", contact.vel_scale);
                igText("    Start Speed: %0.2f", vec3_length(contact.v0));
                igText("    End Speed: %0.2f", vec3_length(contact.v1));
                igText("    Start Velocity: <%0.2f, %0.2f, %0.2f>", 
                        contact.v0.x, contact.v0.y, contact.v0.z);
                igText("    End Velocity: <%0.2f, %0.2f, %0.2f>", 
                        contact.v1.x, contact.v1.y, contact.v1.z);
                igText("    Cull Dot: %0.2f", contact.cull_dot);
                igText("    Position: <%0.2f, %0.2f, %0.2f>",
                        contact.position.x, contact.position.y, contact.position.z);
                igText("    Normal: <%0.2f, %0.2f, %0.2f>",
                        contact.normal.x, contact.normal.y, contact.normal.z);
                igText("    Triangle Normal: <%0.2f, %0.2f, %0.2f>",
                        contact.triangle_normal.x, contact.triangle_normal.y, contact.triangle_normal.z);
            }
            igTreePop();
        }
        if (igIsItemHovered(ImGuiHoveredFlags_None)) {
            collision->is_highlighted = true;
        }
    }
}

static float _golf_game_get_camera_zone_angle(vec3 pos) {
    golf_camera_zone_entity_t camera_zone;
    if (golf_level_get_camera_zone(golf->level, pos, &camera_zone)) {
        vec3 camera_zone_dir;
        if (camera_zone.towards_hole) {
            camera_zone_dir = vec3_sub(game.hole_pos, game.ball.draw_pos);
            camera_zone_dir.y = 0;
            camera_zone_dir = vec3_normalize(camera_zone_dir);
        }
        else {
            camera_zone_dir = vec3_apply_quat(V3(1, 0, 0), 0, camera_zone.transform.rotation);
        }

        float camera_zone_angle = acosf(camera_zone_dir.x);
        if (camera_zone_dir.z > 0) camera_zone_angle *= -1;
        camera_zone_angle += MF_PI;

        return camera_zone_angle;
    }

    return game.cam.angle;
}

void golf_game_init(void) {
    memset(&game, 0, sizeof(game));

    golf_data_load("data/config/game.cfg", false);

    golf = golf_get();
    graphics = golf_graphics_get();
    inputs = golf_inputs_get();
    game_cfg = golf_data_get_config("data/config/game.cfg");

    game.state = GOLF_GAME_STATE_MAIN_MENU;
    game.cam.auto_rotate = true;
    game.cam.angle = 0;
    game.cam.angle_velocity = 0;

    game.ball.pos = V3(0, 0, 0);
    game.ball.draw_pos = V3(0, 0, 0);
    game.ball.vel = V3(0, 0, 0);
    game.ball.rot_vec = V3(0, 0, 0);
    game.ball.orientation = QUAT(0, 0, 0, 1);
    game.ball.radius = 0.12f;
    game.ball.time_going_slow = 0;
    game.ball.time_since_water_ripple = 0;
    game.ball.rot_vel = 0;
    game.ball.is_moving = false;
    game.ball.is_out_of_bounds = false;
    game.ball.is_in_hole = false;
    game.ball.time_since_impact_sound = 0;
    game.ball.time_out_of_water = 1;

    game.physics.time_behind = 0;
    game.physics.debug_draw_collisions = false;
    vec_init(&game.physics.collision_history, "physics");

    golf_bvh_init(&game.physics.static_bvh);
    golf_bvh_init(&game.physics.dynamic_bvh);

    game.aim_line.power = 0;
    game.aim_line.aim_delta = V2(0, 0);
    game.aim_line.offset = V2(0, 0);
    game.aim_line.num_points = 0;

    graphics->cam_pos = V3(5, 5, 5);
    graphics->cam_dir = vec3_normalize(V3(-5, -5, -5));
    graphics->cam_up = V3(0, 1, 0);

    for (int i = 0; i < MAX_NUM_WATER_RIPPLES; i++) {
        game.water_ripples[i].t0 = FLT_MAX;

        vec4 color = V4(0, 0, 0, 0);
        if (i % 4 == 0) {
            color = CFG_VEC4(game_cfg, "water_ripple_color_0");
        }
        else if (i % 4 == 1) {
            color = CFG_VEC4(game_cfg, "water_ripple_color_1");
        }
        else if (i % 4 == 2) {
            color = CFG_VEC4(game_cfg, "water_ripple_color_2");
        }
        else if (i % 4 == 3) {
            color = CFG_VEC4(game_cfg, "water_ripple_color_3");
        }
        game.water_ripples[i].color = color;
    }

    game.t = 0;

    golf_debug_console_add_tab("Game", _golf_game_debug_tab);
}

static void _golf_game_update_state_main_menu(float dt) {
    GOLF_UNUSED(dt);
}

static void _golf_game_update_state_waiting_for_aim(float dt) {
    GOLF_UNUSED(dt);

    if (game.ball.is_moving) {
        game.state = GOLF_GAME_STATE_WATCHING_BALL;
    }
}

static void _golf_game_update_state_aiming(float dt) {
    vec3 aim_direction = V3(game.aim_line.aim_delta.x, 0, game.aim_line.aim_delta.y);
    aim_direction = vec3_normalize(vec3_rotate_y(aim_direction, game.cam.angle - 0.5f * MF_PI));

    // Create aim line
    {
        game.aim_line.offset.x += -3 * dt;
        game.aim_line.num_points = 0;
        vec3 cur_point = game.ball.pos;
        vec3 cur_dir = aim_direction;
        float min_length = CFG_NUM(game_cfg, "aim_line_min_length");
        float max_length = CFG_NUM(game_cfg, "aim_line_max_length");
        max_length = min_length + game.aim_line.power * (max_length - min_length);
        float t = 0;
        while (true) {
            if (game.aim_line.num_points == MAX_AIM_LINE_POINTS) break;
            int idx = game.aim_line.num_points++;
            game.aim_line.points[idx] = cur_point;
            if (t >= max_length) break;

            golf_bvh_face_t static_hit_face;
            float static_hit_t = FLT_MAX;
            int static_hit_idx;
            golf_bvh_ray_test(&game.physics.static_bvh, cur_point, cur_dir, &static_hit_t, &static_hit_idx, &static_hit_face);

            golf_bvh_face_t dynamic_hit_face;
            float dynamic_hit_t = FLT_MAX;
            int dynamic_hit_idx;
            golf_bvh_ray_test(&game.physics.dynamic_bvh, cur_point, cur_dir, &dynamic_hit_t, &dynamic_hit_idx, &dynamic_hit_face);

            golf_bvh_face_t hit_face;
            float hit_t = FLT_MAX;

            if (static_hit_t < dynamic_hit_t) {
                hit_face = static_hit_face;
                hit_t = static_hit_t;
            }
            else if (dynamic_hit_t < static_hit_t) {
                hit_face = dynamic_hit_face;
                hit_t = dynamic_hit_t;
            }

            if (hit_t < FLT_MAX) {
                if (t + hit_t > max_length) {
                    hit_t = max_length - t;
                    t = max_length;
                }
                
                vec3 normal = vec3_normalize(vec3_cross(vec3_sub(hit_face.b, hit_face.a), vec3_sub(hit_face.c, hit_face.a)));
                cur_point = vec3_add(cur_point, vec3_scale(cur_dir, hit_t));
                cur_point = vec3_add(cur_point, vec3_scale(cur_dir, -0.095f));
                cur_dir = vec3_reflect_with_restitution(cur_dir, normal, 1);
                t += hit_t;
            }
            else {
                cur_point = vec3_add(cur_point, vec3_scale(cur_dir, max_length - t));
                t = max_length;
            }
        }
    }

    if (game.ball.is_moving) {
        game.state = GOLF_GAME_STATE_WATCHING_BALL;
    }
}

static void _golf_game_update_state_watching_ball(float dt) {
    GOLF_UNUSED(dt);

    if (game.ball.is_in_hole) {
        game.ball.vel = V3(0, 0, 0);
        game.ball.is_moving = false;

        game.state = GOLF_GAME_STATE_CELEBRATION;
        game.celebration.t = 0;
        game.celebration.cam_pos0 = graphics->cam_pos;
        game.celebration.cam_dir0 = graphics->cam_dir;
        game.celebration.cam_pos1 = vec3_add(graphics->cam_pos, vec3_scale(graphics->cam_dir, -1.5f));
        game.celebration.cam_dir1 = vec3_normalize(vec3_sub(game.ball.draw_pos, game.celebration.cam_pos1));

        {
            char storage_key[256];
            snprintf(storage_key, 256, "stroke_count_level_%d", golf->level_num);

            float storage_stroke_count;
            bool is_key_set = golf_storage_get_num(storage_key, &storage_stroke_count); 
            if (!is_key_set || game.stroke_count < (int)storage_stroke_count) {
                golf_storage_set_num(storage_key, (float)game.stroke_count);
            }
            golf_storage_save();
        }

        golf_audio_start_sound("ball_in_hole", "data/audio/confirmation_002.ogg", 1, false, true);
    }
    else if (game.ball.is_out_of_bounds) {
        game.ball.pos = game.ball.start_pos;
        game.ball.draw_pos = game.ball.pos;
        game.ball.is_moving = false;
        game.ball.vel = V3(0, 0, 0);
        game.ball.rot_vel = 0;
        game.ball.orientation = QUAT(0, 0, 0, 1);
        game.ball.is_out_of_bounds = 0;
        game.cam.angle = game.cam.start_angle;
        game.cam.auto_rotate = false;
        game.state = GOLF_GAME_STATE_WAITING_FOR_AIM;
        golf_audio_start_sound("ball_out_of_bounds", "data/audio/error_008.ogg", 1, false, true);
    }
    else if (!game.ball.is_moving) {
        game.state = GOLF_GAME_STATE_WAITING_FOR_AIM;
    }
}

int _ball_contact_cmp(const void *a, const void *b) {
    const golf_ball_contact_t *bc0 = (golf_ball_contact_t*)a;
    const golf_ball_contact_t *bc1 = (golf_ball_contact_t*)b;

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

static void _physics_tick(float dt) {
    float EPS = 0.001f;

    vec3 bp = game.ball.pos;
    float br = game.ball.radius;
    vec3 bv = game.ball.vel;
    float bs = vec3_length(bv);
    vec3 bp0 = bp;
    vec3 bv0 = bv;

    float dist_to_hole = FLT_MAX;
    vec3 dir_to_hole = V3(0, 0, 0);
    vec3 hole_pos = V3(0, 0, 0);
    golf_entity_t *close_hole = NULL;
    for (int i = 0; i < golf->level->entities.length; i++) {
        golf_entity_t *entity = &golf->level->entities.data[i];
        if (entity->type == HOLE_ENTITY) {
            vec3 hp = entity->hole.transform.position;
            vec3 hs = entity->hole.transform.scale;
            float dist = vec3_distance(hp, bp);
            if (dist <= hs.x) {
                close_hole = entity;
            }
            if (dist < dist_to_hole) {
                dist_to_hole = dist;
                dir_to_hole = vec3_normalize(vec3_sub(hp, bp));
                hole_pos = hp;
            }
        }
    }

    // Create the BVH for entities that move
    {
        golf_bvh_t *bvh = &game.physics.dynamic_bvh;
        bvh->node_infos.length = 0;
        for (int i = 0; i < golf->level->entities.length; i++) {
            golf_entity_t *entity = &golf->level->entities.data[i];

            switch (entity->type) {
                case BEGIN_ANIMATION_ENTITY:
                case CAMERA_ZONE_ENTITY:
                case MODEL_ENTITY:
                case WATER_ENTITY:
                case GEO_ENTITY: {
                    golf_movement_t *movement = golf_entity_get_movement(entity);
                    if (movement && movement->type != GOLF_MOVEMENT_NONE) {
                        vec_push(&bvh->node_infos, golf_bvh_node_info(bvh, i, golf->level, entity, game.t));
                    }
                    break;
                }
                case BALL_START_ENTITY:
                case HOLE_ENTITY:
                case GROUP_ENTITY:
                    break;
            }
        }
        golf_bvh_construct(bvh, bvh->node_infos);
    }

    int num_contacts = 0;
    golf_ball_contact_t contacts[MAX_NUM_CONTACTS];
    if (close_hole) {
        golf_model_t *model = golf_entity_get_model(close_hole);
        golf_transform_t transform = golf_entity_get_world_transform(golf->level, close_hole);
        mat4 model_mat = golf_transform_get_model_mat(transform);
        for (int i = 0; i < model->positions.length; i += 3) {
            vec3 a = vec3_apply_mat4(model->positions.data[i + 0], 1, model_mat);
            vec3 b = vec3_apply_mat4(model->positions.data[i + 1], 1, model_mat);
            vec3 c = vec3_apply_mat4(model->positions.data[i + 2], 1, model_mat);
            triangle_contact_type_t type;
            vec3 cp = closest_point_point_triangle(bp, a, b, c, &type);
            float dist = vec3_distance(bp, cp);
            if (dist < br) {
                float restitution, friction, vel_scale;
                if (type == TRIANGLE_CONTACT_AB || type == TRIANGLE_CONTACT_AC || type == TRIANGLE_CONTACT_BC) {
                    restitution = 0.4f;
                    if (bs > 2) {
                        friction = 1;
                        vel_scale = 0.95f;
                    }
                    else {
                        friction = 0;
                        vel_scale = 1;
                    }
                }
                else {
                    restitution = 0.5f;
                    friction = 0.5f;
                    vel_scale = 1;
                }
                if (num_contacts < MAX_NUM_CONTACTS) {
                    vec3 vel = V3(0, 0, 0);
                    golf_ball_contact_t contact = golf_ball_contact(a, b, c, vel, bp, br, cp, dist, restitution, friction, vel_scale, type, false, V3(0, 0, 0), false);
                    contacts[num_contacts] = contact;
                    num_contacts = num_contacts + 1;
                }
            }
        }
    }
    else {
        golf_bvh_ball_test(&game.physics.static_bvh, bp, br, bv, contacts, &num_contacts, MAX_NUM_CONTACTS);
        golf_bvh_ball_test(&game.physics.dynamic_bvh, bp, br, bv, contacts, &num_contacts, MAX_NUM_CONTACTS);
    }
    qsort(contacts, num_contacts, sizeof(golf_ball_contact_t), _ball_contact_cmp);

    // Apply a force to pull the ball towards the hole
    if (dist_to_hole < CFG_NUM(game_cfg, "physics_hole_force_distance") && num_contacts > 0) {
        float hole_force = CFG_NUM(game_cfg, "physics_hole_force");
        bv = vec3_add(bv, vec3_scale(dir_to_hole, hole_force));
    }

    // Filter out the contacts
    {
        int num_processed_vertices = 0;
        vec3 processed_vertices[9 * MAX_NUM_CONTACTS];

        // All face contacts are used
        for (int i = 0; i < num_contacts; i++) {
            golf_ball_contact_t *contact = &contacts[i];
            if (contact->is_ignored || contact->type != TRIANGLE_CONTACT_FACE) {
                continue;
            }

            processed_vertices[num_processed_vertices++] = contact->triangle_a;
            processed_vertices[num_processed_vertices++] = contact->triangle_b;
            processed_vertices[num_processed_vertices++] = contact->triangle_c;
        }

        // Remove unecessary edge contacts
        for (int i = 0; i < num_contacts; i++) {
            golf_ball_contact_t *contact = &contacts[i];
            if (contact->is_ignored || 
                    (contact->type != TRIANGLE_CONTACT_AB && 
                     contact->type != TRIANGLE_CONTACT_AC &&
                     contact->type != TRIANGLE_CONTACT_BC)) {
                continue;
            }

            vec3 e0 = V3(0, 0, 0);
            vec3 e1 = V3(0, 0, 0);
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
                if (vec3_line_segments_on_same_line(a, b, e0, e1, EPS) ||
                        vec3_line_segments_on_same_line(a, c, e0, e1, EPS) ||
                        vec3_line_segments_on_same_line(b, c, e0, e1, EPS)) {
                    contact->is_ignored = true;
                    break;
                }
            }

            processed_vertices[num_processed_vertices++] = contact->triangle_a;
            processed_vertices[num_processed_vertices++] = contact->triangle_b;
            processed_vertices[num_processed_vertices++] = contact->triangle_c;
        }

        // Remove uncessary point contacts
        for (int i = 0; i < num_contacts; i++) {
            golf_ball_contact_t *contact = &contacts[i];
            if (contact->is_ignored ||
                    (contact->type != TRIANGLE_CONTACT_A && 
                     contact->type != TRIANGLE_CONTACT_B &&
                     contact->type != TRIANGLE_CONTACT_C)) {
                continue;
            }

            vec3 p = V3(0, 0, 0);
            if (contact->type == TRIANGLE_CONTACT_A) {
                p = contact->triangle_a;
            }
            else if (contact->type == TRIANGLE_CONTACT_B) {
                p = contact->triangle_b;
            }
            else if (contact->type == TRIANGLE_CONTACT_C) {
                p = contact->triangle_c;
            }

            for (int j = 0; j < num_processed_vertices; j++) {
                vec3 a = processed_vertices[j + 0];
                vec3 b = processed_vertices[j + 1];
                vec3 c = processed_vertices[j + 2];
                if (vec3_point_on_line_segment(p, a, b, EPS) ||
                        vec3_point_on_line_segment(p, a, c, EPS) ||
                        vec3_point_on_line_segment(p, b, c, EPS)) {
                    contact->is_ignored = true;
                    break;
                }
            }

            processed_vertices[num_processed_vertices++] = contact->triangle_a;
            processed_vertices[num_processed_vertices++] = contact->triangle_b;
            processed_vertices[num_processed_vertices++] = contact->triangle_c;
        }
    }

    for (int i = 0; i < num_contacts; i++) {
        golf_ball_contact_t *contact = &contacts[i];
        if (contact->is_ignored) {
            continue;
        }
        if (contact->is_water) {
            continue;
        }

        vec3 n = contact->normal;
        vec3 vr = vec3_sub(bv, contact->velocity);
        contact->cull_dot = vec3_dot(n, vec3_normalize(vr));
        if (contact->cull_dot > EPS) {
            contact->is_ignored = true;
            continue;
        }

        float e = contact->restitution;
        float v_scale = contact->vel_scale;
        float imp = -(1 + e) * vec3_dot(vr, n);

        contact->impulse_mag = imp; 
        contact->impulse = vec3_scale(n, imp);
        contact->v0 = bv0;

        bv = vec3_add(bv, contact->impulse);
        bv = vec3_scale(bv, v_scale);

        game.ball.rot_vel = vec3_length(bv) / (MF_PI * game.ball.radius);
        game.ball.rot_vec = vec3_normalize(vec3_cross(n, bv));

        vec3 t = vec3_sub(bv, vec3_scale(n, vec3_dot(bv, n)));
        if (vec3_length(t) > EPS) {
            t = vec3_normalize(t);

            float jt = -vec3_dot(vr, t);
            if (fabsf(jt) > EPS) {
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

        if (contact->impulse_mag > 1 && contact->cull_dot < -0.15f) {
            if (game.ball.time_since_impact_sound > 0.1f) {
                golf_audio_start_sound("ball_impact", "data/audio/footstep_grass_004.ogg", 1, false, true);
                game.ball.time_since_impact_sound = 0;
            }
        }
    }
    game.ball.time_since_impact_sound += dt;

    float gravity = -9.8f;
    bv = vec3_add(bv, V3(0, gravity * dt, 0));
    bp = vec3_add(bp, vec3_scale(bv, dt));

    for (int i = 0; i < num_contacts; i++) {
        golf_ball_contact_t *contact = &contacts[i];
        if (contact->is_ignored) {
            continue;
        }
        if (contact->is_water) {
            continue;
        }

        float pen = fmaxf(contact->penetration, 0);
        vec3 correction = vec3_scale(contact->normal, pen * 0.5f);
        bp = vec3_add(bp, correction);
    }

    game.ball.is_in_water = false;
    for (int i = 0; i < num_contacts; i++) {
        golf_ball_contact_t *contact = &contacts[i];
        if (contact->is_ignored) {
            continue;
        }
        if (!contact->is_water) {
            continue;
        }

        vec3 water_dir = contact->water_dir;
        vec3 water_vel = vec3_scale(water_dir, CFG_NUM(game_cfg, "physics_water_max_speed"));
        bv = vec3_add(bv, vec3_scale(vec3_sub(water_vel, bv), CFG_NUM(game_cfg, "physics_water_speed") * dt));
        game.ball.is_in_water = true;
    }

    if (game.ball.is_moving && num_contacts > 0) {
        golf_collision_data_t collision;
        collision.num_contacts = num_contacts;
        for (int i = 0; i < num_contacts; i++) {
            collision.contacts[i] = contacts[i];
        }
        collision.ball_pos = bp0;
        collision.is_highlighted = false;
        vec_push(&game.physics.collision_history, collision);
    }

    if (vec3_length(bv) < 0.1f) {
        game.ball.time_going_slow += dt;
    }
    else {
        game.ball.time_going_slow = 0.0f;
    }

    if (!game.ball.is_moving && vec3_length(bv) > 0.1f) {
        game.ball.is_moving = true;
    }
    if (game.ball.is_moving) {
        game.ball.pos = bp;
        game.ball.vel = bv;
        game.ball.rot_vel = game.ball.rot_vel - dt * game.ball.rot_vel * CFG_NUM(game_cfg, "physics_ball_rot_scale");
        game.ball.orientation = quat_multiply(
                quat_create_from_axis_angle(game.ball.rot_vec, game.ball.rot_vel * dt),
                game.ball.orientation);
        if (game.ball.time_going_slow > 0.5f) {
            game.ball.is_moving = false;
        }
    }

    {
        // Check to see if the ball ended up in the hole
        vec3 p = vec3_add(hole_pos, CFG_VEC3(game_cfg, "physics_in_hole_delta"));
        if (vec3_distance(p, bp) < CFG_NUM(game_cfg, "physics_in_hole_radius")) {
            game.ball.is_in_hole = true;
        }

        // Check to see if the ball ended up out of bounds
        for (int i = 0; i < num_contacts; i++) {
            golf_ball_contact_t *contact = &contacts[i];
            if (contact->is_out_of_bounds) {
                game.ball.time_out_of_bounds = game.t;
                game.ball.is_out_of_bounds = true;
            }
        }
    }

    if (game.ball.is_in_water) {
        game.ball.time_out_of_water = 0;
        game.ball.time_since_water_ripple += dt;
        if (game.ball.time_since_water_ripple > CFG_NUM(game_cfg, "water_ripple_frequency")) {
            game.ball.time_since_water_ripple = 0;
            
            vec3 pos = game.ball.draw_pos;
            pos.y -= game.ball.radius;
            pos.y += 0.02f;

            for (int i = 0; i < MAX_NUM_WATER_RIPPLES; i++) {
                if (game.water_ripples[i].t0 < FLT_MAX) {
                    continue;
                }

                game.water_ripples[i].t0 = game.t;
                game.water_ripples[i].pos = pos;
                break;
            }
        }
    }
    else {
        game.ball.time_out_of_water += dt;
    }

    if (game.ball.pos.y < CFG_NUM(game_cfg, "physics_kill_y")) {
        game.ball.time_out_of_bounds = game.t;
        game.ball.is_out_of_bounds = true;
    }

    if (game.ball.time_out_of_water < 0.1f) {
        golf_audio_start_sound("ball_in_water", "data/audio/in_water.ogg", 0.1f, true, false);
    }
    else {
        golf_audio_stop_sound("ball_in_water", 0.2f);
    }
}

void golf_game_update(float dt) {
    if (game.state == GOLF_GAME_STATE_PAUSED) {
        return;
    }

    game.t += dt;

    switch (game.state) {
        case GOLF_GAME_STATE_MAIN_MENU:
            _golf_game_update_state_main_menu(dt);
            break;
        case GOLF_GAME_STATE_WAITING_FOR_AIM:
            _golf_game_update_state_waiting_for_aim(dt);
            break;
        case GOLF_GAME_STATE_AIMING:
            _golf_game_update_state_aiming(dt);
            break;
        case GOLF_GAME_STATE_WATCHING_BALL:
            _golf_game_update_state_watching_ball(dt);
            break;
        case GOLF_GAME_STATE_BEGIN_CAMERA_ANIMATION:
        case GOLF_GAME_STATE_CELEBRATION:
        case GOLF_GAME_STATE_FINISHED:
        case GOLF_GAME_STATE_PAUSED:
            break;
    }

    {
        // Remove any water ripples that have finished
        float time_length = CFG_NUM(game_cfg, "water_ripple_time_length"); 
        for (int i = 0; i < MAX_NUM_WATER_RIPPLES; i++) {
            if (game.water_ripples[i].t0 == FLT_MAX) {
                continue;
            }

            float dt = game.t - game.water_ripples[i].t0;
            if (dt > time_length) {
                game.water_ripples[i].t0 = FLT_MAX;
            }
        }
    }

    if (game.state > GOLF_GAME_STATE_MAIN_MENU) {
        float physics_dt = 1.0f/120.0f;
        game.physics.time_behind += dt;

        vec3 bp_prev = game.ball.pos;
        int num_ticks = 0;
        while (game.physics.time_behind >= 0 && num_ticks < 5) {
            bp_prev = game.ball.pos;
            _physics_tick(physics_dt);
            game.physics.time_behind -= physics_dt;
            num_ticks++;
        }
        while (game.physics.time_behind >= 0) {
            game.physics.time_behind -= physics_dt;
        }

        float alpha = (float)(-game.physics.time_behind / physics_dt);
        game.ball.draw_pos = vec3_add(vec3_scale(game.ball.pos, 1.0f - alpha), vec3_scale(bp_prev, alpha));
    }

    // Move around the camera
    switch (game.state) {
        case GOLF_GAME_STATE_BEGIN_CAMERA_ANIMATION: {
            vec3 cam_pos0 = game.begin_camera_animation.cam_pos0;
            vec3 cam_pos1 = game.begin_camera_animation.cam_pos1;
            vec3 cam_dir0 = game.begin_camera_animation.cam_dir0;
            vec3 cam_dir1 = game.begin_camera_animation.cam_dir1;
            float t = game.begin_camera_animation.t;
            float length0 = CFG_NUM(game_cfg, "begin_camera_animation_length0");
            float length1 = CFG_NUM(game_cfg, "begin_camera_animation_length1");

            if (t >= length0) {
                t = t - length0;
                float a = sinf(0.5f * MF_PI * t / length1);

                graphics->cam_pos = vec3_add(vec3_scale(cam_pos0, 1 - a), vec3_scale(cam_pos1, a));
                graphics->cam_dir = vec3_normalize(vec3_add(vec3_scale(cam_dir0, 1 - a), vec3_scale(cam_dir1, a)));

                if (t >= length1) {
                    game.state = GOLF_GAME_STATE_WAITING_FOR_AIM;
                    graphics->cam_pos = cam_pos1;
                    graphics->cam_dir = cam_dir1;
                }
            }

            game.begin_camera_animation.t += dt;
            break;
        }
        case GOLF_GAME_STATE_CELEBRATION: {
            vec3 cam_pos0 = game.celebration.cam_pos0;
            vec3 cam_pos1 = game.celebration.cam_pos1;
            vec3 cam_dir0 = game.celebration.cam_dir0;
            vec3 cam_dir1 = game.celebration.cam_dir1;

            float t = game.celebration.t;
            float length = CFG_NUM(game_cfg, "celebration_length");
            float a = sinf(0.5f * MF_PI * t / length);

            graphics->cam_pos = vec3_add(cam_pos0, vec3_scale(vec3_sub(cam_pos1, cam_pos0), a));
            graphics->cam_dir = vec3_add(cam_dir0, vec3_scale(vec3_sub(cam_dir1, cam_dir0), a));

            if (t >= length) {
                game.state = GOLF_GAME_STATE_FINISHED;
            }

            game.celebration.t += dt;
            break;
        }
        case GOLF_GAME_STATE_WAITING_FOR_AIM:
        case GOLF_GAME_STATE_AIMING:
        case GOLF_GAME_STATE_WATCHING_BALL: {
            if (game.cam.auto_rotate) {
                float camera_zone_angle = _golf_game_get_camera_zone_angle(game.ball.draw_pos);
                float delta_angle = camera_zone_angle - game.cam.angle;
                delta_angle = atan2f(sinf(delta_angle), cosf(delta_angle));
                game.cam.angle += delta_angle * CFG_NUM(game_cfg, "cam_auto_rotate_speed");
            }

            vec3 cam_delta = vec3_rotate_y(V3(2.6f, 1.5f, 0), game.cam.angle);
            vec3 wanted_pos = vec3_add(game.ball.draw_pos, cam_delta);
            vec3 diff = vec3_sub(wanted_pos, graphics->cam_pos);
            graphics->cam_pos = vec3_add(graphics->cam_pos, vec3_scale(diff, 0.5f));
            graphics->cam_dir = vec3_normalize(vec3_sub(vec3_add(game.ball.draw_pos, V3(0, 0.3f, 0)), graphics->cam_pos));
            break;
        }
        case GOLF_GAME_STATE_MAIN_MENU:
        case GOLF_GAME_STATE_PAUSED:
        case GOLF_GAME_STATE_FINISHED: 
            break;
    }
}

void golf_game_start_main_menu(void) {
    game.state = GOLF_GAME_STATE_MAIN_MENU;

    vec3 hole_pos = V3(0, 0, 0);;
    vec3 begin_animation_pos = V3(0, 0, 0);

    golf_level_t *level = golf->level;
    for (int i = 0; i < level->entities.length; i++) {
        golf_entity_t *entity = &level->entities.data[i];
        switch (entity->type) {
            case CAMERA_ZONE_ENTITY:
            case MODEL_ENTITY:
            case GEO_ENTITY:
            case GROUP_ENTITY:
            case WATER_ENTITY:
            case BALL_START_ENTITY:
                break;
            case HOLE_ENTITY:
                hole_pos = entity->hole.transform.position;
                break;
            case BEGIN_ANIMATION_ENTITY:
                begin_animation_pos = entity->begin_animation.transform.position;
                break;
        }
    }

    game.ball.pos = V3(99999.0f, 99999.0f, 99999.0f);
    game.ball.draw_pos = game.ball.pos;
    game.ball.vel = V3(0, 0, 0);

    graphics->cam_pos = begin_animation_pos;
    graphics->cam_dir = vec3_normalize(vec3_sub(hole_pos, begin_animation_pos));
}

void golf_game_start_level(bool retry) {
    game.state = GOLF_GAME_STATE_BEGIN_CAMERA_ANIMATION;

    vec3 ball_start_pos = V3(0, 0, 0);
    vec3 hole_pos = V3(0, 0, 0);
    vec3 begin_animation_pos = V3(0, 0, 0);

    golf_level_t *level = golf->level;
    for (int i = 0; i < level->entities.length; i++) {
        golf_entity_t *entity = &level->entities.data[i];
        switch (entity->type) {
            case CAMERA_ZONE_ENTITY:
            case MODEL_ENTITY:
            case GEO_ENTITY:
            case GROUP_ENTITY:
            case WATER_ENTITY:
                break;
            case HOLE_ENTITY:
                hole_pos = entity->hole.transform.position;
                break;
            case BALL_START_ENTITY:
                ball_start_pos = entity->ball_start.transform.position;
                ball_start_pos.y += game.ball.radius;
                break;
            case BEGIN_ANIMATION_ENTITY:
                begin_animation_pos = entity->begin_animation.transform.position;
                break;
        }
    }

    // Create the BVH for entities that are not moving
    {
        golf_bvh_t *bvh = &game.physics.static_bvh;
        bvh->node_infos.length = 0;
        for (int i = 0; i < golf->level->entities.length; i++) {
            golf_entity_t *entity = &golf->level->entities.data[i];

            switch (entity->type) {
                case MODEL_ENTITY:
                case WATER_ENTITY:
                case GEO_ENTITY: {
                    bool ignore_physics = entity->type == MODEL_ENTITY && entity->model.ignore_physics;
                    if (!ignore_physics) {
                        golf_movement_t *movement = golf_entity_get_movement(entity);
                        if (!movement || movement->type == GOLF_MOVEMENT_NONE) {
                            vec_push(&bvh->node_infos, golf_bvh_node_info(bvh, i, golf->level, entity, game.t));
                        }
                    }
                    break;
                }
                case BEGIN_ANIMATION_ENTITY:
                case CAMERA_ZONE_ENTITY:
                case BALL_START_ENTITY:
                case HOLE_ENTITY:
                case GROUP_ENTITY:
                    break;
            }
        }
        golf_bvh_construct(bvh, bvh->node_infos);
    }

    game.t = 0;

    game.stroke_count = 0;
    game.ball_start_pos = ball_start_pos;
    game.hole_pos = hole_pos;

    game.ball.pos = ball_start_pos;
    game.ball.draw_pos = ball_start_pos;
    game.ball.vel = V3(0, 0, 0);
    game.ball.rot_vec = V3(0, 0, 0);
    game.ball.orientation = QUAT(0, 0, 0, 1);
    game.ball.radius = 0.12f;
    game.ball.time_going_slow = 0;
    game.ball.time_since_water_ripple = 0;
    game.ball.rot_vel = 0;
    game.ball.is_moving = false;
    game.ball.is_out_of_bounds = false;
    game.ball.is_in_hole = false;
    game.ball.time_out_of_bounds = -1;

    game.cam.auto_rotate = true;
    game.cam.angle = _golf_game_get_camera_zone_angle(ball_start_pos);
    game.cam.angle_velocity = 0;

    game.physics.time_behind = 0;

    game.aim_line.power = 0;
    game.aim_line.aim_delta = V2(0, 0);
    game.aim_line.offset = V2(0, 0);
    game.aim_line.num_points = 0;
    if (retry == false){
        game.begin_camera_animation.t = 0;
    }
    game.begin_camera_animation.cam_pos0 = begin_animation_pos;
    game.begin_camera_animation.cam_dir0 = vec3_normalize(vec3_sub(hole_pos, game.begin_camera_animation.cam_pos0));

    vec3 cam_delta = vec3_rotate_y(V3(2.6f, 1.5f, 0), game.cam.angle);
    game.begin_camera_animation.cam_pos1 = vec3_add(game.ball.draw_pos, cam_delta);
    game.begin_camera_animation.cam_dir1 = vec3_normalize(vec3_sub(vec3_add(game.ball.draw_pos, V3(0, 0.3f, 0)), game.begin_camera_animation.cam_pos1));

    graphics->cam_pos = game.begin_camera_animation.cam_pos0;
    graphics->cam_dir = game.begin_camera_animation.cam_dir0;
}

void golf_game_start_aiming(void) {
    game.state = GOLF_GAME_STATE_AIMING;
    game.aim_line.num_points = 0;
}

void golf_game_stop_aiming(void) {
    game.state = GOLF_GAME_STATE_WAITING_FOR_AIM;
}

void golf_game_hit_ball(vec2 aim_delta) {
    game.state = GOLF_GAME_STATE_WATCHING_BALL;
    game.stroke_count++;

    vec3 aim_direction = V3(aim_delta.x, 0, aim_delta.y);
    aim_direction = vec3_normalize(vec3_rotate_y(aim_direction, game.cam.angle - 0.5f * MF_PI));

    float green_power = CFG_NUM(game_cfg, "aim_green_power");
    float yellow_power = CFG_NUM(game_cfg, "aim_yellow_power");
    float red_power = CFG_NUM(game_cfg, "aim_red_power");
    float green_speed = CFG_NUM(game_cfg, "aim_green_speed");
    float yellow_speed = CFG_NUM(game_cfg, "aim_yellow_speed");
    float red_speed = CFG_NUM(game_cfg, "aim_red_speed");
    float dark_red_speed = CFG_NUM(game_cfg, "aim_dark_red_speed");
    float start_speed = 0;
    float p = game.aim_line.power;
    if (p < green_power) {
        float a = p / green_power;
        start_speed = green_speed + (yellow_speed - green_speed) * a;
    }
    else if (p < yellow_power) {
        float a = (p - green_power) / (yellow_power - green_power);
        start_speed = yellow_speed + (red_speed - yellow_speed) * a;
    }
    else if (p < red_power) {
        float a = (p - yellow_power) / (red_power - yellow_power);
        start_speed = red_speed + (dark_red_speed - red_speed) * a;
    }
    else {
        start_speed = dark_red_speed;
    }

    game.cam.auto_rotate = true;

    game.ball.vel = vec3_scale(aim_direction, start_speed);
    game.ball.is_moving = true;
    game.ball.start_pos = game.ball.pos;
    game.cam.start_angle = game.cam.angle;

    game.physics.collision_history.length = 0;

    golf_audio_start_sound("hit_ball", "data/audio/impactPlank_medium_000.ogg", 1, false, true);
}

void golf_game_pause(void) {
    game.state_before_pause = game.state;
    game.state = GOLF_GAME_STATE_PAUSED;
}

void golf_game_resume(void) {
    game.state = game.state_before_pause;
}
