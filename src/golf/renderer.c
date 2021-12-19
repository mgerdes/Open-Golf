#include "golf/renderer.h"

#include <assert.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include "cimgui/cimgui.h"
#include "cimguizmo/cimguizmo.h"
#include "stb/stb_image.h"
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "golf/editor.h"
#include "golf/log.h"
#include "golf/maths.h"
#include "golf/ui.h"

#include "golf/shaders/diffuse_color_material.glsl.h"
#include "golf/shaders/environment_material.glsl.h"
#include "golf/shaders/pass_through.glsl.h"
#include "golf/shaders/solid_color_material.glsl.h"
#include "golf/shaders/texture_material.glsl.h"
#include "golf/shaders/ui_sprite.glsl.h"

static golf_renderer_t renderer;

static void _set_ui_proj_mat(vec2 pos_offset) {
    float fb_width = (float) 1280;
    float fb_height = (float) 720;
    float w_width = (float) sapp_width();
    float w_height = (float) sapp_height();
    float w_fb_width = w_width;
    float w_fb_height = (fb_height/fb_width)*w_fb_width;
    if (w_fb_height > w_height) {
        w_fb_height = w_height;
        w_fb_width = (fb_width/fb_height)*w_fb_height;
    }
    renderer.ui_proj_mat = mat4_multiply_n(4,
            mat4_orthographic_projection(0.0f, w_width, 0.0f, w_height, 0.0f, 1.0f),
            mat4_translation(V3(pos_offset.x, pos_offset.y, 0.0f)),
            mat4_translation(V3(0.5f*w_width - 0.5f*w_fb_width, 0.5f*w_height - 0.5f*w_fb_height, 0.0f)),
            mat4_scale(V3(w_fb_width/fb_width, w_fb_height/fb_height, 1.0f))
            );
}

golf_renderer_t *golf_renderer_get(void) {
    return &renderer;
}

void golf_renderer_init(void) {
    golf_data_load("data/shaders/diffuse_color_material.glsl");
    golf_data_load("data/shaders/environment_material.glsl");
    golf_data_load("data/shaders/pass_through.glsl");
    golf_data_load("data/shaders/solid_color_material.glsl");
    golf_data_load("data/shaders/texture_material.glsl");
    golf_data_load("data/shaders/ui_sprite.glsl");

    {
        golf_shader_t *shader = golf_data_get_shader("data/shaders/diffuse_color_material.glsl");
        sg_pipeline_desc pipeline_desc = {
            .shader = shader->sg_shader,
            .layout = {
                .attrs = {
                    [ATTR_diffuse_color_material_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_diffuse_color_material_vs_normal] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 1 },
                },
            },
            .depth = {
                .compare = SG_COMPAREFUNC_LESS_EQUAL,
                .write_enabled = true,
            },
        };
        renderer.diffuse_color_material_pipeline = sg_make_pipeline(&pipeline_desc);
    }

    {
        golf_shader_t *shader = golf_data_get_shader("data/shaders/environment_material.glsl");
        sg_pipeline_desc pipeline_desc = {
            .shader = shader->sg_shader,
            .layout = {
                .attrs = {
                    [ATTR_environment_material_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_environment_material_vs_texturecoord] 
                        = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                    [ATTR_environment_material_vs_normal] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 2 },
                    [ATTR_environment_material_vs_lightmap_uv] 
                        = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 3 },
                },
            },
            .depth = {
                .compare = SG_COMPAREFUNC_LESS_EQUAL,
                .write_enabled = true,
            },
        };
        renderer.environment_material_pipeline = sg_make_pipeline(&pipeline_desc);
    }

    {
        golf_shader_t *shader = golf_data_get_shader("data/shaders/solid_color_material.glsl");
        sg_pipeline_desc pipeline_desc = {
            .shader = shader->sg_shader,
            .layout = {
                .attrs = {
                    [ATTR_solid_color_material_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                },
            },
            .depth = {
                .compare = SG_COMPAREFUNC_LESS_EQUAL,
                .write_enabled = true,
            },
            .colors[0] = {
                .blend = {
                    .enabled = true,
                    .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                    .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                }
            }
        };
        renderer.solid_color_material_pipeline = sg_make_pipeline(&pipeline_desc);
    }

    {
        golf_shader_t *shader = golf_data_get_shader("data/shaders/texture_material.glsl");
        sg_pipeline_desc pipeline_desc = {
            .shader = shader->sg_shader,
            .layout = {
                .attrs = {
                    [ATTR_texture_material_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_texture_material_vs_texturecoord] 
                        = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                    [ATTR_texture_material_vs_normal] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 2 },
                },
            },
            .depth = {
                .compare = SG_COMPAREFUNC_LESS_EQUAL,
                .write_enabled = true,
            },
        };
        renderer.texture_material_pipeline = sg_make_pipeline(&pipeline_desc);
    }

    {
        golf_shader_t *shader = golf_data_get_shader("data/shaders/ui_sprite.glsl");
        sg_pipeline_desc pipeline_desc = {
            .shader = shader->sg_shader,
            .layout = {
                .attrs = {
                    [ATTR_ui_sprite_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_ui_sprite_vs_texture_coord] 
                        = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                },
            },
            .colors[0] = {
                .blend = {
                    .enabled = true,
                    .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                    .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                },
            },
        };
        renderer.ui_sprites_pipeline = sg_make_pipeline(&pipeline_desc);
    }

    {
        golf_shader_t *shader = golf_data_get_shader("data/shaders/pass_through.glsl");
        sg_pipeline_desc pipeline_desc = {
            .shader = shader->sg_shader,
            .layout = {
                .attrs = {
                    [ATTR_pass_through_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                },
            },
            .depth = {
                .compare = SG_COMPAREFUNC_LESS_EQUAL
            },
            .stencil = {
                .enabled = true,
                .front = {
                    .compare = SG_COMPAREFUNC_ALWAYS,
                    .depth_fail_op = SG_STENCILOP_KEEP,
                    .pass_op = SG_STENCILOP_REPLACE,
                },
                .back = {
                    .compare = SG_COMPAREFUNC_ALWAYS,
                    .depth_fail_op = SG_STENCILOP_KEEP,
                    .pass_op = SG_STENCILOP_REPLACE,
                },
                .write_mask = 255,
                .ref = 255,
            },
            .colors[0] = {
                .write_mask = SG_COLORMASK_NONE,
            },
        };
        renderer.hole_pass1_pipeline = sg_make_pipeline(&pipeline_desc);
    }

    {
        golf_shader_t *shader = golf_data_get_shader("data/shaders/texture_material.glsl");
        sg_pipeline_desc pipeline_desc = {
            .shader = shader->sg_shader,
            .layout = {
                .attrs = {
                    [ATTR_texture_material_vs_position] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                    [ATTR_texture_material_vs_texturecoord] 
                        = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                    [ATTR_texture_material_vs_normal] 
                        = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 2 },
                },
            },
            .stencil = {
                .enabled = true,
                .front = {
                    .compare = SG_COMPAREFUNC_EQUAL,
                },
                .back = {
                    .compare = SG_COMPAREFUNC_EQUAL,
                },
                .read_mask = 255,
                .ref = 255,
            },
        };
        renderer.hole_pass2_pipeline = sg_make_pipeline(&pipeline_desc);
    }

    renderer.cam_azimuth_angle = 0.5f*MF_PI;
    renderer.cam_inclination_angle = 0;
    renderer.cam_pos = V3(5, 5, 5);
    renderer.cam_dir = vec3_normalize(V3(-5, -5, -5));

    {
        float x = renderer.cam_dir.x;
        float y = renderer.cam_dir.y;
        float z = renderer.cam_dir.z;
        renderer.cam_inclination_angle = acosf(y);
        renderer.cam_azimuth_angle = atan2f(z, x);
        if (x < 0) {
            //renderer.cam_inclination_angle += MF_PI;
        }

        float theta = renderer.cam_inclination_angle;
        float phi = renderer.cam_azimuth_angle;
        renderer.cam_dir.x = sinf(theta) * cosf(phi);
        renderer.cam_dir.y = cosf(theta);
        renderer.cam_dir.z = sinf(theta) * sinf(phi);
    }

    renderer.cam_up = V3(0, 1, 0);
}


