#include "golf/game.h"

#include <assert.h>

#include "common/log.h"
#include "common/graphics.h"
#include "common/inputs.h"
#include "golf/golf.h"

static golf_game_t game;
static golf_t *golf;
static golf_graphics_t *graphics;
static golf_inputs_t *inputs;

golf_game_t *golf_game_get(void) {
    return &game;
}

void golf_game_init(void) {
    memset(&game, 0, sizeof(game));

    golf = golf_get();
    graphics = golf_graphics_get();
    inputs = golf_inputs_get();

    game.state = GOLF_GAME_STATE_MAIN_MENU;
    game.cam.angle = 0;
    game.cam.angle_velocity = 0;

    game.ball.pos = V3(0, 0, 0);
    game.ball.vel = V3(0, 0, 0);
    game.ball.radius = 0.2f;

    golf_bvh_init(&game.bvh);

    graphics->cam_pos = V3(5, 5, 5);
    graphics->cam_dir = vec3_normalize(V3(-5, -5, -5));
    graphics->cam_up = V3(0, 1, 0);
}

static void _golf_game_update_state_main_menu(float dt) {
}

static void _golf_game_update_state_waiting_for_aim(float dt) {


    if (inputs->button_down[SAPP_KEYCODE_W]) {
        vec3 dir = vec3_normalize(V3(graphics->cam_dir.x, 0, graphics->cam_dir.z));
        game.ball.pos = vec3_add(game.ball.pos, vec3_scale(dir, 0.1f));
    }
    if (inputs->button_down[SAPP_KEYCODE_A]) {
        vec3 dir = vec3_normalize(V3(graphics->cam_dir.x, 0, graphics->cam_dir.z));
        dir = vec3_rotate_y(dir, 0.5f * MF_PI);
        game.ball.pos = vec3_add(game.ball.pos, vec3_scale(dir, 0.1f));
    }
    if (inputs->button_down[SAPP_KEYCODE_D]) {
        vec3 dir = vec3_normalize(V3(graphics->cam_dir.x, 0, graphics->cam_dir.z));
        dir = vec3_rotate_y(dir, 1.5f * MF_PI);
        game.ball.pos = vec3_add(game.ball.pos, vec3_scale(dir, 0.1f));
    }
    if (inputs->button_down[SAPP_KEYCODE_S]) {
        vec3 dir = vec3_normalize(V3(graphics->cam_dir.x, 0, graphics->cam_dir.z));
        dir = vec3_rotate_y(dir, MF_PI);
        game.ball.pos = vec3_add(game.ball.pos, vec3_scale(dir, 0.1f));
    }
    if (inputs->button_down[SAPP_KEYCODE_Q]) {
        game.ball.pos = vec3_add(game.ball.pos, V3(0, 0.1f, 0));
    }
    if (inputs->button_down[SAPP_KEYCODE_E]) {
        game.ball.pos = vec3_add(game.ball.pos, V3(0, -0.1f, 0));
    }
}

static void _golf_game_update_state_aiming(float dt) {
}

static void _golf_game_update_state_watching_ball(float dt) {
}

