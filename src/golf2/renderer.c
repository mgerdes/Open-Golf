#include "golf2/renderer.h"

#include <assert.h>

#include "3rd_party/stb/stb_image.h"
#include "3rd_party/sokol/sokol_app.h"
#include "3rd_party/sokol/sokol_gfx.h"
#include "mcore/maths.h"
#include "mcore/mdata.h"
#include "mcore/mimport.h"
#include "mcore/mlog.h"
#include "mcore/mstring.h"
#include "golf2/shaders/ui_sprite.glsl.h"
#include "golf2/ui.h"

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

static void _load_shader_make_sg_shader(const char *path, mdata_shader_t *shader_data, const sg_shader_desc *const_shader_desc, sg_shader *shader) {
    const char *fs = NULL, *vs = NULL;
#if SOKOL_GLCORE33
    fs = shader_data->glsl330.fs;
    vs = shader_data->glsl330.vs;
#elif SOKOL_GLES3
    fs = shader_data->glsl300es.fs;
    vs = shader_data->glsl300es.vs;
#endif

    if (!fs || !vs) {
        mlog_error("Unable to load shader %s", path);
    }

    sg_shader_desc shader_desc = *const_shader_desc;
    shader_desc.fs.source = fs;
    shader_desc.vs.source = vs;
    *shader = sg_make_shader(&shader_desc);
}

static void _load_shader_ui_sprite(const char *path, mdata_shader_t *shader_data) {
    sg_shader shader;
    _load_shader_make_sg_shader(path, shader_data, ui_sprite_shader_desc(sg_query_backend()), &shader);
    map_set(&renderer.shaders_map, path, shader);

    sg_pipeline pipeline = sg_make_pipeline(&(sg_pipeline_desc){
            .shader = shader,
            .layout = {
            .attrs = {
            [ATTR_ui_sprite_vs_position] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
            [ATTR_ui_sprite_vs_texture_coord] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
            },
            },
            .colors[0] = {
            .blend = {
            .enabled = true,
            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            },
            },
            });
    map_set(&renderer.pipelines_map, "ui_sprites", pipeline);
}

static bool _load_shader(const char *path, mdata_t data) {
    assert(data.type == MDATA_SHADER);

    mdata_shader_t *shader_data = data.shader;
    if (strcmp(path, "data/shaders/ui_sprite.glsl") == 0) {
        _load_shader_ui_sprite(path, shader_data);
    }
    else {
        mlog_warning("No importer for shader %s", path);
        return false;
    }

    return true;
}

static bool _unload_shader(const char *path, mdata_t data) {
    assert(data.type == MDATA_SHADER);
}

static bool _reload_shader(const char *path, mdata_t data) {
    assert(data.type == MDATA_SHADER);
}

static bool _load_texture(const char *path, mdata_t data) {
    assert(data.type == MDATA_TEXTURE);

    mdata_texture_t *texture_data = data.texture;
    int x, y, n;
    int force_channels = 4;
    stbi_set_flip_vertically_on_load(0);
    unsigned char *img_data = stbi_load_from_memory((unsigned char*) texture_data->data, texture_data->data_len, &x, &y, &n, force_channels);
    if (!img_data) {
        mlog_error("STB Failed to load image");
    }

    sg_filter filter;
    if (strcmp(texture_data->filter, "linear") == 0) {
        filter = SG_FILTER_LINEAR;
    }
    else if (strcmp(texture_data->filter, "nearest") == 0) {
        filter = SG_FILTER_NEAREST;
    }
    else {

    }
    sg_image_desc img_desc = {
        .width = x,
        .height = y,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = filter,
        .mag_filter = filter,
        .wrap_u = SG_WRAP_REPEAT,
        .wrap_v = SG_WRAP_REPEAT,
        .data.subimage[0][0] = {
            .ptr = img_data,
            .size = 4*sizeof(char)*x*y,
        },
    };

    golf_renderer_texture_t texture;
    texture.texture_data = texture_data;
    texture.width = x;
    texture.height = y;
    texture.sg_image = sg_make_image(&img_desc);
    map_set(&renderer.textures_map, path, texture);
}