static void _draw_ui_text(golf_ui_text_t text) {
    golf_font_t *font = text.font;
    golf_model_t *square_model = golf_data_get_model("data/models/ui_sprite_square.obj");

    float cur_x = text.pos.x;
    float cur_y = text.pos.y;
    int i = 0;

    int sz_idx = 0;
    for (int idx = 1; idx < font->atlases.length; idx++) {
        if (fabsf(font->atlases.data[idx].font_size - text.size) <
                fabsf(font->atlases.data[sz_idx].font_size - text.size)) {
            sz_idx = idx;
        }
    }

    golf_font_atlas_t atlas = font->atlases.data[sz_idx];

    float sz_scale = text.size / atlas.font_size;

    {
        sg_bindings bindings = {
            .vertex_buffers[ATTR_ui_sprite_vs_position] = square_model->sg_positions_buf,
            .vertex_buffers[ATTR_ui_sprite_vs_texture_coord] = square_model->sg_texcoords_buf,
            .fs_images[SLOT_ui_sprite_texture] = atlas.sg_image,
        };
        sg_apply_bindings(&bindings);
    }

    float width = 0.0f;
    while (text.string[i]) {
        char c = text.string[i];
        width += sz_scale * atlas.char_data[(int)c].xadvance;
        i++;
    }

    if (text.horiz_align == 0) {
        cur_x -= 0.5f * width;
    }
    else if (text.horiz_align < 0) {
    }
    else if (text.horiz_align > 0) {
        cur_x -= width;
    }
    else {
        golf_log_warning("Invalid text horizontal_alignment %s", text.horiz_align);
    }

    if (text.vert_align == 0) {
        cur_y -= 0.5f * (atlas.ascent + atlas.descent);
    }
    else if (text.vert_align > 0) {
    }
    else if (text.vert_align < 0) {
        cur_y -= (atlas.ascent + atlas.descent);
    }
    else {
        golf_log_warning("Invalid text vert_align %s", text.vert_align);
    }

    i = 0;
    while (text.string[i]) {
        char c = text.string[i];

        int x0 = (int)atlas.char_data[(int)c].x0;
        int x1 = (int)atlas.char_data[(int)c].x1;
        int y0 = (int)atlas.char_data[(int)c].y0;
        int y1 = (int)atlas.char_data[(int)c].y1;
        float xoff = atlas.char_data[(int)c].xoff;
        float yoff = atlas.char_data[(int)c].yoff;
        float xadvance = atlas.char_data[(int)c].xadvance;

        int round_x = (int)floor((cur_x + xoff) + 0.5f);
        int round_y = (int)floor((cur_y - yoff) + 0.5f);

        float qx0 = (float)round_x; 
        float qy0 = (float)round_y;
        float qx1 = (float)(round_x + x1 - x0);
        float qy1 = (float)(round_y - (y1 - y0));

        vec3 translate;
        translate.x = qx0 + 0.5f * (qx1 - qx0);
        translate.y = qy0 + 0.5f * (qy1 - qy0);
        translate.z = 0.0f;

        vec3 scale;
        scale.x = sz_scale * 0.5f * (qx1 - qx0);
        scale.y = sz_scale * 0.5f * (qy1 - qy0);
        scale.z = 1.0f;

        ui_sprite_vs_params_t vs_params = {
            .mvp_mat = mat4_transpose(mat4_multiply_n(3,
                        renderer.ui_proj_mat,
                        mat4_translation(translate),
                        mat4_scale(scale)))

        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_ui_sprite_vs_params,
                &(sg_range) { &vs_params, sizeof(vs_params) } );

        ui_sprite_fs_params_t fs_params = {
            .tex_x = (float) x0 / atlas.size,
            .tex_y = (float) y0 / atlas.size, 
            .tex_dx = (float) (x1 - x0) / atlas.size, 
            .tex_dy = (float) (y1 - y0) / atlas.size,
            .is_font = 1.0f,
            .color = text.color,

        };
        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ui_sprite_fs_params,
                &(sg_range) { &fs_params, sizeof(fs_params) });

        sg_draw(0, square_model->positions.length, 1);

        cur_x += sz_scale * xadvance;

        i++;
    }
}

