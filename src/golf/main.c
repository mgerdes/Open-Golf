#include <stdbool.h>

#if defined(_WIN32)
#include "3rd_party/glad/glad.h"
#endif
#include "3rd_party/sokol/sokol_app.h"
#include "3rd_party/sokol/sokol_audio.h"
#include "3rd_party/sokol/sokol_gfx.h"
#include "3rd_party/sokol/sokol_glue.h"
#include "3rd_party/sokol/sokol_imgui.h"
#include "3rd_party/sokol/sokol_time.h"

#include "mcore/mimport.h"
#include "mcore/mlog.h"
#include "mcore/mscript.h"
#include "mcore/mfile.h"

#include "golf/assets.h"
#include "golf/audio.h"
#include "golf/config.h"
#include "golf/game.h"
#include "golf/game_editor.h"
#include "golf/log.h"
#include "golf/profiler.h"
#include "golf/renderer.h"
#include "golf/ui.h"

#if MEMBED_FILES
#include "membedder/membedded_files.h"
#endif

enum app_state {
    APP_MAIN_MENU,
    APP_SINGLE_SHOT_MENU,
    APP_IN_GAME,
};

static bool inited = false;
static enum app_state state;
static struct renderer renderer;
static struct game game;
static struct game_editor game_editor;
static struct main_menu main_menu;
static struct button_inputs inputs;
static uint64_t last_time;

static void init(void) {
#if defined(_WIN32)
    if(!gladLoadGL()) { exit(-1); }
#endif
    stm_setup();
    sg_setup(&(sg_desc){ 
            .buffer_pool_size = 2048, 
            .image_pool_size = 2048,
            .context = sapp_sgcontext(),
            });
    simgui_setup(&(simgui_desc_t) { .dpi_scale = sapp_dpi_scale() });
    saudio_setup(&(saudio_desc){ .sample_rate = 44100, .buffer_frames = 1024, .packet_frames = 64, .num_packets = 32, });
    memset(&inputs, 0, sizeof(inputs));
    profiler_init();
    last_time = 0;
}

static void get_world_ray_from_window_pos(vec2 window_pos, vec3 *world_ro, vec3 *world_rd) {
    mat4 inv_proj = mat4_inverse(renderer.proj_mat);
    mat4 inv_view = mat4_inverse(renderer.view_mat);
    float x = -1.0f + 2.0f * window_pos.x / renderer.game_fb_width;
    float y = -1.0f + 2.0f * window_pos.y / renderer.game_fb_height;
    vec4 clip_space = V4(x, y, -1.0f, 1.0f);
    vec4 eye_space = vec4_apply_mat(clip_space, inv_proj);
    eye_space = V4(eye_space.x, eye_space.y, -1.0f, 0.0f);
    vec4 world_space_4 = vec4_apply_mat(eye_space, inv_view);
    vec3 world_space = V3(world_space_4.x, world_space_4.y, world_space_4.z);
    *world_ro = renderer.cam_pos;
    *world_rd = vec3_normalize(world_space);
}