static bool _unload_texture(const char *path, mdata_t data) {
    assert(data.type == MDATA_TEXTURE);
}

static bool _reload_texture(const char *path, mdata_t data) {
    assert(data.type == MDATA_TEXTURE);
}

static void _load_font_atlas(const char *path, mdata_font_atlas_t *atlas, sg_image *atlas_image) {
    int x, y, n;
    int force_channels = 4;
    stbi_set_flip_vertically_on_load(0);
    unsigned char *data = stbi_load_from_memory(atlas->bmp_data, atlas->bmp_data_len, &x, &y, &n, force_channels);
    if (!data) {
        mlog_error("STB Failed to load image");
    }

    *atlas_image = sg_make_image(&(sg_image_desc) {
            .width = atlas->bmp_size,
            .height = atlas->bmp_size,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
            .data.subimage[0][0] = {
            .ptr = data,
            .size = 4*sizeof(char)*x*y,
            },
            });

    free(data);
}

static bool _load_font(const char *path, mdata_t data) {
    assert(data.type == MDATA_FONT);
    mdata_font_t *font_data = data.font;

    golf_renderer_font_t font;
    font.font_data = font_data;
    _load_font_atlas(path, &font_data->atlases[0], &font.atlas_images[0]);
    _load_font_atlas(path, &font_data->atlases[1], &font.atlas_images[1]);
    _load_font_atlas(path, &font_data->atlases[2], &font.atlas_images[2]);
    map_set(&renderer.fonts_map, path, font);
}

static bool _unload_font(const char *path, mdata_t data) {
    assert(data.type == MDATA_FONT);
}

static bool _reload_font(const char *path, mdata_t data) {
    assert(data.type == MDATA_FONT);
}

static golf_renderer_font_t *_get_font(const char *path) {
    static const char *fallback = "data/font/DroidSerif-Bold.ttf";
    golf_renderer_font_t *font = map_get(&renderer.fonts_map, path);
    if (!font) {
        mlog_warning("Can't find font %s falling back to %s", path, fallback);
        font = map_get(&renderer.fonts_map, fallback);
        if (!font) {
            mlog_error("Can't find fallback %s", fallback);
        }
    }
    return font;
}

static bool _load_model(const char *path, mdata_t data) {
    assert(data.type == MDATA_MODEL);
    mdata_model_t *model_data = data.model;

    golf_renderer_model_t model;
    model.model_data = model_data;

    {
        sg_buffer_desc desc = {
            .type = SG_BUFFERTYPE_VERTEXBUFFER,
        };

        desc.data.size = sizeof(vec3) * model_data->positions.length;
        desc.data.ptr = model_data->positions.data;
        model.sg_positions_buf = sg_make_buffer(&desc);

        desc.data.size = sizeof(vec3) * model_data->normals.length;
        desc.data.ptr = model_data->normals.data;
        model.sg_normals_buf = sg_make_buffer(&desc);

        desc.data.size = sizeof(vec2) * model_data->texcoords.length;
        desc.data.ptr = model_data->texcoords.data;
        model.sg_texcoords_buf = sg_make_buffer(&desc);
    }

    map_set(&renderer.models_map, path, model);
}

static bool _unload_model(const char *path, mdata_t data) {
    assert(data.type == MDATA_MODEL);
}

static bool _reload_model(const char *path, mdata_t data) {
    assert(data.type == MDATA_MODEL);
}

static golf_renderer_model_t *_get_model(const char *path) {
    static const char *fallback = "data/models/ui_sprite_square.obj";
    golf_renderer_model_t *model = map_get(&renderer.models_map, path);
    if (!model) {
        mlog_warning("Can't find model %s falling back to %s", path, fallback);
        model = map_get(&renderer.models_map, fallback);
        if (!model) {
            mlog_error("Can't find fallback %s", fallback);
        }
    }
    return model;
}

