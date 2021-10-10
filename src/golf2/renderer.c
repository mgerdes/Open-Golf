#include "golf2/renderer.h"

#include "3rd_party/fast_obj/fast_obj.h"
#include "3rd_party/stb/stb_image.h"
#include "3rd_party/stb/stb_image_write.h"
#include "3rd_party/sokol/sokol_app.h"
#include "3rd_party/sokol/sokol_gfx.h"
#include "mcore/maths.h"
#include "mcore/mimport.h"
#include "mcore/mlog.h"
#include "golf2/shaders/ui_sprite.glsl.h"
#include "golf2/ui.h"

static golf_renderer_t renderer;

static bool _load_shader(mdatafile_t *file, const sg_shader_desc *const_shader_desc, sg_shader *shader) {
#if SOKOL_GLCORE33
    unsigned char *fs_data;
    int fs_data_len;
    if (!mdatafile_get_data(file, "glsl330_fs", &fs_data, &fs_data_len)) {
        mlog_warning("Failed to get fragment shader data for %s", mdatafile_get_name(file));
        return false;
    }

    unsigned char *vs_data;
    int vs_data_len;
    if (!mdatafile_get_data(file, "glsl330_vs", &vs_data, &vs_data_len)) {
        mlog_warning("Failed to get vertex shader data for %s", mdatafile_get_name(file));
        return false;
    }
#elif
    unsigned char *fs_data;
    int fs_data_len;
    if (!mdatafile_get_data(file, "glsl300es_fs", &fs_data, &fs_data_len)) {
        mlog_warning("Failed to get fragment shader data for %s", mdatafile_get_name(file));
        return false;
    }

    unsigned char *vs_data;
    int vs_data_len;
    if (!mdatafile_get_data(file, "glsl300es_vs", &vs_data, &vs_data_len)) {
        mlog_warning("Failed to get vertex shader data for %s", mdatafile_get_name(file));
        return false;
    }
#else
#error "Invalid sokol backend selected"
#endif

    sg_shader_desc shader_desc = *const_shader_desc;
    shader_desc.fs.source = fs_data;
    shader_desc.vs.source = vs_data;
    *shader = sg_make_shader(&shader_desc);
    return true;
} 

static void _ui_sprite_shader_import(mdatafile_t *file, void *udata) {
    sg_shader shader;
    if (!_load_shader(file, ui_sprite_shader_desc(sg_query_backend()), &shader)) {
        mlog_warning("Unable to load shader %s", mdatafile_get_name(file));
        return;
    }
    map_set(&renderer.shaders_map, mdatafile_get_name(file), shader);

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

static void _shader_import(mdatafile_t *file, void *udata) {
    const char *name = mdatafile_get_name(file);
    if (strcmp(name, "data/shaders/ui_sprite.glsl") == 0) {
        _ui_sprite_shader_import(file, udata);
    }
    else {
        mlog_warning("No importer for shader %s", name);
    }
}

static void _texture_import(mdatafile_t *file, void *udata) {
    const char *name = mdatafile_get_name(file);

    unsigned char *tex_data;
    int tex_data_len;
    if (!mdatafile_get_data(file, "data", &tex_data, &tex_data_len)) {
        mlog_error("Missing data field for texture mdatafile");
    }

    int x, y, n;
    int force_channels = 4;
    stbi_set_flip_vertically_on_load(0);
    unsigned char *data = stbi_load_from_memory((unsigned char*) tex_data, tex_data_len, &x, &y, &n, force_channels);
    if (!data) {
        mlog_error("STB Failed to load image");
    }
    
    sg_filter filter = SG_FILTER_LINEAR;
    const char *filter_string = NULL;
    if (mdatafile_get_string(file, "filter", &filter_string)) {
        if (strcmp(filter_string, "linear") == 0) {
            filter = SG_FILTER_LINEAR;
        }
        else if (strcmp(filter_string, "nearest") == 0) {
            filter = SG_FILTER_NEAREST;
        }
        else {

        }
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
            .ptr = data,
            .size = 4*sizeof(char)*x*y,
        },
    };

    golf_renderer_texture_t texture;
    texture.data = data;
    texture.width = x;
    texture.height = y;
    texture.sg_image = sg_make_image(&img_desc);
    map_set(&renderer.textures_map, name, texture);
}

static golf_renderer_texture_t *_renderer_get_texture(const char *path) {
    golf_renderer_texture_t *texture = map_get(&renderer.textures_map, path);
    if (!texture) {
        mlog_error("Could not find textures %s", path);
    }
    return texture;
}