static void _draw_ui_pixel_pack_square_section(vec2 pos, vec2 size, float tile_screen_size, golf_pixel_pack_t *pixel_pack, golf_pixel_pack_square_t *pixel_pack_square, int x, int y, vec4 overlay_color) {
    golf_model_t *square_model = golf_data_get_model("data/models/ui_sprite_square.obj");

    float px = pos.x + x * (0.5f * size.x - 0.5f * tile_screen_size);
    float py = pos.y + y * (0.5f * size.y - 0.5f * tile_screen_size);

    float sx = 0.5f * tile_screen_size;
    float sy = 0.5f * tile_screen_size;
    if (x == 0) {
        sx = 0.5f*size.x - 2.0f;
    }
    if (y == 0) {
        sy = 0.5f*size.y - 2.0f;
    }

    vec2 uv0, uv1;
    if (x == -1 && y == -1) {
        uv0 = pixel_pack_square->bl_uv0;
        uv1 = pixel_pack_square->bl_uv1;
    }
    else if (x == -1 && y == 0) {
        uv0 = pixel_pack_square->ml_uv0;
        uv1 = pixel_pack_square->ml_uv1;
    }
    else if (x == -1 && y == 1) {
        uv0 = pixel_pack_square->tl_uv0;
        uv1 = pixel_pack_square->tl_uv1;
    }
    else if (x == 0 && y == -1) {
        uv0 = pixel_pack_square->bm_uv0;
        uv1 = pixel_pack_square->bm_uv1;
    }
    else if (x == 0 && y == 0) {
        uv0 = pixel_pack_square->mm_uv0;
        uv1 = pixel_pack_square->mm_uv1;
    }
    else if (x == 0 && y == 1) {
        uv0 = pixel_pack_square->tm_uv0;
        uv1 = pixel_pack_square->tm_uv1;
    }
    else if (x == 1 && y == -1) {
        uv0 = pixel_pack_square->br_uv0;
        uv1 = pixel_pack_square->br_uv1;
    }
    else if (x == 1 && y == 0) {
        uv0 = pixel_pack_square->mr_uv0;
        uv1 = pixel_pack_square->mr_uv1;
    }
    else if (x == 1 && y == 1) {
        uv0 = pixel_pack_square->tr_uv0;
        uv1 = pixel_pack_square->tr_uv1;
    }
    else {
        golf_log_warning("Invalid x and y for pixel pack square section");
    }

    ui_sprite_vs_params_t vs_params = {
        .mvp_mat = mat4_transpose(mat4_multiply_n(3,
                    renderer.ui_proj_mat,
                    mat4_translation(V3(px, py, 0.0f)),
                    mat4_scale(V3(sx, sy, 1.0))))

    };
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_ui_sprite_vs_params,
            &(sg_range) { &vs_params, sizeof(vs_params) } );

    ui_sprite_fs_params_t fs_params = {
        .tex_x = uv0.x,
        .tex_y = uv1.y, 
        .tex_dx = uv1.x - uv0.x, 
        .tex_dy = -(uv1.y - uv0.y),
        .is_font = 0.0f,
        .color = overlay_color,

    };
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ui_sprite_fs_params,
            &(sg_range) { &fs_params, sizeof(fs_params) });

    sg_draw(0, square_model->positions.length, 1);
}