static bool _load_ui_pixel_pack(const char *path, mdata_t data) {
    assert(data.type == MDATA_UI_PIXEL_PACK);
    mdata_ui_pixel_pack_t *ui_pixel_pack_data = data.ui_pixel_pack;

    golf_renderer_texture_t *texture = map_get(&renderer.textures_map, ui_pixel_pack_data->texture);
    if (!texture) {
        mlog_warning("Texture %s must be loaded to load ui pixel pack %s", ui_pixel_pack_data->texture, path);
        return false;
    }

    float s = ui_pixel_pack_data->tile_size;
    float p = ui_pixel_pack_data->tile_padding;
    float w = texture->width;
    float h = texture->height;

    golf_renderer_ui_pixel_pack_t ui_pixel_pack;
    ui_pixel_pack.texture = texture;
    map_init(&ui_pixel_pack.squares);
    map_init(&ui_pixel_pack.icons);

    for (int i = 0; i < ui_pixel_pack_data->squares.length; i++) {
        mdata_ui_pixel_pack_square_t square_data = ui_pixel_pack_data->squares.data[i];

        golf_renderer_ui_pixel_pack_square_t square; 
        square.tl.uv0 = V2(((s + p) * square_data.tl.x) / w, 		((s + p) * square_data.tl.y) / h);
        square.tl.uv1 = V2(((s + p) * square_data.tl.x + s) / w, 	((s + p) * square_data.tl.y + s) / h);
        square.tm.uv0 = V2(((s + p) * square_data.tm.x) / w, 		((s + p) * square_data.tm.y) / h);
        square.tm.uv1 = V2(((s + p) * square_data.tm.x + s) / w, 	((s + p) * square_data.tm.y + s) / h);
        square.tr.uv0 = V2(((s + p) * square_data.tr.x) / w, 		((s + p) * square_data.tr.y) / h);
        square.tr.uv1 = V2(((s + p) * square_data.tr.x + s) / w, 	((s + p) * square_data.tr.y + s) / h);

        square.ml.uv0 = V2(((s + p) * square_data.ml.x) / w, 		((s + p) * square_data.ml.y) / h);
        square.ml.uv1 = V2(((s + p) * square_data.ml.x + s) / w, 	((s + p) * square_data.ml.y + s) / h);
        square.mm.uv0 = V2(((s + p) * square_data.mm.x) / w, 		((s + p) * square_data.mm.y) / h);
        square.mm.uv1 = V2(((s + p) * square_data.mm.x + s) / w, 	((s + p) * square_data.mm.y + s) / h);
        square.mr.uv0 = V2(((s + p) * square_data.mr.x) / w, 		((s + p) * square_data.mr.y) / h);
        square.mr.uv1 = V2(((s + p) * square_data.mr.x + s) / w, 	((s + p) * square_data.mr.y + s) / h);

        square.bl.uv0 = V2(((s + p) * square_data.bl.x) / w, 		((s + p) * square_data.bl.y) / h);
        square.bl.uv1 = V2(((s + p) * square_data.bl.x + s) / w, 	((s + p) * square_data.bl.y + s) / h);
        square.bm.uv0 = V2(((s + p) * square_data.bm.x) / w, 		((s + p) * square_data.bm.y) / h);
        square.bm.uv1 = V2(((s + p) * square_data.bm.x + s) / w, 	((s + p) * square_data.bm.y + s) / h);
        square.br.uv0 = V2(((s + p) * square_data.br.x) / w, 		((s + p) * square_data.br.y) / h);
        square.br.uv1 = V2(((s + p) * square_data.br.x + s) / w, 	((s + p) * square_data.br.y + s) / h);

        map_set(&ui_pixel_pack.squares, square_data.name, square);
    }

    for (int i = 0; i < ui_pixel_pack_data->icons.length; i++) {
        mdata_ui_pixel_pack_icon_t icon_data = ui_pixel_pack_data->icons.data[i];
        golf_renderer_ui_pixel_pack_icon_t icon;
        icon.uv0 = V2(((s + p) * icon_data.x) / w, 		((s + p) * icon_data.y) / h);
        icon.uv1 = V2(((s + p) * icon_data.x + s) / w, 	((s + p) * icon_data.y + s) / h);
        map_set(&ui_pixel_pack.icons, icon_data.name, icon);
    }

    map_set(&renderer.ui_pixel_packs_map, path, ui_pixel_pack);
}

