#include "golf/game.h"

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

    graphics->cam_pos = V3(5, 5, 5);
    graphics->cam_dir = vec3_normalize(V3(-5, -5, -5));
    graphics->cam_up = V3(0, 1, 0);
}

static void _golf_game_update_state_main_menu(float dt) {
}

static void _golf_game_update_state_waiting_for_aim(float dt) {
    if (inputs->mouse_down[SAPP_MOUSEBUTTON_LEFT] || inputs->touch_down) {
        vec2 aim_circle_pos = vec2_scale(graphics->window_size, 0.5f);

        vec2 pos0, pos1;
        if (inputs->mouse_down[SAPP_MOUSEBUTTON_LEFT]) {
            pos0 = inputs->mouse_pos;
            pos1 = inputs->prev_mouse_pos;
        }
        else
        {
            pos0 = inputs->touch_pos;
            pos1 = inputs->prev_touch_pos;
        }
        
        vec2 delta = vec2_sub(pos0, pos1);

        if (pos0.x < aim_circle_pos.x) {
            game.camera.angle -= 1.5f * (delta.y / graphics->window_size.x);
        }
        else {
            game.camera.angle += 1.5f * (delta.y / graphics->window_size.x);
        }

        if (pos0.y >= aim_circle_pos.y) {
            game.camera.angle -= 1.5f * (delta.x / graphics->window_size.x);
        }
        else {
            game.camera.angle += 1.5f * (delta.x / graphics->window_size.x);
        }
    }

    vec3 cam_delta = vec3_rotate_y(V3(2.6f, 1.5f, 0), game.camera.angle);
    graphics->cam_pos = vec3_add(game.ball_pos, cam_delta);
    graphics->cam_dir = vec3_normalize(vec3_sub(game.ball_pos, graphics->cam_pos));
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

    game.ball_pos = ball_start_pos;
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