static void frame(void) {
    float dt = (float) stm_sec(stm_laptime(&last_time));

    profiler_start_frame();
    if (!inited) {
        inited = true;
        profiler_push_section("init");
        state = APP_MAIN_MENU;
#ifdef MEMBED_FILES
        mimport_init(_num_embedded_files, _embedded_files);
#else
        mimport_init(0, NULL);
#endif
        asset_store_init();
        config_init();
        audio_init();
        renderer_init(&renderer);
        game_editor_init(&game_editor, &game, &renderer);
        game_init(&game, &game_editor, &renderer);
        main_menu_init(&main_menu);
        mlog_init();
        profiler_pop_section();

        game_load_hole(&game, &game_editor, 0);
        game.state = GAME_STATE_MAIN_MENU;

        //{
            //tests_run();
        //}

        {
            struct mscript *mscript = mscript_create("data/scripts");
            mscript_program_t *program = mscript_get_program(mscript, "testing.mscript");

            if (program) {
                mscript_vm_t *vm = mscript_vm_create(program);
                mscript_val_t args[3];
                args[0] = mscript_val_int(20);
                args[1] = mscript_val_int(25);

                mfile_t *file = malloc(sizeof(mfile_t));
                *file = mfile("scripts/testing.mscript");
                mfile_load_data(file);
                args[2] = mscript_val_void_ptr(file);

                mscript_vm_run(vm, "run", 3, args);
            }
        }
    }

    //
    // Controls
    //
    {
        profiler_push_section("button_maths");
        inputs.mouse_delta = vec2_sub(inputs.mouse_pos, inputs.prev_mouse_pos);
        inputs.mouse_down_delta = vec2_sub(inputs.mouse_pos, inputs.mouse_down_pos);
        inputs.prev_mouse_pos = inputs.mouse_pos;
        get_world_ray_from_window_pos(inputs.mouse_pos, &inputs.mouse_ray_orig,
                &inputs.mouse_ray_dir);
        get_world_ray_from_window_pos(inputs.mouse_down_pos, &inputs.mouse_down_ray_orig,
                &inputs.mouse_down_ray_dir);
        profiler_pop_section();
    }

    simgui_new_frame(sapp_width(), sapp_height(), dt);

    //
    // Update
    //
    if (state == APP_MAIN_MENU) {
        main_menu_update(&main_menu, dt, inputs);
        game_update(&game, dt, inputs, &renderer, &game_editor);
        if (main_menu.start_game_button.is_clicked) {
            game.state = GAME_STATE_BEGIN_HOLE;
            state = APP_IN_GAME;
        }
    }
    else if (state == APP_IN_GAME) {
        if (game_editor.editing_hole) {
            game_editor_update(&game_editor, dt, inputs, &renderer);
        }
        else {
            game_update(&game, dt, inputs, &renderer, &game_editor);
            game_editor_update(&game_editor, dt, inputs, &renderer);
        }

        {
            game_editor_draw_warnings(&game_editor);
        }

        if (game.state == GAME_STATE_HOLE_COMPLETE) {
            if (game.ui.next_hole_button.is_clicked) {
                if (game.cur_hole + 1 < config_get_int("game_num_holes")) {
                    game_load_hole(&game, &game_editor, game.cur_hole + 1);
                    game.drawing.blink_t = 0.0f;
                    game.drawing.is_blink = true;
                    game.state = GAME_STATE_BEGIN_HOLE;
                }
                else {
                    game_load_hole(&game, &game_editor, 0);
                    game.state = GAME_STATE_MAIN_MENU;
                    state = APP_MAIN_MENU;
                }
            }
        }
    }

    //
    // Audio
    //
    {
        int num_samples = saudio_expect();
        if (num_samples > 0) {
            float *buffer = malloc(sizeof(float)*num_samples);
            audio_get_samples(buffer, num_samples, dt);
            saudio_push(buffer, num_samples);
            free(buffer);
        }
    }

    //
    // Draw
    //
    renderer_new_frame(&renderer, dt);
    if (state == APP_MAIN_MENU) {
        renderer_draw_game(&renderer, &game, &game_editor);
        renderer_draw_main_menu(&renderer, &main_menu);
    }
    else if (state == APP_IN_GAME) {
        if (game_editor.editing_hole) {
            renderer_draw_hole_editor(&renderer, &game, &game_editor.hole_editor);
        }
        else {
            renderer_draw_game(&renderer, &game, &game_editor);
        }
    }
    renderer_end_frame(&renderer);

    {
        for (int i = 0; i < SAPP_MAX_KEYCODES; i++) {
            if (inputs.button_clicked[i]) {
                inputs.button_clicked[i] = false;
            }
        }
        for (int i = 0; i < SAPP_MAX_MOUSEBUTTONS; i++) {
            if (inputs.mouse_clicked[i]) {
                inputs.mouse_clicked[i] = false;
            }
        }
    }

    fflush(stdout);
    profiler_finish_frame();
}