static void _draw_ui_pixel_pack_square(golf_ui_pixel_pack_square_t square) {
    golf_model_t *square_model = golf_data_get_model("data/models/ui_sprite_square.obj");
    golf_pixel_pack_t *pixel_pack = square.pixel_pack;
    golf_pixel_pack_square_t *pixel_pack_square = square.square;

    sg_bindings bindings = {
        .vertex_buffers[ATTR_ui_sprite_vs_position] = square_model->sg_positions_buf,
        .vertex_buffers[ATTR_ui_sprite_vs_texture_coord] = square_model->sg_texcoords_buf,
        .fs_images[SLOT_ui_sprite_texture] = pixel_pack->texture->sg_image,
    };
    sg_apply_bindings(&bindings);

    _draw_ui_pixel_pack_square_section(square.pos, square.size, square.tile_screen_size, pixel_pack, pixel_pack_square, 0, 0, square.overlay_color);
    _draw_ui_pixel_pack_square_section(square.pos, square.size, square.tile_screen_size, pixel_pack, pixel_pack_square, 0, -1, square.overlay_color);
    _draw_ui_pixel_pack_square_section(square.pos, square.size, square.tile_screen_size, pixel_pack, pixel_pack_square, 0, 1, square.overlay_color);
    _draw_ui_pixel_pack_square_section(square.pos, square.size, square.tile_screen_size, pixel_pack, pixel_pack_square, -1, 0, square.overlay_color);
    _draw_ui_pixel_pack_square_section(square.pos, square.size, square.tile_screen_size, pixel_pack, pixel_pack_square, 1, 0, square.overlay_color);
    _draw_ui_pixel_pack_square_section(square.pos, square.size, square.tile_screen_size, pixel_pack, pixel_pack_square, -1, -1, square.overlay_color);
    _draw_ui_pixel_pack_square_section(square.pos, square.size, square.tile_screen_size, pixel_pack, pixel_pack_square, 1, -1, square.overlay_color);
    _draw_ui_pixel_pack_square_section(square.pos, square.size, square.tile_screen_size, pixel_pack, pixel_pack_square, -1, 1, square.overlay_color);
    _draw_ui_pixel_pack_square_section(square.pos, square.size, square.tile_screen_size, pixel_pack, pixel_pack_square, 1, 1, square.overlay_color);
}

static void _draw_ui_pixel_pack_icon(golf_ui_pixel_pack_icon_t icon) {
    golf_model_t *square_model = golf_data_get_model("data/models/ui_sprite_square.obj");
    golf_pixel_pack_t *pixel_pack = icon.pixel_pack;
    golf_pixel_pack_icon_t *pixel_pack_icon = icon.icon;

    sg_bindings bindings = {
        .vertex_buffers[ATTR_ui_sprite_vs_position] = square_model->sg_positions_buf,
        .vertex_buffers[ATTR_ui_sprite_vs_texture_coord] = square_model->sg_texcoords_buf,
        .fs_images[SLOT_ui_sprite_texture] = pixel_pack->texture->sg_image,
    };
    sg_apply_bindings(&bindings);

    float px = icon.pos.x; 
    float py = icon.pos.y;

    float sx = 0.5f * icon.size.x;
    float sy = 0.5f * icon.size.y;

    ui_sprite_vs_params_t vs_params = {
        .mvp_mat = mat4_transpose(mat4_multiply_n(3,
                    renderer.ui_proj_mat,
                    mat4_translation(V3(px, py, 0.0f)),
                    mat4_scale(V3(sx, sy, 1.0))))

    };
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_ui_sprite_vs_params,
            &(sg_range) { &vs_params, sizeof(vs_params) } );

    vec2 uv0 = pixel_pack_icon->uv0;
    vec2 uv1 = pixel_pack_icon->uv1;

    ui_sprite_fs_params_t fs_params = {
        .tex_x = uv0.x,
        .tex_y = uv1.y, 
        .tex_dx = uv1.x - uv0.x, 
        .tex_dy = -(uv1.y - uv0.y),
        .is_font = 0.0f,
        .color = icon.overlay_color,

    };
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ui_sprite_fs_params,
            &(sg_range) { &fs_params, sizeof(fs_params) });

    sg_draw(0, square_model->positions.length, 1);
}

static void _draw_ui_scroll_list_begin(golf_ui_scroll_list_t scroll_list) {
    vec2 p = scroll_list.pos;
    vec2 s = scroll_list.size;
    sg_apply_scissor_rectf(p.x - 0.5f * s.x, p.y - 0.5f * s.y, s.x, s.y, false);
}

static void _draw_ui_scroll_list_end(void) {
    sg_apply_scissor_rectf(0, 0, 1280, 720, false);
}