void golf_game_update(float dt) {
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
    }

    if (game.state > GOLF_GAME_STATE_MAIN_MENU) {
        float EPS = 0.001f;

        game.bvh.node_infos.length = 0;
        for (int i = 0; i < golf->level->entities.length; i++) {
            golf_entity_t *entity = &golf->level->entities.data[i];

            switch (entity->type) {
                case MODEL_ENTITY:
                case GEO_ENTITY: {
                    golf_model_t *model = golf_entity_get_model(entity);
                    golf_transform_t *transform = golf_entity_get_transform(entity);
                    assert(model && transform);

                    golf_transform_t world_transform = golf_entity_get_world_transform(golf->level, entity);
                    mat4 model_mat = golf_transform_get_model_mat(world_transform);
                    vec_push(&game.bvh.node_infos, golf_bvh_node_info(&game.bvh, i, model, model_mat));
                    break;
                }
                case BALL_START_ENTITY:
                case HOLE_ENTITY:
                case GROUP_ENTITY:
                    break;
            }
        }
        golf_bvh_construct(&game.bvh, game.bvh.node_infos);

        int num_contacts;
        golf_ball_contact_t contacts[8];
        golf_bvh_ball_test(&game.bvh, game.ball.pos, game.ball.radius, game.ball.vel, contacts, &num_contacts, 8);

        // Filter out the contacts
        {
            int num_processed_vertices = 0;
            vec3 processed_vertices[9 * 8];

            // All face contacts are used
            for (int i = 0; i < num_contacts; i++) {
                golf_ball_contact_t *contact = &contacts[i];
                if (contact->is_ignored || contact->type != TRIANGLE_CONTACT_FACE) {
                    continue;
                }

                processed_vertices[num_processed_vertices++] = contact->face.a;
                processed_vertices[num_processed_vertices++] = contact->face.b;
                processed_vertices[num_processed_vertices++] = contact->face.c;
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
                    e0 = contact->face.a;
                    e1 = contact->face.b;
                }
                else if (contact->type == TRIANGLE_CONTACT_AC) {
                    e0 = contact->face.a;
                    e0 = contact->face.c;
                }
                else if (contact->type == TRIANGLE_CONTACT_BC) {
                    e0 = contact->face.b;
                    e0 = contact->face.c;
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

                processed_vertices[num_processed_vertices++] = contact->face.a;
                processed_vertices[num_processed_vertices++] = contact->face.b;
                processed_vertices[num_processed_vertices++] = contact->face.c;
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
                    p = contact->face.a;
                }
                else if (contact->type == TRIANGLE_CONTACT_B) {
                    p = contact->face.b;
                }
                else if (contact->type == TRIANGLE_CONTACT_C) {
                    p = contact->face.c;
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

                processed_vertices[num_processed_vertices++] = contact->face.a;
                processed_vertices[num_processed_vertices++] = contact->face.b;
                processed_vertices[num_processed_vertices++] = contact->face.c;
            }
        }

        int contact_num = 0;
        for (int i = 0; i < num_contacts; i++) {
            golf_ball_contact_t *contact = &contacts[i];
            if (contact->is_ignored) {
                continue;
            }

            vec3 n = contact->normal;
            vec3 vr = vec3_sub(game.ball.vel, contact->velocity);
            float cull_dot = vec3_dot(vec3_normalize(vr), n);
            if (cull_dot > 0.01f) {
                contact->is_ignored = true;
                continue;
            }

            float imp = -(1 + 0.4f) * vec3_dot(vr, n);

            game.ball.vel = vec3_add(game.ball.vel, vec3_scale(n, imp));
            game.ball.vel = vec3_scale(game.ball.vel, 1);

            vec3 t = vec3_sub(game.ball.vel, vec3_scale(n, vec3_dot(game.ball.vel, n)));
            if (vec3_length(t) > EPS) {
                t = vec3_normalize(t);

                float jt = -vec3_dot(vr, t);
                if (fabsf(jt) > EPS) {
                    float friction = 0.3f;
                    if (jt > imp * friction) {
                        jt = imp * friction;
                    }
                    else if (jt < -imp * friction) {
                        jt = -imp * friction;
                    }

                    game.ball.vel = vec3_add(game.ball.vel, vec3_scale(t, jt));
                }
            }
        }

        game.ball.vel = vec3_add(game.ball.vel, V3(0, -9.8 * dt, 0));
        game.ball.pos = vec3_add(game.ball.pos, vec3_scale(game.ball.vel, dt));

        for (int i = 0; i < num_contacts; i++) {
            golf_ball_contact_t *contact = &contacts[i];
            if (contact->is_ignored) {
                continue;
            }

            float pen = fmaxf(contact->penetration, 0);
            vec3 correction = vec3_scale(contact->normal, pen * 0.5f);
            game.ball.pos = vec3_add(game.ball.pos, correction);
        }
    }

    {
        vec3 cam_delta = vec3_rotate_y(V3(2.6f, 1.5f, 0), game.cam.angle);
        graphics->cam_pos = vec3_add(game.ball.pos, cam_delta);
        graphics->cam_dir = vec3_normalize(vec3_sub(game.ball.pos, graphics->cam_pos));
    }
}

void golf_game_start_level(void) {
    game.state = GOLF_GAME_STATE_WAITING_FOR_AIM;

    vec3 ball_start_pos = V3(0, 0, 0);

    golf_level_t *level = golf->level;
    for (int i = 0; i < level->entities.length; i++) {
        golf_entity_t *entity = &level->entities.data[i];
        switch (entity->type) {
            case MODEL_ENTITY:
                break;
            case BALL_START_ENTITY:
                ball_start_pos = entity->ball_start.transform.position;
                break;
            case HOLE_ENTITY:
                break;
            case GEO_ENTITY:
                break;
            case GROUP_ENTITY:
                break;
        }
    }

    game.ball.pos = ball_start_pos;
    vec3 cam_delta = vec3_rotate_y(V3(2.6f, 1.5f, 0), game.cam.angle);
    graphics->cam_pos = vec3_add(ball_start_pos, cam_delta);
    graphics->cam_dir = vec3_normalize(vec3_sub(ball_start_pos, graphics->cam_pos));
}

void golf_game_start_aiming(void) {
    game.state = GOLF_GAME_STATE_AIMING;
}

void golf_game_stop_aiming(void) {
    game.state = GOLF_GAME_STATE_WAITING_FOR_AIM;
}

void golf_game_hit_ball(vec2 aim_delta) {
    game.state = GOLF_GAME_STATE_WATCHING_BALL;

    vec3 aim_direction = V3(aim_delta.x, 0, aim_delta.y);
    aim_direction = vec3_normalize(vec3_rotate_y(aim_direction, game.cam.angle - 0.5f * MF_PI));
    game.ball.vel = vec3_scale(aim_direction, 5.0f);
}