static bool _unload_ui_pixel_pack(const char *path, mdata_t data) {
    assert(data.type == MDATA_UI_PIXEL_PACK);
}

static bool _reload_ui_pixel_pack(const char *path, mdata_t data) {
    assert(data.type == MDATA_UI_PIXEL_PACK);

    golf_renderer_ui_pixel_pack_t *ui_pixel_pack = map_get(&renderer.ui_pixel_packs_map, path);
    if (!ui_pixel_pack) {
        mlog_warning("Reloading file that isn't loaded %s", path); 
        return false;
    }
    map_deinit(&ui_pixel_pack->squares);
    map_deinit(&ui_pixel_pack->icons);

    _load_ui_pixel_pack(path, data);
}

static golf_renderer_ui_pixel_pack_t *_get_ui_pixel_pack(const char *path) {
    static const char *fallback = "data/textures/UIpackSheet_transparent.ui_pixel_pack";
    golf_renderer_ui_pixel_pack_t *ui_pixel_pack = map_get(&renderer.ui_pixel_packs_map, path);
    if (!ui_pixel_pack) {
        mlog_warning("Can't find ui pixel pack %s falling back to %s", path, fallback);
        ui_pixel_pack = map_get(&renderer.ui_pixel_packs_map, fallback);
        if (!ui_pixel_pack) {
            mlog_error("Can't find fallback %s", fallback);
        }
    }
    return ui_pixel_pack;
}

golf_renderer_t *golf_renderer_get(void) {
    return &renderer;
}

void golf_renderer_init(void) {
    map_init(&renderer.shaders_map);
    map_init(&renderer.pipelines_map);
    map_init(&renderer.textures_map);
    map_init(&renderer.fonts_map);
    map_init(&renderer.models_map);
    map_init(&renderer.ui_pixel_packs_map);

    mdata_add_loader(MDATA_SHADER, _load_shader, _unload_shader, _reload_shader);
    mdata_add_loader(MDATA_TEXTURE, _load_texture, _unload_texture, _reload_texture);
    mdata_add_loader(MDATA_FONT, _load_font, _unload_font, _reload_font);
    mdata_add_loader(MDATA_MODEL, _load_model, _unload_model, _reload_model);
    mdata_add_loader(MDATA_UI_PIXEL_PACK, _load_ui_pixel_pack, _unload_ui_pixel_pack, _reload_ui_pixel_pack);
}