static void _draw_ui(void) {
    golf_ui_t *ui = golf_ui_get();

    {
        sg_pass_action action = {
            .colors[0] = {
                .action = SG_ACTION_DONTCARE,
                .value = { 0.529f, 0.808f, 0.922f, 1.0f },
            },
        };
        sg_begin_default_pass(&action, sapp_width(), sapp_height());

        sg_apply_pipeline(renderer.ui_sprites_pipeline);
    }

    for (int i = 0; i < ui->entities.length; i++) {
        golf_ui_entity_t entity = ui->entities.data[i];
        switch (entity.type) {
            case GOLF_UI_PIXEL_PACK_SQUARE:
                _draw_ui_pixel_pack_square(entity.pixel_pack_square);
                break;
            case GOLF_UI_PIXEL_PACK_ICON:
                _draw_ui_pixel_pack_icon(entity.pixel_pack_icon);
                break;
            case GOLF_UI_TEXT:
                _draw_ui_text(entity.text);
                break;
            case GOLF_UI_SCROLL_LIST_BEGIN:
                _draw_ui_scroll_list_begin(entity.scroll_list);
                break;
            case GOLF_UI_SCROLL_LIST_END:
                _draw_ui_scroll_list_end();
                break;
        }
    }

    sg_end_pass();
}

void golf_renderer_draw(void) {
    _set_ui_proj_mat(V2(0.0f, 0.0f));
    _draw_ui();
}

static void _golf_renderer_draw_solid_color_material(golf_model_t *model, int start, int count, mat4 model_mat, golf_material_t material) {
    solid_color_material_vs_params_t vs_params = {
        .mvp_mat = mat4_transpose(mat4_multiply(renderer.proj_view_mat, model_mat)),
    };
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_solid_color_material_vs_params,
            &(sg_range) { &vs_params, sizeof(vs_params) });

    solid_color_material_fs_params_t fs_params = {
        .color = material.color,
    };
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_solid_color_material_fs_params,
            &(sg_range) { &fs_params, sizeof(fs_params) });

    sg_bindings bindings = {
        .vertex_buffers[0] = model->sg_positions_buf,
    };
    sg_apply_bindings(&bindings);

    sg_draw(start, count, 1);
}

static void _golf_renderer_draw_environment_material(golf_model_t *model, int start, int count, mat4 model_mat, golf_material_t material, golf_lightmap_image_t *lightmap_image, golf_lightmap_section_t *lightmap_section, bool movement_repeats) {
    environment_material_vs_params_t vs_params = {
        .proj_view_mat = mat4_transpose(renderer.proj_view_mat),
        .model_mat = mat4_transpose(model_mat),
    };
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_environment_material_vs_params,
            &(sg_range) { &vs_params, sizeof(vs_params) });

    int num_samples = lightmap_image->num_samples;
    int sample0 = 0;
    int sample1 = 0;
    float lightmap_t = 0;
    if (lightmap_image->time_length > 0) {
        float t = lightmap_image->cur_time / lightmap_image->time_length;
        if (movement_repeats) {
            t = 2 * t;
            if (t > 1) {
                t = 2 - t;
            }
        }

        for (int i = 1; i < num_samples; i++) {
            if (t < i / ((float) (num_samples - 1))) {
                sample0 = i - 1;
                sample1 = i;
                break;
            }

            float lightmap_t0 = sample0 / ((float) num_samples - 1);
            float lightmap_t1 = sample1 / ((float) num_samples - 1);
            lightmap_t = (t - lightmap_t0) / (lightmap_t1 - lightmap_t0);
            lightmap_t = golf_clampf(lightmap_t, 0, 1);
        }
    }

    environment_material_fs_params_t fs_params = {
        .lightmap_texture_a = lightmap_t,
    };
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_solid_color_material_fs_params,
            &(sg_range) { &fs_params, sizeof(fs_params) });

    sg_bindings bindings = {
        .vertex_buffers[0] = model->sg_positions_buf,
        .vertex_buffers[1] = model->sg_texcoords_buf,
        .vertex_buffers[2] = model->sg_normals_buf,
        .vertex_buffers[3] = lightmap_section->sg_uvs_buf,
        .fs_images[SLOT_environment_material_texture] = material.texture->sg_image,
        .fs_images[SLOT_environment_material_lightmap_texture0] = lightmap_image->sg_image[sample0],
        .fs_images[SLOT_environment_material_lightmap_texture1] = lightmap_image->sg_image[sample1],
    };
    sg_apply_bindings(&bindings);

    sg_draw(start, count, 1);
}

