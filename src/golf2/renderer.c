#include "golf2/renderer.h"

#include <assert.h>

#include "3rd_party/fast_obj/fast_obj.h"
#include "3rd_party/parson/parson.h"
#include "3rd_party/stb/stb_image.h"
#include "3rd_party/stb/stb_image_write.h"
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

static void _load_shader_base(const char *path, mdata_shader_t *shader_data, const sg_shader_desc *const_shader_desc, sg_shader *shader) {
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
    _load_shader_base(path, shader_data, ui_sprite_shader_desc(sg_query_backend()), &shader);
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

static void _load_font_atlas(const char *path, mdata_font_atlas_t *atlas, sg_image *atlas_image) {
    int x, y, n;
    int force_channels = 4;
    stbi_set_flip_vertically_on_load(0);
    unsigned char *data = stbi_load_from_memory(atlas->bmp_data, atlas->bmp_data_len, &x, &y, &n, force_channels);
    if (!data) {
        mlog_error("STB Failed to load image");
    }

    *atlas_image = sg_make_image(&(sg_image_desc) {
        .width = bitmap_size,
        .height = bitmap_size,
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
}

static bool _load_font(const char *path, mdata_t data) {
    assert(data.type == MDATA_FONT);
    mdata_font *font_data = data->font;

    golf_renderer_font_t font;
    _load_font_atlas(path, &font_data->atlases[0], &font->atlas_images[0]);
    _load_font_atlas(path, &font_data->atlases[1], &font->atlas_images[1]);
    _load_font_atlas(path, &font_data->atlases[2], &font->atlas_images[2]);
    map_set(&renderer.fonts_map, path, font);
}

static bool _unload_font(const char *path, mdata_t data) {
    assert(data.type == MDATA_FONT);
}

static bool _load_model(const char *path, mdata_t data) {
    assert(data.type == MDATA_MODEL);
}

static bool _unload_model(const char *path, mdata_t data) {
    assert(data.type == MDATA_MODEL);
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

    {
        mdata_loader_t loader;
        loader.type = MDATA_SHADER;
        loader.load = _load_shader;
        loader.unload = _unload_shader;
        mdata_add_loader(loader);
    }

    {
        mdata_loader_t loader;
        loader.type = MDATA_TEXTURE;
        loader.load = _load_texture;
        loader.unload = _unload_texture;
        mdata_add_loader(loader);
    }

    {
        mdata_loader_t loader;
        loader.type = MDATA_FONT;
        loader.load = _load_font;
        loader.unload = _unload_font;
        mdata_add_loader(loader);
    }

    {
        mdata_loader_t loader;
        loader.type = MDATA_MODEL;
        loader.load = _load_model;
        loader.unload = _unload_model;
        mdata_add_loader(loader);
    }
}

void golf_renderer_draw(void) {
}
