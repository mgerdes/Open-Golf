#include "golf2/renderer.h"

#include "3rd_party/fast_obj/fast_obj.h"
#include "3rd_party/sokol/sokol_gfx.h"
#include "mcore/maths.h"
#include "mcore/mimport.h"
#include "mcore/mlog.h"
#include "golf2/shaders/ui_sprite.glsl.h"
#include "golf2/ui.h"

typedef struct _model {
    char name[1024];
    vec_vec3_t positions;
    vec_vec2_t texcoords;
    vec_vec3_t normals;
    sg_buffer positions_buf;
    sg_buffer normals_buf;
    sg_buffer texcoords_buf;
} _model_t;

typedef map_t(_model_t) _map_model_t;
typedef map_t(sg_shader) _map_sg_shader_t;
typedef map_t(sg_pipeline) _map_sg_pipeline_t;

typedef struct _renderer {
    _map_sg_shader_t shaders_map;
    _map_sg_pipeline_t pipelines_map;
    _map_model_t models_map;
} _renderer_t;

static _renderer_t _renderer;

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
    map_set(&_renderer.shaders_map, mdatafile_get_name(file), shader);

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
    map_set(&_renderer.pipelines_map, "ui_sprites", pipeline);
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

static void _model_generate_buffers(_model_t *model) {
    sg_buffer_desc desc = {
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
    };

    desc.data.size = sizeof(vec3) * model->positions.length;
    desc.data.ptr = model->positions.data;
    model->positions_buf = sg_make_buffer(&desc);

    desc.data.size = sizeof(vec2) * model->texcoords.length;
    desc.data.ptr = model->texcoords.data;
    model->texcoords_buf = sg_make_buffer(&desc);

    desc.data.size = sizeof(vec3) * model->normals.length;
    desc.data.ptr = model->normals.data;
    model->normals_buf = sg_make_buffer(&desc);
}

static void _model_obj_import(mdatafile_t *file, void *udata) {
    const char *name = mdatafile_get_name(file);

    fastObjCallbacks callbacks;
    callbacks.file_open = _fast_obj_file_open;
    callbacks.file_close = _fast_obj_file_close;
    callbacks.file_read = _fast_obj_file_read;
    callbacks.file_size = _fast_obj_file_size;

    _model_t model;
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
    map_set(&_renderer.models_map, model.name, model);
}

void golf_renderer_init(void) {
    map_init(&_renderer.shaders_map);
    map_init(&_renderer.pipelines_map);
    map_init(&_renderer.models_map);
    mimport_add_importer(".obj" , _model_obj_import, NULL);
    mimport_add_importer(".glsl" , _shader_import, NULL);
}

void golf_renderer_draw(void) {
    golf_ui_draw();
}

void *golf_renderer_get_pipeline(const char *name) {
    return map_get(&_renderer.pipelines_map, name);
}