static void _golf_renderer_draw_with_material(golf_model_t *model, int start, int count, mat4 model_mat, golf_material_t material) {
    switch (material.type) {
        case GOLF_MATERIAL_TEXTURE: {
            texture_material_vs_params_t vs_params = {
                .proj_view_mat = mat4_transpose(renderer.proj_view_mat),
                .model_mat = mat4_transpose(model_mat),
            };
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_texture_material_vs_params, 
                    &(sg_range) { &vs_params, sizeof(vs_params) });

            sg_bindings bindings = {
                .vertex_buffers[0] = model->sg_positions_buf,
                .vertex_buffers[1] = model->sg_texcoords_buf,
                .vertex_buffers[2] = model->sg_normals_buf,
                .fs_images[SLOT_texture_material_tex] = material.texture->sg_image,
            };
            sg_apply_bindings(&bindings);

            sg_draw(start, count, 1);
            break;
        }
        case GOLF_MATERIAL_COLOR: {
            solid_color_material_vs_params_t vs_params = {
                .mvp_mat = mat4_transpose(mat4_multiply(renderer.proj_view_mat, model_mat)),
            };
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_solid_color_material_vs_params,
                    &(sg_range) { &vs_params, sizeof(vs_params) });

            solid_color_material_fs_params_t fs_params = {
                .color = material.color,
            };
            sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_solid_color_material_fs_params,
                    &(sg_range) { &fs_params, sizeof(fs_params) });

            sg_bindings bindings = {
                .vertex_buffers[0] = model->sg_positions_buf,
            };
            sg_apply_bindings(&bindings);

            sg_draw(start, count, 1);
            break;
        }
        case GOLF_MATERIAL_DIFFUSE_COLOR: {
            break;
        }
        case GOLF_MATERIAL_ENVIRONMENT: {
            break;
        }
    }
}

/*
static void _golf_renderer_draw_model(golf_model_t *model, mat4 model_mat, golf_lightmap_t *lightmap, golf_material_t *material_override, golf_level_t *level) {
    for (int i = 0; i < model->groups.length; i++) {
        golf_model_group_t group = model->groups.data[i];
        golf_material_t material;
        if (material_override) {
            material = *material_override;
        }
        else if (!golf_level_get_material(level, group.material_name, &material)) {
            golf_log_warning("Could not find material %s", group.material_name);
            material.type = GOLF_MATERIAL_TEXTURE;
            material.texture = golf_data_get_texture("data/textures/fallback.png");
        }

        switch (material.type) {
            case GOLF_MATERIAL_TEXTURE: {
                sg_apply_pipeline(renderer.environment_material_pipeline);

                environment_vs_params_t vs_params = {
                    .proj_view_mat = mat4_transpose(renderer.proj_view_mat),
                    .model_mat = mat4_transpose(model_mat),
                };
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_environment_vs_params, 
                        &(sg_range) { &vs_params, sizeof(vs_params) });

                sg_bindings bindings = {
                    .vertex_buffers[0] = model->sg_positions_buf,
                    .vertex_buffers[1] = model->sg_texcoords_buf,
                    .vertex_buffers[2] = model->sg_normals_buf,
                    .vertex_buffers[3] = lightmap->sg_uvs_buf,
                    .fs_images[SLOT_kd_texture] = material.texture->sg_image,
                    .fs_images[SLOT_lightmap_texture] = lightmap->sg_image,
                };
                sg_apply_bindings(&bindings);

                sg_draw(group.start_vertex, group.vertex_count, 1);

                break;
            }
            case GOLF_MATERIAL_COLOR: {
                sg_apply_pipeline(renderer.solid_color_material_pipeline);

                solid_color_material_vs_params_t vs_params = {
                    .mvp_mat = mat4_transpose(mat4_multiply(renderer.proj_view_mat, model_mat)),
                };
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_solid_color_material_vs_params,
                        &(sg_range) { &vs_params, sizeof(vs_params) });

                solid_color_material_fs_params_t fs_params = {
                    .color = material.color,
                };
                sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_solid_color_material_fs_params,
                        &(sg_range) { &fs_params, sizeof(fs_params) });

                sg_bindings bindings = {
                    .vertex_buffers[0] = model->sg_positions_buf,
                };
                sg_apply_bindings(&bindings);

                sg_draw(group.start_vertex, group.vertex_count, 1);

                break;
            }
            case GOLF_MATERIAL_DIFFUSE_COLOR: {
                sg_apply_pipeline(renderer.diffuse_color_material_pipeline);

                diffuse_color_material_vs_params_t vs_params = {
                    .proj_view_mat = mat4_transpose(renderer.proj_view_mat),
                    .model_mat =mat4_transpose(model_mat), 
                };
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_diffuse_color_material_vs_params,
                        &(sg_range) { &vs_params, sizeof(vs_params) });

                diffuse_color_material_fs_params_t fs_params = {
                    .color = material.color,
                };
                sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_diffuse_color_material_fs_params,
                        &(sg_range) { &fs_params, sizeof(fs_params) });

                sg_bindings bindings = {
                    .vertex_buffers[0] = model->sg_positions_buf,
                    .vertex_buffers[1] = model->sg_normals_buf,
                };
                sg_apply_bindings(&bindings);

                sg_draw(group.start_vertex, group.vertex_count, 1);

                break;
            }
        }
    }
}
*/