static void _draw_ui_text(golf_ui_text_t text) {
    golf_renderer_font_t *font = _get_font(text.font);
    golf_renderer_model_t *square_model = _get_model("data/models/ui_sprite_square.obj");

    float cur_x = text.pos.x;
    float cur_y = text.pos.y;
    int i = 0;

    int sz_idx = 0;
    for (int idx = 1; idx < 3; idx++) {
        if (fabsf(font->font_data->atlases[idx].font_size - text.size) <
                fabsf(font->font_data->atlases[sz_idx].font_size - text.size)) {
            sz_idx = idx;
        }
    }

    mdata_font_atlas_t atlas = font->font_data->atlases[sz_idx];

    float sz_scale = text.size / atlas.font_size;

    {
        sg_bindings bindings = {
            .vertex_buffers[ATTR_ui_sprite_vs_position] = square_model->sg_positions_buf,
            .vertex_buffers[ATTR_ui_sprite_vs_texture_coord] = square_model->sg_texcoords_buf,
            .fs_images[SLOT_ui_sprite_texture] = font->atlas_images[sz_idx],
        };
        sg_apply_bindings(&bindings);
    }

    float width = 0.0f;
    while (text.string[i]) {
        char c = text.string[i];
        width += sz_scale * atlas.char_data[c].xadvance;
        i++;
    }

    if (strcmp(text.horiz_align, "center") == 0) {
        cur_x -= 0.5f * width;
    }
    else if (strcmp(text.horiz_align, "left") == 0) {
    }
    else if (strcmp(text.horiz_align, "right") == 0) {
        cur_x -= width;
    }
    else {
        mlog_warning("Invalid text horizontal_alignment %s", text.horiz_align);
    }

    if (strcmp(text.vert_align, "center") == 0) {
        cur_y -= 0.5f * (font->font_data->atlases[sz_idx].ascent + font->font_data->atlases[sz_idx].descent);
    }
    else if (strcmp(text.vert_align, "top") == 0) {
    }
    else if (strcmp(text.vert_align, "bottom") == 0) {
        cur_y -= (font->font_data->atlases[sz_idx].ascent + font->font_data->atlases[sz_idx].descent);
    }
    else {
        mlog_warning("Invalid text vert_align %s", text.vert_align);
    }

    i = 0;
    while (text.string[i]) {
        char c = text.string[i];

        int x0 = atlas.char_data[c].x0;
        int x1 = atlas.char_data[c].x1;
        int y0 = atlas.char_data[c].y0;
        int y1 = atlas.char_data[c].y1;
        float xoff = atlas.char_data[c].xoff;
        float yoff = atlas.char_data[c].yoff;
        float xadvance = atlas.char_data[c].xadvance;

        int round_x = floor((cur_x + xoff) + 0.5f);
        int round_y = floor((cur_y - yoff) + 0.5f);

        float qx0 = round_x; 
        float qy0 = round_y;
        float qx1 = round_x + x1 - x0;
        float qy1 = round_y - (y1 - y0);

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
            .tex_x = (float) x0 / atlas.bmp_size,
            .tex_y = (float) y0 / atlas.bmp_size, 
            .tex_dx = (float) (x1 - x0) / atlas.bmp_size, 
            .tex_dy = (float) (y1 - y0) / atlas.bmp_size,
            .is_font = 1.0f,
            .color = text.color,

        };
        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ui_sprite_fs_params,
                &(sg_range) { &fs_params, sizeof(fs_params) });

        sg_draw(0, square_model->model_data->positions.length, 1);
        
        cur_x += sz_scale * atlas.char_data[c].xadvance;

        i++;
    }
}

static void _draw_ui_pixel_pack_square_section(vec2 pos, vec2 size, float tile_screen_size, golf_renderer_ui_pixel_pack_t *pixel_pack, golf_renderer_ui_pixel_pack_square_t *pixel_pack_square, int x, int y) {
    golf_renderer_model_t *square_model = _get_model("data/models/ui_sprite_square.obj");

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

    golf_renderer_ui_pixel_pack_icon_t tile; 
    if (x == -1 && y == -1) {
        tile = pixel_pack_square->bl;
    }
    else if (x == -1 && y == 0) {
        tile = pixel_pack_square->ml;
    }
    else if (x == -1 && y == 1) {
        tile = pixel_pack_square->tl;
    }
    else if (x == 0 && y == -1) {
        tile = pixel_pack_square->bm;
    }
    else if (x == 0 && y == 0) {
        tile = pixel_pack_square->mm;
    }
    else if (x == 0 && y == 1) {
        tile = pixel_pack_square->tm;
    }
    else if (x == 1 && y == -1) {
        tile = pixel_pack_square->br;
    }
    else if (x == 1 && y == 0) {
        tile = pixel_pack_square->mr;
    }
    else if (x == 1 && y == 1) {
        tile = pixel_pack_square->tr;
    }
    else {
        mlog_warning("Invalid x and y for pixel pack square section");
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
        .tex_x = tile.uv0.x,
        .tex_y = tile.uv1.y, 
        .tex_dx = tile.uv1.x - tile.uv0.x, 
        .tex_dy = -(tile.uv1.y - tile.uv0.y),
        .is_font = 0.0f,
        .color = V4(0.0f, 1.0f, 1.0f, 1.0f),

    };
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ui_sprite_fs_params,
            &(sg_range) { &fs_params, sizeof(fs_params) });

    sg_draw(0, square_model->model_data->positions.length, 1);
}

