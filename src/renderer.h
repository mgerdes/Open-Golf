#ifndef _RENDERER_H
#define _RENDERER_H

#include <stdint.h>

#include "array.h"
#include "assets.h"
#include "file.h"
#include "map.h"
#include "maths.h"
#include "ui.h"
#include "sokol_gfx.h"

struct font {
    struct {
        int x, y, width, height, xoffset, yoffset, xadvance;
    } chars[256];
};

struct renderer {
    int window_width, window_height;
    int game_fb_width, game_fb_height;

    vec3 cam_pos, cam_dir, cam_up;
    float cam_azimuth_angle, cam_inclination_angle;

    float near_plane, far_plane;
    mat4 proj_mat, view_mat, proj_view_mat;
    mat4 ui_proj_mat;

    struct {
        sg_image 
            fxaa_image,
            game_color_image,
            game_depth_image;
        sg_pass fxaa_pass,
                game_pass,
                ui_pass;

        sg_shader 
            aim_icon_shader,
            aim_helper_shader,
            ball_shader,
            hole_editor_environment_shader, 
            hole_editor_ground_shader,
            hole_editor_terrain_shader, 
            hole_editor_water_shader,
            environment_shader,
            fxaa_shader,
            cup_shader,
            occluded_ball_shader,
            pass_through_shader,
            single_color_shader,
            terrain_shader,
            texture_shader,
            ui_shader,
            ui_single_color_shader,
            water_shader,
            water_around_ball_shader,
            water_ripple_shader
                ;

        sg_pipeline 
            aim_icon_pipeline,
            aim_helper_pipeline,
            ball_pipeline[2],
            hole_editor_environment_pipeline,
            hole_editor_ground_pipeline,
            hole_editor_terrain_pipeline,
            hole_editor_water_pipeline,
            environment_pipeline,
            fxaa_pipeline,
            cup_pipeline[2],
            terrain_pipeline[2],
            texture_pipeline,
            ui_pipeline,
            ui_single_color_pipeline,
            water_pipeline,
            water_around_ball_pipeline,
            water_ripple_pipeline,
            occluded_ball_pipeline,
            objects_pipeline,
            physics_debug_pipeline
                ;

            struct {
                int num_points;
                sg_buffer positions_buf, 
                          alpha_buf;
                vec4 color;
            } game_aim;
    } sokol;

    struct font small_font, medium_font, large_font;
};

struct game;
struct game_editor;
struct hole_editor;

void renderer_init(struct renderer *renderer);
void renderer_new_frame(struct renderer *renderer, float dt);
void renderer_end_frame(struct renderer *renderer);
vec3 renderer_world_to_screen(struct renderer *renderer, vec3 world_point);
vec3 renderer_screen_to_world(struct renderer *renderer, vec3 screen_point);
void renderer_draw_line(struct renderer *renderer, vec3 p0, vec3 p1, float size, vec4 color);
void renderer_draw_main_menu(struct renderer *renderer, struct main_menu *main_menu);
void renderer_draw_game(struct renderer *renderer, struct game *game, struct game_editor *ed);
void renderer_draw_hole_editor(struct renderer *renderer, struct game *game, struct hole_editor *ce);
void renderer_update_game_icon_buffer(struct renderer *renderer, 
        vec2 off, vec2 bp0, vec2 bp1, vec2 bp2, vec2 bp3);

#endif
