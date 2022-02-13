#include "golf/game.h"

#include <assert.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"

#include "common/debug_console.h"
#include "common/graphics.h"
#include "common/inputs.h"
#include "common/log.h"
#include "golf/golf.h"

static golf_game_t game;
static golf_t *golf;
static golf_graphics_t *graphics;
static golf_inputs_t *inputs;

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
                igText("    Restitution: %0.2f", contact.face.restitution); 
                igText("    Velocity Scale: %0.2f", contact.face.vel_scale);
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

void golf_game_init(void) {
    memset(&game, 0, sizeof(game));

    golf = golf_get();
    graphics = golf_graphics_get();
    inputs = golf_inputs_get();

    game.state = GOLF_GAME_STATE_MAIN_MENU;
    game.cam.angle = 0;
    game.cam.angle_velocity = 0;

    game.ball.pos = V3(0, 0, 0);
    game.ball.draw_pos = V3(0, 0, 0);
    game.ball.vel = V3(0, 0, 0);
    game.ball.radius = 0.12f;
    game.ball.time_going_slow = 0;
    game.ball.is_moving = true;

    game.physics.time_behind = 0;
    game.physics.debug_draw_collisions = false;
    vec_init(&game.physics.collision_history, "physics");

    golf_bvh_init(&game.bvh);

    graphics->cam_pos = V3(5, 5, 5);
    graphics->cam_dir = vec3_normalize(V3(-5, -5, -5));
    graphics->cam_up = V3(0, 1, 0);

    golf_debug_console_add_tab("Game", _golf_game_debug_tab);
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
    if (!game.ball.is_moving) {
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
    else if (bc1->face.vel_scale > bc0->face.vel_scale) {
        return 1;
    }
    else if (bc1->face.vel_scale < bc0->face.vel_scale) {
        return -1;
    }
    else if (bc1->face.restitution > bc0->face.restitution) {
        return -1;
    }
    else if (bc1->face.restitution < bc0->face.restitution) {
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
    vec3 bp0 = bp;
    vec3 bv0 = bv;

    int num_contacts;
    golf_ball_contact_t contacts[MAX_NUM_CONTACTS];
    golf_bvh_ball_test(&game.bvh, bp, br, bv, contacts, &num_contacts, MAX_NUM_CONTACTS);
    qsort(contacts, num_contacts, sizeof(golf_ball_contact_t), _ball_contact_cmp);

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
                e1 = contact->face.c;
            }
            else if (contact->type == TRIANGLE_CONTACT_BC) {
                e0 = contact->face.b;
                e1 = contact->face.c;
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
        vec3 vr = vec3_sub(bv, contact->velocity);
        contact->cull_dot = vec3_dot(n, vec3_normalize(vr));
        if (contact->cull_dot > 0.01f) {
            contact->is_ignored = true;
            continue;
        }

        float e = contact->face.restitution;
        float v_scale = contact->face.vel_scale;
        float imp = -(1 + e) * vec3_dot(vr, n);

        contact->impulse_mag = imp; 
        contact->impulse = vec3_scale(n, imp);
        contact->v0 = bv0;

        bv = vec3_add(bv, contact->impulse);
        bv = vec3_scale(bv, v_scale);

        vec3 t = vec3_sub(bv, vec3_scale(n, vec3_dot(bv, n)));
        if (vec3_length(t) > 0.0001f) {
            t = vec3_normalize(t);

            float jt = -vec3_dot(vr, t);
            if (fabsf(jt) > 0.0001f) {
                float friction = contact->face.friction;
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
    }

    bv = vec3_add(bv, V3(0, -9.8f * dt, 0));
    bp = vec3_add(bp, vec3_scale(bv, dt));

    for (int i = 0; i < num_contacts; i++) {
        golf_ball_contact_t *contact = &contacts[i];
        if (contact->is_ignored) {
            continue;
        }

        float pen = fmaxf(contact->penetration, 0);
        vec3 correction = vec3_scale(contact->normal, pen * 0.5f);
        bp = vec3_add(bp, correction);
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

    if (vec3_length(bv) < 0.5f) {
        game.ball.time_going_slow += dt;
    }
    else {
        game.ball.time_going_slow = 0.0f;
    }

    if (!game.ball.is_moving && vec3_length(bv) > 0.5f) {
        game.ball.is_moving = true;
    }
    if (game.ball.is_moving) {
        game.ball.pos = bp;
        game.ball.vel = bv;
        if (game.ball.time_going_slow > 0.5f) {
            game.ball.is_moving = false;
        }
    }
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
        float physics_dt = 1.0f/60.0f;
        game.physics.time_behind += dt;

        vec3 bp_prev = V3(0, 0, 0);
        int num_ticks = 0;
        while (game.physics.time_behind >= 0 && num_ticks < 5) {
            bp_prev = game.ball.pos;
            _physics_tick(physics_dt);
            game.physics.time_behind -= physics_dt;
            num_ticks++;
        }

        float alpha = (float)(-game.physics.time_behind / physics_dt);
        game.ball.draw_pos = vec3_add(vec3_scale(game.ball.pos, 1.0f - alpha), vec3_scale(bp_prev, alpha));
    }

    {
        vec3 cam_delta = vec3_rotate_y(V3(2.6f, 1.5f, 0), game.cam.angle);
        vec3 wanted_pos = vec3_add(game.ball.draw_pos, cam_delta);
        vec3 diff = vec3_sub(wanted_pos, graphics->cam_pos);
        graphics->cam_pos = vec3_add(graphics->cam_pos, vec3_scale(diff, 0.5f));
        graphics->cam_dir = vec3_normalize(vec3_sub(vec3_add(game.ball.draw_pos, V3(0, 0.3f, 0)), graphics->cam_pos));
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

    {
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
                    vec_push(&game.bvh.node_infos, golf_bvh_node_info(&game.bvh, i, model, model_mat, golf->level));
                    break;
                }
                case BALL_START_ENTITY:
                case HOLE_ENTITY:
                case GROUP_ENTITY:
                    break;
            }
        }
        golf_bvh_construct(&game.bvh, game.bvh.node_infos);
    }

    game.ball.pos = ball_start_pos;
    game.ball.draw_pos = ball_start_pos;

    vec3 cam_delta = vec3_rotate_y(V3(2.6f, 1.5f, 0), game.cam.angle);
    graphics->cam_pos = vec3_add(game.ball.draw_pos, cam_delta);
    graphics->cam_dir = vec3_normalize(vec3_sub(vec3_add(game.ball.draw_pos, V3(0, 0.3f, 0)), graphics->cam_pos));
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
    game.ball.vel = vec3_scale(aim_direction, 15.0f);
    game.ball.is_moving = true;

    game.physics.collision_history.length = 0;
}