static void _draw_ui_pixel_pack_square(golf_ui_pixel_pack_square_t square) {
    golf_renderer_model_t *square_model = _get_model("data/models/ui_sprite_square.obj");
    golf_renderer_ui_pixel_pack_t *pixel_pack = _get_ui_pixel_pack(square.ui_pixel_pack);
    golf_renderer_ui_pixel_pack_square_t *pixel_pack_square = map_get(&pixel_pack->squares, square.square_name);
    if (!pixel_pack_square) {
        mlog_warning("Invalid pixel pack square %s in pixel pack %s", square.square_name, square.ui_pixel_pack);
        return;
    }

    sg_bindings bindings = {
        .vertex_buffers[ATTR_ui_sprite_vs_position] = square_model->sg_positions_buf,
        .vertex_buffers[ATTR_ui_sprite_vs_texture_coord] = square_model->sg_texcoords_buf,
        .fs_images[SLOT_ui_sprite_texture] = pixel_pack->texture->sg_image,
    };
    sg_apply_bindings(&bindings);

    _draw_ui_pixel_pack_square_section(square.pos, square.size, square.tile_screen_size, pixel_pack, pixel_pack_square, 0, 0);
    _draw_ui_pixel_pack_square_section(square.pos, square.size, square.tile_screen_size, pixel_pack, pixel_pack_square, 0, -1);
    _draw_ui_pixel_pack_square_section(square.pos, square.size, square.tile_screen_size, pixel_pack, pixel_pack_square, 0, 1);
    _draw_ui_pixel_pack_square_section(square.pos, square.size, square.tile_screen_size, pixel_pack, pixel_pack_square, -1, 0);
    _draw_ui_pixel_pack_square_section(square.pos, square.size, square.tile_screen_size, pixel_pack, pixel_pack_square, 1, 0);
    _draw_ui_pixel_pack_square_section(square.pos, square.size, square.tile_screen_size, pixel_pack, pixel_pack_square, -1, -1);
    _draw_ui_pixel_pack_square_section(square.pos, square.size, square.tile_screen_size, pixel_pack, pixel_pack_square, 1, -1);
    _draw_ui_pixel_pack_square_section(square.pos, square.size, square.tile_screen_size, pixel_pack, pixel_pack_square, -1, 1);
    _draw_ui_pixel_pack_square_section(square.pos, square.size, square.tile_screen_size, pixel_pack, pixel_pack_square, 1, 1);
}