typedef struct _fast_obj_file {
    mdatafile_t *mdatafile;
    unsigned char *data; 
    int data_len;
    int data_index;
} _fast_obj_file_t;

static void *_fast_obj_file_open(const char *path, void *user_data) {
    _fast_obj_file_t *file = malloc(sizeof(_fast_obj_file_t));
    file->mdatafile = mdatafile_load(path);
    if (!file->mdatafile) {
        mlog_error("Unable to open file %s", path);
        return NULL;
    }
    if (!mdatafile_get_data(file->mdatafile, "data", &file->data, &file->data_len)) {
        mlog_error("Unable to get data from mdatafile %s", path);
        return NULL;
    }
    file->data_index = 0;
    return file;
}

static void _fast_obj_file_close(void *file, void *user_data) {
    _fast_obj_file_t *fast_obj_file = (_fast_obj_file_t*)file;
    mdatafile_delete(fast_obj_file->mdatafile);
    free(file);
}

static size_t _fast_obj_file_read(void *file, void *dst, size_t bytes, void *user_data) {
    _fast_obj_file_t *fast_obj_file = (_fast_obj_file_t*)file;

    int bytes_to_read = (int)bytes;
    if (fast_obj_file->data_index + bytes_to_read > fast_obj_file->data_len) {
        bytes_to_read = fast_obj_file->data_len - fast_obj_file->data_index;
    }
    memcpy(dst, fast_obj_file->data + fast_obj_file->data_index, bytes_to_read);
    fast_obj_file->data_index += bytes_to_read;
    return bytes_to_read;
}

static unsigned long _fast_obj_file_size(void *file, void *user_data) {
    _fast_obj_file_t *fast_obj_file = (_fast_obj_file_t*)file;
    return fast_obj_file->data_len;
}

static void _model_generate_buffers(golf_renderer_model_t *model) {
    sg_buffer_desc desc = {
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
    };

    desc.data.size = sizeof(vec3) * model->positions.length;
    desc.data.ptr = model->positions.data;
    model->sg_positions_buf = sg_make_buffer(&desc);

    desc.data.size = sizeof(vec2) * model->texcoords.length;
    desc.data.ptr = model->texcoords.data;
    model->sg_texcoords_buf = sg_make_buffer(&desc);

    desc.data.size = sizeof(vec3) * model->normals.length;
    desc.data.ptr = model->normals.data;
    model->sg_normals_buf = sg_make_buffer(&desc);
}

static void _model_obj_import(mdatafile_t *file, void *udata) {
    const char *name = mdatafile_get_name(file);

    fastObjCallbacks callbacks;
    callbacks.file_open = _fast_obj_file_open;
    callbacks.file_close = _fast_obj_file_close;
    callbacks.file_read = _fast_obj_file_read;
    callbacks.file_size = _fast_obj_file_size;

    golf_renderer_model_t model;
    snprintf(model.name, 1024, "%s", name);
    vec_init(&model.positions);
    vec_init(&model.texcoords);
    vec_init(&model.normals);

    fastObjMesh *m = fast_obj_read_with_callbacks(name, &callbacks, NULL);
    for (int i = 0; i < m->group_count; i++) {
        const fastObjGroup grp = m->groups[i];

        int idx = 0;
        for (int j = 0; j < grp.face_count; j++) {
            int fv = m->face_vertices[grp.face_offset + j];
            if (fv != 3) {
                mlog_warning("OBJ file isn't triangulated %s", name); 
            }

            for (int k = 0; k < fv; k++) {
                fastObjIndex mi = m->indices[grp.index_offset + idx];

                vec3 p;
                p.x = m->positions[3 * mi.p + 0];
                p.y = m->positions[3 * mi.p + 1];
                p.z = m->positions[3 * mi.p + 2];

                vec2 t;
                t.x = m->texcoords[2 * mi.t + 0];
                t.y = m->texcoords[2 * mi.t + 1];

                vec3 n;
                n.x = m->normals[3 * mi.n + 0];
                n.y = m->normals[3 * mi.n + 1];
                n.z = m->normals[3 * mi.n + 2];

                vec_push(&model.positions, p);
                vec_push(&model.texcoords, t);
                vec_push(&model.normals, n);
                idx++;
            }
        }
    }
    fast_obj_destroy(m);
    _model_generate_buffers(&model);
    map_set(&renderer.models_map, model.name, model);
}

