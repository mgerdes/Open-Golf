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
    ui_pixel_pack.texture = ui_pixel_pack_data->texture;
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

void golf_renderer_draw(void) {
}
