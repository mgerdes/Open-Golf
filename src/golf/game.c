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
    game.camera.angle = 0;
    game.camera.angle_velocity = 0;

    game.ball.pos = V3(0, 0, 0);
    game.ball.radius = 0.2f;

    golf_bvh_init(&game.bvh);

    graphics->cam_pos = V3(5, 5, 5);
    graphics->cam_dir = vec3_normalize(V3(-5, -5, -5));
    graphics->cam_up = V3(0, 1, 0);
}

static void _golf_game_update_state_main_menu(float dt) {
}

static void _golf_game_update_state_waiting_for_aim(float dt) {
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
                    vec_push(&game.bvh.node_infos, golf_bvh_node_info(i, model, world_transform));

                    break;
                }
                case BALL_START_ENTITY:
                case HOLE_ENTITY:
                case GROUP_ENTITY:
                    break;
            }
        }
        golf_bvh_construct(&game.bvh, game.bvh.node_infos);

        if (golf_bvh_ball_test(&game.bvh, game.ball.pos, game.ball.radius, V3(0, 0, 0), NULL, NULL, 0)) {
            printf("1\n");
        }
        else {
            printf("0\n");
        }
    }


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

void golf_game_update(float dt) {
    switch (game.state) {
        case GOLF_GAME_STATE_MAIN_MENU:
            _golf_game_update_state_main_menu(dt);
            break;
        case GOLF_GAME_STATE_WAITING_FOR_AIM:
            _golf_game_update_state_waiting_for_aim(dt);
            break;
    }

    //float theta = game.camera.inclination_angle;
    //float phi = game.camera.azimuth_angle;
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
    vec3 cam_delta = vec3_rotate_y(V3(2.6f, 1.5f, 0), game.camera.angle);
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
}