static golf_renderer_model_t *_renderer_get_model(const char *path) {
    golf_renderer_model_t *model = map_get(&renderer.models_map, path);
    if (!model) {
        mlog_error("Cannot find model %s", path);
    }
    return model;
}

static void _font_load_size(golf_renderer_font_t *font, mdatafile_t *file, int index,
        const char *bitmap_size_name, const char *bitmap_name, const char *char_data_name) {
    int bitmap_size;
    if (!mdatafile_get_int(file, bitmap_size_name, &bitmap_size)) {
        mlog_warning("Cannot find property %s on font mdatafile", bitmap_size_name);
    }

    unsigned char *bitmap_data;
    int bitmap_data_len;
    if (!mdatafile_get_data(file, bitmap_name, &bitmap_data, &bitmap_data_len)) {
        mlog_warning("Cannot find property %s on font mdatafile", bitmap_name);
    }

    int x, y, n;
    int force_channels = 4;
    stbi_set_flip_vertically_on_load(0);
    unsigned char *data = stbi_load_from_memory((unsigned char*) bitmap_data, bitmap_data_len, &x, &y, &n, force_channels);
    if (!data) {
        mlog_error("STB Failed to load image");
    }

    unsigned char *char_data;
    int char_data_len;
    if (!mdatafile_get_data(file, char_data_name, &char_data, &char_data_len)) {
        mlog_warning("Cannot find property %s on font mdatafile", char_data_name);
    }

    font->image_size[index] = bitmap_size;
    font->sg_image[index] = sg_make_image(&(sg_image_desc) {
        .width = bitmap_size,
        .height = bitmap_size,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .data.subimage[0][0] = {
            .ptr = data,
            .size = 4*sizeof(char)*x*y,
        },
    });
}

static void _font_import(mdatafile_t *file, void *udata) {
    const char *name = mdatafile_get_name(file);

    golf_renderer_font_t font;
    _font_load_size(&font, file, 0, "small_bitmap_size", "small_bitmap", "small_char_data");
    _font_load_size(&font, file, 1, "medium_bitmap_size", "medium_bitmap", "medium_char_data");
    _font_load_size(&font, file, 2, "large_bitmap_size", "large_bitmap", "large_char_data");
    map_set(&renderer.fonts_map, mdatafile_get_name(file), font);
}

static golf_renderer_font_t *_renderer_get_font(const char *path) {
    golf_renderer_font_t *font = map_get(&renderer.fonts_map, path);
    if (!font) {
        mlog_error("Cannot find font %s", path);
    }
    return font;
}

golf_renderer_t *golf_renderer_get(void) {
    return &renderer;
}

void golf_renderer_init(void) {
    map_init(&renderer.shaders_map);
    map_init(&renderer.pipelines_map);
    map_init(&renderer.models_map);
    map_init(&renderer.textures_map);
    map_init(&renderer.fonts_map);
    mimport_add_importer(".obj" , _model_obj_import, NULL);
    mimport_add_importer(".glsl" , _shader_import, NULL);
    mimport_add_importer(".png" , _texture_import, NULL);
    mimport_add_importer(".bmp" , _texture_import, NULL);
    mimport_add_importer(".jpg" , _texture_import, NULL);
    mimport_add_importer(".ttf" , _font_import, NULL);
}