static void cleanup(void) {
    sg_shutdown();
}

static void event(const sapp_event *event) {
    simgui_handle_event(event);

    if (event->type == SAPP_EVENTTYPE_MOUSE_DOWN ||
            event->type == SAPP_EVENTTYPE_MOUSE_UP ||
            event->type == SAPP_EVENTTYPE_MOUSE_MOVE ||
            event->type == SAPP_EVENTTYPE_TOUCHES_BEGAN ||
            event->type == SAPP_EVENTTYPE_TOUCHES_ENDED ||
            event->type == SAPP_EVENTTYPE_TOUCHES_MOVED) {
        float fb_width = (float) renderer.game_fb_width;
        float fb_height = (float) renderer.game_fb_height;
        float w_width = (float) renderer.window_width;
        float w_height = (float) renderer.window_height;
        float w_fb_width = w_width;
        float w_fb_height = (fb_height/fb_width) * w_fb_width;
        if (w_fb_height > w_height) {
            w_fb_height = w_height;
            w_fb_width = (fb_width/fb_height)*w_fb_height;
        }
        vec2 mp = V2(event->mouse_x, w_height - event->mouse_y);
        if (event->num_touches > 0) {
            sapp_touchpoint tp = event->touches[0];
            mp.x = tp.pos_x;
            mp.y = w_height - tp.pos_y;
        }
        mp.x = mp.x - (0.5f*w_width - 0.5f*w_fb_width);
        mp.y = mp.y - (0.5f*w_height - 0.5f*w_fb_height);
        mp.x = mp.x*(fb_width/w_fb_width);
        mp.y = mp.y*(fb_height/w_fb_height);
        inputs.mouse_pos = mp;
        inputs.window_mouse_pos = V2(event->mouse_x, event->mouse_y);
        if (event->num_touches > 0) {
            sapp_touchpoint tp = event->touches[0];
            inputs.window_mouse_pos = V2(tp.pos_x, tp.pos_y);
        }
    }

    if (event->type == SAPP_EVENTTYPE_KEY_DOWN) {
        inputs.button_down[event->key_code] = true;
    }
    else if (event->type == SAPP_EVENTTYPE_KEY_UP) {
        inputs.button_down[event->key_code] = false;
        inputs.button_clicked[event->key_code] = true;
    }
    else if (event->type == SAPP_EVENTTYPE_MOUSE_DOWN ||
            event->type == SAPP_EVENTTYPE_TOUCHES_BEGAN) {
        if (!inputs.mouse_down[SAPP_MOUSEBUTTON_LEFT] && SAPP_MOUSEBUTTON_LEFT == SAPP_MOUSEBUTTON_LEFT) {
            inputs.mouse_down_pos = inputs.mouse_pos;
        }
        inputs.mouse_down[SAPP_MOUSEBUTTON_LEFT] = true;
    }
    else if (event->type == SAPP_EVENTTYPE_MOUSE_UP ||
            event->type == SAPP_EVENTTYPE_TOUCHES_ENDED) {
        inputs.mouse_down[SAPP_MOUSEBUTTON_LEFT] = false;
        inputs.mouse_clicked[SAPP_MOUSEBUTTON_LEFT] = true;
    }
}

sapp_desc sokol_main(int argc, char *argv[]) {
    return (sapp_desc){
        .init_cb = init,
            .frame_cb = frame,
            .cleanup_cb = cleanup,
            .event_cb = event,
            .width = 1280,
            .height = 720,
            .window_title = "Minigolf",
            .enable_clipboard = true,
            .clipboard_size = 1024,
            .fullscreen = false,
            .high_dpi = false,
            .html5_canvas_resize = false,
            .win32_console_utf8 = true,
            .win32_console_create = true,
            .swap_interval = 1,
    };
}