static void _draw_ui_pixel_pack_square_button(golf_ui_pixel_pack_square_button_t button) {
    golf_renderer_model_t *square_model = _get_model("data/models/ui_sprite_square.obj");
    golf_renderer_ui_pixel_pack_t *pixel_pack = _get_ui_pixel_pack(button.ui_pixel_pack);

    const char *square_name = NULL;
    if (button.button.state == GOLF_UI_BUTTON_DOWN) {
        square_name = button.down_square_name;
    }
    else {
        square_name = button.up_square_name;
    }
    golf_renderer_ui_pixel_pack_square_t *pixel_pack_square = map_get(&pixel_pack->squares, square_name);
    if (!pixel_pack_square) {
        mlog_warning("Invalid pixel pack square %s in pixel pack %s", square_name, button.ui_pixel_pack);
        return;
    }

    sg_bindings bindings = {
        .vertex_buffers[ATTR_ui_sprite_vs_position] = square_model->sg_positions_buf,
        .vertex_buffers[ATTR_ui_sprite_vs_texture_coord] = square_model->sg_texcoords_buf,
        .fs_images[SLOT_ui_sprite_texture] = pixel_pack->texture->sg_image,
    };
    sg_apply_bindings(&bindings);

    _draw_ui_pixel_pack_square_section(button.button.pos, button.button.size, button.tile_screen_size, pixel_pack, pixel_pack_square, 0, 0);
    _draw_ui_pixel_pack_square_section(button.button.pos, button.button.size, button.tile_screen_size, pixel_pack, pixel_pack_square, 0, -1);
    _draw_ui_pixel_pack_square_section(button.button.pos, button.button.size, button.tile_screen_size, pixel_pack, pixel_pack_square, 0, 1);
    _draw_ui_pixel_pack_square_section(button.button.pos, button.button.size, button.tile_screen_size, pixel_pack, pixel_pack_square, -1, 0);
    _draw_ui_pixel_pack_square_section(button.button.pos, button.button.size, button.tile_screen_size, pixel_pack, pixel_pack_square, 1, 0);
    _draw_ui_pixel_pack_square_section(button.button.pos, button.button.size, button.tile_screen_size, pixel_pack, pixel_pack_square, -1, -1);
    _draw_ui_pixel_pack_square_section(button.button.pos, button.button.size, button.tile_screen_size, pixel_pack, pixel_pack_square, 1, -1);
    _draw_ui_pixel_pack_square_section(button.button.pos, button.button.size, button.tile_screen_size, pixel_pack, pixel_pack_square, -1, 1);
    _draw_ui_pixel_pack_square_section(button.button.pos, button.button.size, button.tile_screen_size, pixel_pack, pixel_pack_square, 1, 1);
}

static void _draw_ui_scroll_list_begin(golf_ui_scroll_list_t scroll_list) {
    vec2 p = scroll_list.pos;
    vec2 s = scroll_list.size;
    float o = scroll_list.offset;
    sg_apply_scissor_rectf(p.x - 0.5f * s.x, p.y - 0.5f * s.y, s.x, s.y, false);
    _set_ui_proj_mat(V2(p.x - 0.5f * s.x, p.y - 0.5f * s.y + o));
}

static void _draw_ui_scroll_list_end(void) {
    sg_apply_scissor_rectf(0, 0, 1280, 720, false);
    _set_ui_proj_mat(V2(0.0f, 0.0f));
}

static void _draw_ui(void) {
    golf_ui_t *ui = golf_ui_get();
    sg_pass_action action = {
        .colors[0] = {
            .action = SG_ACTION_DONTCARE,
            .value = { 0.529f, 0.808f, 0.922f, 1.0f },
        },
    };
    sg_begin_default_pass(&action, sapp_width(), sapp_height());

    sg_pipeline *ui_sprites_pipeline = map_get(&renderer.pipelines_map, "ui_sprites");
    if (!ui_sprites_pipeline) {
        mlog_error("Could not fine 'ui_sprites' pipeline");
    }
    sg_apply_pipeline(*ui_sprites_pipeline);

    golf_renderer_model_t *square_model = _get_model("data/models/ui_sprite_square.obj");

    for (int i = 0; i < ui->entities.length; i++) {
        golf_ui_entity_t entity = ui->entities.data[i];
        switch (entity.type) {
            case GOLF_UI_TEXT:
                _draw_ui_text(entity.text);
                break;
            case GOLF_UI_PIXEL_PACK_SQUARE:
                _draw_ui_pixel_pack_square(entity.pixel_pack_square);
                break;
            case GOLF_UI_PIXEL_PACK_SQUARE_BUTTON:
                _draw_ui_pixel_pack_square_button(entity.pixel_pack_square_button);
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