static void _draw_ui_sprite_atlas(golf_ui_sprite_atlas_t sprite) {
    golf_renderer_model_t *square_model = _renderer_get_model("data/models/ui_sprite_square.obj");

    {
        sg_bindings bindings = {
            .vertex_buffers[ATTR_ui_sprite_vs_position] = square_model->sg_positions_buf,
            .vertex_buffers[ATTR_ui_sprite_vs_texture_coord] = square_model->sg_texcoords_buf,
            .fs_images[SLOT_ui_sprite_texture] = _renderer_get_texture(sprite.texture)->sg_image,
        };
        sg_apply_bindings(&bindings);
    }

    {
        float px = sprite.pos.x - (0.5f * sprite.size.x - 0.5f * sprite.tile_screen_size);
        float py = sprite.pos.y + (0.5f * sprite.size.y - 0.5f * sprite.tile_screen_size);
        float sx = 0.5f * sprite.tile_screen_size;
        float sy = 0.5f * sprite.tile_screen_size;

        ui_sprite_vs_params_t vs_params = {
            .mvp_mat = mat4_transpose(mat4_multiply_n(3,
                        renderer.ui_proj_mat,
                        mat4_translation(V3(px, py, 0.0f)),
                        mat4_scale(V3(sx, sy, 1.0))))

        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_ui_sprite_vs_params,
                &(sg_range) { &vs_params, sizeof(vs_params) } );

        ui_sprite_fs_params_t fs_params = {
            .tex_x = (sprite.tile_size + sprite.tile_padding) * sprite.tile_top.x,
            .tex_y = (sprite.tile_size + sprite.tile_padding) * sprite.tile_top.y + sprite.tile_size,
            .tex_dx = sprite.tile_size, 
            .tex_dy = -sprite.tile_size,
            .is_font = 0.0f,
            .color = V4(0.0f, 1.0f, 1.0f, 1.0f),

        };
        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ui_sprite_fs_params,
                &(sg_range) { &fs_params, sizeof(fs_params) });

        sg_draw(0, square_model->positions.length, 1);
    }

    {
        float px = sprite.pos.x;
        float py = sprite.pos.y;
        float sx = 0.5f * (sprite.size.x - sprite.tile_screen_size);
        float sy = 0.5f * (sprite.size.y - sprite.tile_screen_size);

        ui_sprite_vs_params_t vs_params = {
            .mvp_mat = mat4_transpose(mat4_multiply_n(3,
                        renderer.ui_proj_mat,
                        mat4_translation(V3(px, py, 0.0f)),
                        mat4_scale(V3(sx, sy, 1.0))))

        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_ui_sprite_vs_params,
                &(sg_range) { &vs_params, sizeof(vs_params) } );

        ui_sprite_fs_params_t fs_params = {
            .tex_x = (sprite.tile_size + sprite.tile_padding) * (sprite.tile_mid.x + 1),
            .tex_y = (sprite.tile_size + sprite.tile_padding) * sprite.tile_mid.y + sprite.tile_size,
            .tex_dx = sprite.tile_size, 
            .tex_dy = -sprite.tile_size,
            .is_font = 0.0f,
            .color = V4(0.0f, 1.0f, 1.0f, 1.0f),

        };
        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_ui_sprite_fs_params,
                &(sg_range) { &fs_params, sizeof(fs_params) });

        sg_draw(0, square_model->positions.length, 1);
    }
}

static void _draw_ui_text(golf_ui_text_t text) {
    float px = text.pos.x;
    float py = text.pos.y;
    int i = 0;
    while (text.text[i]) {
        i++;
    }
}

void golf_renderer_draw(void) {
    {
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
        renderer.ui_proj_mat = mat4_multiply_n(3,
                mat4_orthographic_projection(0.0f, w_width, 0.0f, w_height, 0.0f, 1.0f),
                mat4_translation(V3(0.5f*w_width - 0.5f*w_fb_width, 0.5f*w_height - 0.5f*w_fb_height, 0.0f)),
                mat4_scale(V3(w_fb_width/fb_width, w_fb_height/fb_height, 1.0f))
                );
    }

    {
        sg_pass_action action = {
            .colors[0] = {
                .action = SG_ACTION_DONTCARE,
                .value = { 0.529f, 0.808f, 0.922f, 1.0f },
            },
        };
        sg_begin_default_pass(&action, sapp_width(), sapp_height());

        golf_ui_t *ui = golf_ui_get();

        golf_ui_menu_t *main_menu = map_get(&ui->ui_menu_map, "data/ui/main_menu.ui_menu");
        if (!main_menu) {
            mlog_error("Could not find main_menu ui_menu");
        }

        sg_pipeline *ui_sprites_pipeline = map_get(&renderer.pipelines_map, "ui_sprites");
        if (!ui_sprites_pipeline) {
            mlog_error("Could not fine ui_sprites pipeline");
        }
        sg_apply_pipeline(*ui_sprites_pipeline);

        golf_renderer_model_t *square_model = _renderer_get_model("data/models/ui_sprite_square.obj");

        for (int i = 0; i < main_menu->entity_vec.length; i++) {
            golf_ui_entity_t entity = main_menu->entity_vec.data[i];
            switch (entity.type) {
                case GOLF_UI_ENTITY_SPRITE:
                    break;
                case GOLF_UI_ENTITY_BUTTON:
                    break;
                case GOLF_UI_ENTITY_SPRITE_ATLAS:
                    _draw_ui_sprite_atlas(entity.sprite_atlas);
                    break;
                case GOLF_UI_ENTITY_TEXT:
                    _draw_ui_text(entity.text);
                    break;
            }
        }

        sg_end_pass();
    }
}