void golf_renderer_draw_editor(void) {
    golf_editor_t *editor = golf_editor_get();
    golf_config_t *editor_cfg = golf_data_get_config("data/config/editor.cfg");

    {
        float theta = renderer.cam_inclination_angle;
        float phi = renderer.cam_azimuth_angle;
        renderer.cam_dir.x = sinf(theta) * cosf(phi);
        renderer.cam_dir.y = cosf(theta);
        renderer.cam_dir.z = sinf(theta) * sinf(phi);

        float near = 0.1f;
        float far = 150.0f;
        renderer.proj_mat = mat4_perspective_projection(66.0f,
                renderer.viewport_size.x / renderer.viewport_size.y, near, far);
        renderer.view_mat = mat4_look_at(renderer.cam_pos,
                vec3_add(renderer.cam_pos, renderer.cam_dir), renderer.cam_up);
        renderer.proj_view_mat = mat4_multiply(renderer.proj_mat, renderer.view_mat);
    }

    sg_pass_action action = {
        .colors[0] = {
            .action = SG_ACTION_DONTCARE,
            .value = { 0.529f, 0.808f, 0.922f, 1.0f },
        },
    };
    sg_begin_default_pass(&action, sapp_width(), sapp_height());
    sg_apply_viewportf(renderer.viewport_pos.x, renderer.viewport_pos.y, 
            renderer.viewport_size.x, renderer.viewport_size.y, true);

    for (int i = 0; i < editor->level->entities.length; i++) {
        golf_entity_t *entity = &editor->level->entities.data[i];
        if (!entity->active || entity->type == HOLE_ENTITY) continue;

        golf_model_t *model = golf_entity_get_model(entity);
        if (!model) continue;

        golf_transform_t *transform = golf_entity_get_transform(entity);
        if (!transform) continue;

        mat4 model_mat;
        golf_movement_t *movement = golf_entity_get_movement(entity);
        if (movement) {
            golf_transform_t moved_transform = golf_transform_apply_movement(*transform, *movement);
            model_mat = golf_transform_get_model_mat(moved_transform);
        }
        else {
            model_mat = golf_transform_get_model_mat(*transform);
        }

        for (int i = 0; i < model->groups.length; i++) {
            golf_model_group_t group = model->groups.data[i];
            golf_material_t material;
            if (!golf_level_get_material(editor->level, group.material_name, &material)) {
                golf_log_warning("Could not find material %s", group.material_name);
                material = golf_material_texture("data/textures/fallback.png");
            }

            switch (material.type) {
                case GOLF_MATERIAL_TEXTURE: {
                    sg_apply_pipeline(renderer.texture_material_pipeline);
                    _golf_renderer_draw_with_material(model, group.start_vertex, group.vertex_count, model_mat, material);
                    break;
                }
                case GOLF_MATERIAL_COLOR: {
                    break;
                }
                case GOLF_MATERIAL_DIFFUSE_COLOR: {
                    break;
                }
                case GOLF_MATERIAL_ENVIRONMENT: {
                    golf_lightmap_section_t *lightmap_section = golf_entity_get_lightmap_section(entity);
                    if (!lightmap_section) {
                        golf_log_warning("Cannot use environment material on entity with no lightmap");
                        break;
                    }

                    golf_lightmap_image_t lightmap_image;
                    if (!golf_level_get_lightmap_image(editor->level, lightmap_section->lightmap_name, &lightmap_image)) {
                        golf_log_warning("Could not find lightmap %s", lightmap_section->lightmap_name);
                        break;
                    }

                    sg_apply_pipeline(renderer.environment_material_pipeline);

                    bool movement_repeats = false;
                    if (movement) {
                        movement_repeats = movement->repeats;
                    }
                    _golf_renderer_draw_environment_material(model, group.start_vertex, group.vertex_count, model_mat, material, &lightmap_image, lightmap_section, movement_repeats);
                    break;
                }
            }
        }
    }

    if (editor->in_edit_mode) {
        golf_geo_t *geo = editor->edit_mode.geo;

        sg_apply_pipeline(renderer.solid_color_material_pipeline);
        for (int i = 0; i < geo->points.length; i++) {
            golf_geo_point_t p = geo->points.data[i];
            if (!p.active) continue;

            vec3 pos = vec3_apply_mat4(p.position, 1, editor->edit_mode.model_mat);
            float sz = vec3_distance(pos, renderer.cam_pos) / CFG_NUM(editor_cfg, "edit_mode_sphere_size");
            mat4 model_mat = mat4_multiply_n(2, 
                    mat4_translation(pos), 
                    mat4_scale(V3(sz, sz, sz)));
            golf_model_t *model = golf_data_get_model("data/models/sphere.obj");
            int start = 0;
            int count = model->positions.length;
            vec3 color = CFG_VEC3(editor_cfg, "edit_mode_selectable_color");
            golf_edit_mode_entity_t entity = golf_edit_mode_entity_point(i);
            if (golf_editor_is_edit_entity_hovered(entity)) {
                color = CFG_VEC3(editor_cfg, "edit_mode_hovered_color");
            }
            else if (golf_editor_is_edit_entity_selected(entity)) {  
                color = CFG_VEC3(editor_cfg, "edit_mode_selected_color");
            }
            golf_material_t material = golf_material_color(V4(color.x, color.y, color.z, 1));
            _golf_renderer_draw_solid_color_material(model, start, count, model_mat, material);
        }

        int start_vertex = 0;
        for (int i = 0; i < geo->faces.length; i++) {
            golf_geo_face_t face = geo->faces.data[i];
            if (!face.active) continue;

            int n = face.idx.length;
            for (int i = 0; i < n; i++) {
                int idx0 = face.idx.data[i];
                int idx1 = face.idx.data[(i + 1) % n];
                golf_geo_point_t p0 = geo->points.data[idx0];
                golf_geo_point_t p1 = geo->points.data[idx1];
                vec3 pos0 = vec3_apply_mat4(p0.position, 1, editor->edit_mode.model_mat);
                vec3 pos1 = vec3_apply_mat4(p1.position, 1, editor->edit_mode.model_mat);
                vec3 pos_avg = vec3_scale(vec3_add(pos0, pos1), 0.5f);
                float sz = vec3_distance(pos_avg, renderer.cam_pos) / CFG_NUM(editor_cfg, "edit_mode_line_size");
                vec3 color = CFG_VEC3(editor_cfg, "edit_mode_selectable_color");
                golf_edit_mode_entity_t entity = golf_edit_mode_entity_line(idx0, idx1);
                if (golf_editor_is_edit_entity_hovered(entity)) {
                    color = CFG_VEC3(editor_cfg, "edit_mode_hovered_color");
                    sz += 0.001f;
                }
                else if (golf_editor_is_edit_entity_selected(entity)) {
                    color = CFG_VEC3(editor_cfg, "edit_mode_selected_color");
                    sz += 0.001f;
                }
                mat4 model_mat = mat4_box_to_line_transform(pos0, pos1, sz);
                golf_model_t *model = golf_data_get_model("data/models/cube.obj");
                int start = 0;
                int count = model->positions.length;
                golf_material_t material = golf_material_color(V4(color.x, color.y, color.z, 1));
                _golf_renderer_draw_solid_color_material(model, start, count, model_mat, material);
            }

            golf_edit_mode_entity_t entity = golf_edit_mode_entity_face(i);
            if (golf_editor_is_edit_entity_hovered(entity)) {
                golf_model_t *model = &geo->model;
                mat4 model_mat = editor->edit_mode.model_mat;
                vec3 color = CFG_VEC3(editor_cfg, "edit_mode_hovered_color");
                float alpha = CFG_NUM(editor_cfg, "edit_mode_hovered_face_alpha");
                golf_material_t material = golf_material_color(V4(color.x, color.y, color.z, alpha));
                _golf_renderer_draw_solid_color_material(model, start_vertex, 3 * (n - 2), model_mat, material);
            }
            else if (golf_editor_is_edit_entity_selected(entity)) {
                golf_model_t *model = &geo->model;
                mat4 model_mat = editor->edit_mode.model_mat;
                vec3 color = CFG_VEC3(editor_cfg, "edit_mode_selected_color");
                float alpha = CFG_NUM(editor_cfg, "edit_mode_selected_face_alpha");
                golf_material_t material = golf_material_color(V4(color.x, color.y, color.z, alpha));
                _golf_renderer_draw_solid_color_material(model, start_vertex, 3 * (n - 2), model_mat, material);
            }
            start_vertex += 3 * (n - 2);
        }
    }

    sg_apply_pipeline(renderer.hole_pass1_pipeline);
    for (int i = 0; i < editor->level->entities.length; i++) {
        golf_entity_t *entity = &editor->level->entities.data[i];
        if (!entity->active) continue;

        switch (entity->type) {
            case MODEL_ENTITY:
            case BALL_START_ENTITY:
            case GEO_ENTITY:
                break;
            case HOLE_ENTITY: {
                golf_transform_t transform = entity->hole.transform;
                transform.position.y += 0.001f;
                mat4 model_mat = golf_transform_get_model_mat(transform);
                golf_model_t *model = golf_data_get_model("data/models/hole-cover.obj");

                pass_through_vs_params_t vs_params = {
                    .mvp_mat = mat4_transpose(mat4_multiply(renderer.proj_view_mat, model_mat)),
                };
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_pass_through_vs_params,
                        &(sg_range) { &vs_params, sizeof(vs_params) });
                sg_bindings bindings = {
                    .vertex_buffers[0] = model->sg_positions_buf,
                };
                sg_apply_bindings(&bindings);
                sg_draw(0, model->positions.length, 1);

                break;
            }
        }
    }

    sg_apply_pipeline(renderer.hole_pass2_pipeline);
    for (int i = 0; i < editor->level->entities.length; i++) {
        golf_entity_t *entity = &editor->level->entities.data[i];
        if (!entity->active) continue;

        switch (entity->type) {
            case MODEL_ENTITY:
            case BALL_START_ENTITY:
            case GEO_ENTITY:
                break;
            case HOLE_ENTITY: {
                golf_transform_t transform = entity->hole.transform;
                transform.position.y += 0.001f;
                mat4 model_mat = golf_transform_get_model_mat(transform);
                golf_model_t *model = golf_data_get_model("data/models/hole.obj");
                golf_material_t material = golf_material_texture("data/textures/hole_lightmap.png");
                _golf_renderer_draw_with_material(model, 0, model->positions.length, model_mat, material);
                break;
            }
        }
    }

    sg_end_pass();
}
