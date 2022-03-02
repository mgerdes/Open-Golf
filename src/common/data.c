#define _CRT_SECURE_NO_WARNINGS
#include "data.h"

#include <inttypes.h>

#include "fast_obj/fast_obj.h"
#include "mattiasgustavsson_libs/assetsys.h"
#include "parson/parson.h"
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#include "stb/stb_truetype.h"
#include "sokol/sokol_app.h"
#include "sokol/sokol_time.h"
#include "common/base64.h"
#include "common/common.h"
#include "common/file.h"
#include "common/graphics.h"
#include "common/json.h"
#include "common/level.h"
#include "common/log.h"
#include "common/map.h"
#include "common/maths.h"
#include "common/string.h"
#include "common/thread.h"

#if GOLF_PLATFORM_ANDROID | GOLF_PLATFORM_IOS
// CMake builds this file
#include "common/data_zip.h"
#endif

typedef enum _file_event_type {
    FILE_CREATED,
    FILE_UPDATED,
    FILE_LOADED,
} _file_event_type;

typedef struct _file_event {
    _file_event_type type;
    golf_file_t file;
} _file_event_t;
typedef vec_t(_file_event_t) vec_file_event_t;

static golf_thread_timer_t _main_thread_timer;
static golf_thread_timer_t _data_thread_timer;

static golf_mutex_t _loaded_data_lock;
static map_golf_data_t _loaded_data;

static golf_mutex_t _files_to_load_lock;
static vec_golf_file_t _files_to_load;

static golf_mutex_t _seen_files_lock;
static vec_golf_file_t _seen_files;

static golf_mutex_t _file_events_lock;
static vec_file_event_t _file_events;

static golf_mutex_t _assetsys_lock;
static assetsys_t *_assetsys;

static map_uint64_t _file_time_map;

static void _golf_data_thread_load_file(golf_file_t file);

static void _golf_data_add_dependency(vec_golf_file_t *deps, golf_file_t dep) {
    for (int i = 0; i < deps->length; i++) {
        if (strcmp(deps->data[i].path, dep.path) == 0) {
            return;
        }
    }
    vec_push(deps, dep);
}

//
// GIF TEXTURES
//

static bool _golf_gif_texture_finalize(void *ptr) {
    golf_gif_texture_t *texture = (golf_gif_texture_t*) ptr;

    for (int i = 0; i < texture->num_frames; i++) {
        sg_image_desc img_desc = {
            .width = texture->width,
            .height = texture->height,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .min_filter = texture->filter,
            .mag_filter = texture->filter,
            .wrap_u = SG_WRAP_REPEAT,
            .wrap_v = SG_WRAP_REPEAT,
            .data.subimage[0][0] = {
                .ptr = texture->image_data + i * 4 * texture->width * texture->height,
                .size = 4 * texture->width * texture->height,
            },
        };
        texture->sg_images[i] = sg_make_image(&img_desc);
    }

    free(texture->image_data);
    texture->image_data = NULL;

    return true;
}

static bool _golf_gif_texture_load(void *ptr, const char *path, char *data, int data_len, char *meta_data, int meta_data_len) {
    GOLF_UNUSED(path);
    GOLF_UNUSED(meta_data);
    GOLF_UNUSED(meta_data_len);

    golf_gif_texture_t *texture = (golf_gif_texture_t*) ptr;

    texture->filter = SG_FILTER_LINEAR;

    int *delays = NULL;
    int width, height, num_frames, n;
    stbi_set_flip_vertically_on_load(0);
    texture->image_data = stbi_load_gif_from_memory((unsigned char*)data, data_len, &delays, &width, &height, &num_frames, &n, 4);
    texture->width = width;
    texture->height = height;
    texture->num_frames = num_frames;
    texture->delays = delays;
    texture->sg_images = golf_alloc(sizeof(sg_image)*num_frames);

    return true;
}

static bool _golf_gif_texture_unload(void *ptr) {
    GOLF_UNUSED(ptr);
    return true;
}

//
// TEXTURES
//

static bool _golf_texture_finalize(void *ptr) {
    golf_texture_t* texture = (golf_texture_t*) ptr;
    sg_image_desc img_desc = {
        .width = texture->width,
        .height = texture->height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = texture->filter,
        .mag_filter = texture->filter,
        .wrap_u = SG_WRAP_REPEAT,
        .wrap_v = SG_WRAP_REPEAT,
        .data.subimage[0][0] = {
            .ptr = texture->image_data,
            .size = 4 * texture->width * texture->height,
        },
    };
    texture->sg_image = sg_make_image(&img_desc);
    free(texture->image_data);
    texture->image_data = NULL;
    return true;
}

static bool _golf_texture_load(void *ptr, const char *path, char *data, int data_len, char *meta_data, int meta_data_len) {
    GOLF_UNUSED(path);
    GOLF_UNUSED(data_len);
    GOLF_UNUSED(meta_data);
    GOLF_UNUSED(meta_data_len);

    golf_texture_t *texture = (golf_texture_t*) ptr;

    JSON_Value *meta_val = json_parse_string(meta_data);
    JSON_Object *meta_obj = json_value_get_object(meta_val);

    texture->filter = SG_FILTER_LINEAR;
    const char *filter_str = json_object_get_string(meta_obj, "filter");
    if (filter_str && strcmp(filter_str, "linear") == 0) {
        texture->filter = SG_FILTER_LINEAR;
    }
    else if (filter_str && strcmp(filter_str, "nearest") == 0) {
        texture->filter = SG_FILTER_NEAREST;
    }

    int width, height, n;
    stbi_set_flip_vertically_on_load(0);
    texture->image_data = stbi_load_from_memory((unsigned char*) data, data_len, &width, &height, &n, 4);
    if (!texture->image_data) {
        golf_log_error("STB Failed to load image");
    }
    texture->width = width;
    texture->height = height;

    json_value_free(meta_val);

    texture->width = width;
    texture->height = height;
    return true;
}

static bool _golf_texture_unload(void *ptr) {
    golf_texture_t* texture = (golf_texture_t*) ptr;
    sg_destroy_image(texture->sg_image);
    return true;
}

//
// SHADERS
//

static void _golf_shader_import_get_inputs(JSON_Array *inputs_arr, JSON_Value **my_inputs_val) {
    *my_inputs_val = json_value_init_array();
    JSON_Array *my_inputs_arr = json_value_get_array(*my_inputs_val);

    for (int i = 0; i < (int)json_array_get_count(inputs_arr); i++) {
        JSON_Value *my_input_val = json_value_init_object();
        JSON_Object *my_input_obj = json_value_get_object(my_input_val);

        JSON_Object *input_obj = json_array_get_object(inputs_arr, i);
        const char *name = json_object_get_string(input_obj, "name");
        int location = (int)json_object_get_number(input_obj, "location");

        json_object_set_string(my_input_obj, "name", name);
        json_object_set_number(my_input_obj, "location", location);
        json_array_append_value(my_inputs_arr, my_input_val);
    }
}

static void _golf_shader_import_get_uniforms(JSON_Array *uniforms_arr, JSON_Value **my_uniforms_val) {
    *my_uniforms_val = json_value_init_array();
    JSON_Array *my_uniforms_arr = json_value_get_array(*my_uniforms_val);

    for (int i = 0; i < (int)json_array_get_count(uniforms_arr); i++) {
        JSON_Value *my_uniform_val = json_value_init_object();
        JSON_Object *my_uniform_obj = json_value_get_object(my_uniform_val);

        JSON_Object *uniform_obj = json_array_get_object(uniforms_arr, i);
        const char *name = json_object_get_string(uniform_obj, "name");
        int size = (int)json_object_get_number(uniform_obj, "block_size");
        if (size % 16 != 0) {
            size = size + (16 - size % 16);
        }
        int binding = (int)json_object_get_number(uniform_obj, "binding");

        json_object_set_string(my_uniform_obj, "name", name);
        json_object_set_number(my_uniform_obj, "size", size);
        json_object_set_number(my_uniform_obj, "binding", binding);

        JSON_Value *my_members_val = json_value_init_array();
        JSON_Array *my_members_arr = json_value_get_array(my_members_val);

        JSON_Array *members_arr = json_object_get_array(uniform_obj, "members");
        for (int i = 0; i < (int)json_array_get_count(members_arr); i++) {
            JSON_Value *my_member_val = json_value_init_object();
            JSON_Object *my_member_obj = json_value_get_object(my_member_val);

            JSON_Object *member_obj = json_array_get_object(members_arr, i);
            const char *name = json_object_get_string(member_obj, "name");
            int offset = (int)json_object_get_number(member_obj, "offset");
            int size = (int)json_object_get_number(member_obj, "size");

            json_object_set_string(my_member_obj, "name", name);
            json_object_set_number(my_member_obj, "offset", offset);
            json_object_set_number(my_member_obj, "size", size);
            json_array_append_value(my_members_arr, my_member_val);
        }

        json_object_set_value(my_uniform_obj, "members", my_members_val);
        json_array_append_value(my_uniforms_arr, my_uniform_val);
    }
}

static void _golf_shader_import_get_textures(JSON_Array *textures_arr, JSON_Value **my_textures_val) {
    *my_textures_val = json_value_init_array();
    JSON_Array *my_textures_arr = json_value_get_array(*my_textures_val);

    for (int i = 0; i < (int)json_array_get_count(textures_arr); i++) {
        JSON_Value *my_texture_val = json_value_init_object();
        JSON_Object *my_texture_obj = json_value_get_object(my_texture_val);

        JSON_Object *texture_obj = json_array_get_object(textures_arr, i);
        const char *name = json_object_get_string(texture_obj, "name");
        int binding = (int)json_object_get_number(texture_obj, "binding");

        json_object_set_string(my_texture_obj, "name", name);
        json_object_set_number(my_texture_obj, "binding", binding);
        json_array_append_value(my_textures_arr, my_texture_val);
    }
}

static void _golf_shader_import_get_shader(const char *file_path, const char *lang, const char *profile, JSON_Value **my_val) {
    *my_val = json_value_init_object();
    JSON_Object *my_obj = json_value_get_object(*my_val);

    {
        golf_string_t cmd;
        golf_string_init(&cmd, "data", "");
#if GOLF_PLATFORM_LINUX
        golf_string_appendf(&cmd, "tools/glslcc/linux/glslcc %s -S -F -o out/temp/temp.glsl -l %s -p %s -r", file_path, lang, profile);
#elif GOLF_PLATFORM_WINDOWS
        golf_string_appendf(&cmd, "tools\\glslcc\\win64\\glslcc %s -S -F -o out/temp/temp.glsl -l %s -p %s -r", file_path, lang, profile);
#elif GOLF_PLATFORM_MACOS
#endif
        system(cmd.cstr);
        golf_string_deinit(&cmd);
    }

    {
        JSON_Value *my_fs_val = json_value_init_object();
        JSON_Object *my_fs_obj = json_value_get_object(my_fs_val);

        char *source_data;
        int source_data_len;
        if (golf_file_load_data("out/temp/temp_fs.glsl", &source_data, &source_data_len)) {
            json_object_set_string(my_fs_obj, "source", source_data);
        } 
        else {
            golf_log_warning("Unable to open source shader");
        }

        JSON_Value *fs_val = json_parse_file("out/temp/temp_fs.glsl.json");
        JSON_Object *fs_obj = json_value_get_object(fs_val);

        JSON_Value *my_inputs_val;
        JSON_Array *inputs_arr = json_object_dotget_array(fs_obj, "fs.inputs");
        _golf_shader_import_get_inputs(inputs_arr, &my_inputs_val);
        json_object_set_value(my_fs_obj, "inputs", my_inputs_val);

        JSON_Value *my_uniforms_val;
        JSON_Array *uniforms_arr = json_object_dotget_array(fs_obj, "fs.uniform_buffers");
        _golf_shader_import_get_uniforms(uniforms_arr, &my_uniforms_val);
        json_object_set_value(my_fs_obj, "uniforms", my_uniforms_val);

        JSON_Value *my_textures_val;
        JSON_Array *textures_arr = json_object_dotget_array(fs_obj, "fs.textures");
        _golf_shader_import_get_textures(textures_arr, &my_textures_val);
        json_object_set_value(my_fs_obj, "textures", my_textures_val);

        json_object_set_value(my_obj, "fs", my_fs_val);
    }

    {
        JSON_Value *my_vs_val = json_value_init_object();
        JSON_Object *my_vs_obj = json_value_get_object(my_vs_val);

        char *source_data;
        int source_data_len;
        if (golf_file_load_data("out/temp/temp_vs.glsl", &source_data, &source_data_len)) {
            json_object_set_string(my_vs_obj, "source", source_data);
        } 
        else {
            golf_log_warning("Unable to open source shader");
        }

        JSON_Value *vs_val = json_parse_file("out/temp/temp_vs.glsl.json");
        JSON_Object *vs_obj = json_value_get_object(vs_val);

        JSON_Value *my_inputs_val;
        JSON_Array *inputs_arr = json_object_dotget_array(vs_obj, "vs.inputs");
        _golf_shader_import_get_inputs(inputs_arr, &my_inputs_val);
        json_object_set_value(my_vs_obj, "inputs", my_inputs_val);

        JSON_Value *my_uniforms_val;
        JSON_Array *uniforms_arr = json_object_dotget_array(vs_obj, "vs.uniform_buffers");
        _golf_shader_import_get_uniforms(uniforms_arr, &my_uniforms_val);
        json_object_set_value(my_vs_obj, "uniforms", my_uniforms_val);

        json_object_set_value(my_obj, "vs", my_vs_val);
    }
}

static bool _golf_shader_import(const char *path, char *data, int data_len) {
    GOLF_UNUSED(data);
    GOLF_UNUSED(data_len);

#if GOLF_PLATFORM_LINUX | GOLF_PLATFORM_WINDOWS
    golf_file_t file = golf_file(path);
    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    JSON_Value *glsl330_val;
    _golf_shader_import_get_shader(path, "glsl", "330", &glsl330_val);
    json_object_set_value(obj, "glsl330", glsl330_val);

    JSON_Value *gles300_val;
    _golf_shader_import_get_shader(path, "gles", "300", &gles300_val);
    json_object_set_value(obj, "gles300", gles300_val);

    {
        golf_string_t import_shader_file_path;
        golf_string_initf(&import_shader_file_path, "data", "%s.golf_data", file.path);
        json_serialize_to_file_pretty(val, import_shader_file_path.cstr);
        golf_string_deinit(&import_shader_file_path);
    }

    json_value_free(val);
    return true;
#endif
}

static bool _golf_shader_finalize(void *ptr) {
    golf_shader_t *shader = (golf_shader_t*) ptr;

    sg_shader_desc desc = { 0 };
    for (int i = 0; i < shader->vs_inputs.length; i++) {
        golf_shader_input_t *input = &shader->vs_inputs.data[i];
        desc.attrs[input->location].name = input->name;
    }

    desc.vs.source = shader->vs_source;
    desc.vs.entry = "main";
    for (int i = 0; i < shader->vs_uniforms.length; i++) {
        golf_shader_uniform_t *uniform = &shader->vs_uniforms.data[i];
        desc.vs.uniform_blocks[uniform->binding].size = uniform->size;
        uniform->data = golf_alloc(uniform->size);
        memset(uniform->data, 0, uniform->size);

        desc.vs.uniform_blocks[uniform->binding].uniforms[0].name = uniform->name;
        desc.vs.uniform_blocks[uniform->binding].uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
        desc.vs.uniform_blocks[uniform->binding].uniforms[0].array_count = uniform->size / 16;
    }

    desc.fs.source = shader->fs_source;
    desc.fs.entry = "main";
    for (int i = 0; i < shader->fs_uniforms.length; i++) {
        golf_shader_uniform_t *uniform = &shader->fs_uniforms.data[i];
        desc.fs.uniform_blocks[uniform->binding].size = uniform->size;
        uniform->data = golf_alloc(uniform->size);
        memset(uniform->data, 0, uniform->size);

        desc.fs.uniform_blocks[uniform->binding].uniforms[0].name = uniform->name;
        desc.fs.uniform_blocks[uniform->binding].uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
        desc.fs.uniform_blocks[uniform->binding].uniforms[0].array_count = uniform->size / 16;
    }
    for (int i = 0; i < shader->fs_textures.length; i++) {
        golf_shader_texture_t *texture = &shader->fs_textures.data[i];
        desc.fs.images[texture->binding].name = texture->name;
        desc.fs.images[texture->binding].image_type = SG_IMAGETYPE_2D;
        desc.fs.images[texture->binding].sampler_type = SG_SAMPLERTYPE_FLOAT;
    }

    shader->sg_shader = sg_make_shader(&desc);

    vec_init(&shader->pipelines, "data.shader");
    if (strcmp(shader->file.path, "data/shaders/ui.glsl") == 0) {
        {
            sg_pipeline_desc desc = {
                .shader = shader->sg_shader,
                .layout = {
                    .attrs = {
                        [0] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                        [1] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                    },
                },
                .depth = {
                    .compare = SG_COMPAREFUNC_ALWAYS
                },
                .colors[0] = {
                    .blend = {
                        .enabled = true,
                        .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                        .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    },
                },
            };

            golf_shader_pipeline_t pipeline;
            snprintf(pipeline.name, GOLF_MAX_NAME_LEN, "%s", "ui");
            pipeline.sg_pipeline = sg_make_pipeline(&desc);
            vec_push(&shader->pipelines, pipeline);
        }
    }
    else if (strcmp(shader->file.path, "data/shaders/render_image.glsl") == 0) {
        {
            sg_pipeline_desc desc = {
                .shader = shader->sg_shader,
                .layout = {
                    .attrs = {
                        [0] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                        [1] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                    },
                },
            };

            golf_shader_pipeline_t pipeline;
            snprintf(pipeline.name, GOLF_MAX_NAME_LEN, "%s", "render_image");
            pipeline.sg_pipeline = sg_make_pipeline(&desc);
            vec_push(&shader->pipelines, pipeline);
        }
    }
    else if (strcmp(shader->file.path, "data/shaders/fxaa.glsl") == 0) {
        {
            sg_pipeline_desc desc = {
                .shader = shader->sg_shader,
                .layout = {
                    .attrs = {
                        [0] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                        [1] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                    },
                },
            };

            golf_shader_pipeline_t pipeline;
            snprintf(pipeline.name, GOLF_MAX_NAME_LEN, "%s", "fxaa");
            pipeline.sg_pipeline = sg_make_pipeline(&desc);
            vec_push(&shader->pipelines, pipeline);
        }
    }
    else if (strcmp(shader->file.path, "data/shaders/diffuse_color_material.glsl") == 0) {
        {
            sg_pipeline_desc desc = {
                .shader = shader->sg_shader,
                .layout = {
                    .attrs = {
                        [0] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                        [1] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 1 },
                    },
                },
                .depth = {
                    .compare = SG_COMPAREFUNC_LESS_EQUAL,
                    .write_enabled = true,
                },
            };

            golf_shader_pipeline_t pipeline;
            snprintf(pipeline.name, GOLF_MAX_NAME_LEN, "%s", "diffuse_color_material");
            pipeline.sg_pipeline = sg_make_pipeline(&desc);
            vec_push(&shader->pipelines, pipeline);
        }
    }
    else if (strcmp(shader->file.path, "data/shaders/environment_material.glsl") == 0) {
        {
            sg_pipeline_desc desc = {
                .shader = shader->sg_shader,
                .layout = {
                    .attrs = {
                        [0] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                        [1] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                        [2] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 2 },
                        [3] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 3 },
                    },
                },
                .depth = {
                    .compare = SG_COMPAREFUNC_LESS_EQUAL,
                    .write_enabled = true,
                },
            };

            golf_shader_pipeline_t pipeline;
            snprintf(pipeline.name, GOLF_MAX_NAME_LEN, "%s", "environment_material");
            pipeline.sg_pipeline = sg_make_pipeline(&desc);
            vec_push(&shader->pipelines, pipeline);
        }
    }
    else if (strcmp(shader->file.path, "data/shaders/solid_color_material.glsl") == 0) {
        {
            sg_pipeline_desc desc = {
                .shader = shader->sg_shader,
                .layout = {
                    .attrs = {
                        [0] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
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

            golf_shader_pipeline_t pipeline;
            snprintf(pipeline.name, GOLF_MAX_NAME_LEN, "%s", "solid_color_material");
            pipeline.sg_pipeline = sg_make_pipeline(&desc);
            vec_push(&shader->pipelines, pipeline);
        }
    }
    else if (strcmp(shader->file.path, "data/shaders/texture_material.glsl") == 0) {
        {
            sg_pipeline_desc desc = {
                .shader = shader->sg_shader,
                .layout = {
                    .attrs = {
                        [0] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                        [1] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                        [2] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 2 },
                    },
                },
                .depth = {
                    .compare = SG_COMPAREFUNC_LESS_EQUAL,
                    .write_enabled = true,
                },
            };

            golf_shader_pipeline_t pipeline;
            snprintf(pipeline.name, GOLF_MAX_NAME_LEN, "%s", "texture_material");
            pipeline.sg_pipeline = sg_make_pipeline(&desc);
            vec_push(&shader->pipelines, pipeline);
        }

        {
            sg_pipeline_desc desc = {
                .shader = shader->sg_shader,
                .layout = {
                    .attrs = {
                        [0] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                        [1] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                        [2] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 2 },
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

            golf_shader_pipeline_t pipeline;
            snprintf(pipeline.name, GOLF_MAX_NAME_LEN, "%s", "hole_pass_2");
            pipeline.sg_pipeline = sg_make_pipeline(&desc);
            vec_push(&shader->pipelines, pipeline);
        }
    }
    else if (strcmp(shader->file.path, "data/shaders/pass_through.glsl") == 0) {
        {
            sg_pipeline_desc desc = {
                .shader = shader->sg_shader,
                .layout = {
                    .attrs = {
                        [0] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
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

            golf_shader_pipeline_t pipeline;
            snprintf(pipeline.name, GOLF_MAX_NAME_LEN, "%s", "hole_pass_1");
            pipeline.sg_pipeline = sg_make_pipeline(&desc);
            vec_push(&shader->pipelines, pipeline);
        }
    }
    else if (strcmp(shader->file.path, "data/shaders/aim_line.glsl") == 0) {
        {
            sg_pipeline_desc desc = {
                .shader = shader->sg_shader,
                .layout = {
                    .attrs = {
                        [0] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                        [1] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                    },
                },
                .depth = {
                    .compare = SG_COMPAREFUNC_LESS_EQUAL,
                    .write_enabled = false,
                },
                .colors[0] = {
                    .blend = {
                        .enabled = true,
                        .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                        .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    }
                }
            };

            golf_shader_pipeline_t pipeline;
            snprintf(pipeline.name, GOLF_MAX_NAME_LEN, "%s", "aim_line");
            pipeline.sg_pipeline = sg_make_pipeline(&desc);
            vec_push(&shader->pipelines, pipeline);
        }
    }
    else if (strcmp(shader->file.path, "data/shaders/ball.glsl") == 0) {
        {
            sg_pipeline_desc desc = {
                .shader = shader->sg_shader,
                .layout = {
                    .attrs = {
                        [0] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                        [1] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 1 },
                        [2] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 2 },
                    },
                },
                .depth = {
                    .compare = SG_COMPAREFUNC_LESS_EQUAL,
                    .write_enabled = true,
                },
            };

            golf_shader_pipeline_t pipeline;
            snprintf(pipeline.name, GOLF_MAX_NAME_LEN, "%s", "ball");
            pipeline.sg_pipeline = sg_make_pipeline(&desc);
            vec_push(&shader->pipelines, pipeline);
        }
    }
    else if (strcmp(shader->file.path, "data/shaders/editor_water.glsl") == 0) {
        {
            sg_pipeline_desc desc = {
                .shader = shader->sg_shader,
                .layout = {
                    .attrs = {
                        [0] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                        [1] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                        [2] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 2 },
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

            golf_shader_pipeline_t pipeline;
            snprintf(pipeline.name, GOLF_MAX_NAME_LEN, "%s", "editor_water");
            pipeline.sg_pipeline = sg_make_pipeline(&desc);
            vec_push(&shader->pipelines, pipeline);
        }
    }
    else if (strcmp(shader->file.path, "data/shaders/water.glsl") == 0) {
        {
            sg_pipeline_desc desc = {
                .shader = shader->sg_shader,
                .layout = {
                    .attrs = {
                        [0] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                        [1] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                        [2] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 2 },
                    },
                },
                .depth = {
                    .compare = SG_COMPAREFUNC_LESS_EQUAL,
                    .write_enabled = true,
                },
                .stencil = {
                    .front = {
                        .fail_op = SG_STENCILOP_KEEP,
                        .depth_fail_op = SG_STENCILOP_REPLACE,
                        .pass_op = SG_STENCILOP_REPLACE,
                        .compare = SG_COMPAREFUNC_ALWAYS,
                    },
                    .back = {
                        .fail_op = SG_STENCILOP_KEEP,
                        .depth_fail_op = SG_STENCILOP_REPLACE,
                        .pass_op = SG_STENCILOP_REPLACE,
                        .compare = SG_COMPAREFUNC_ALWAYS,
                    },
                    .enabled = true,
                    .write_mask = 255,
                    .read_mask = 255,
                    .ref = 255,
                },
                .colors[0] = {
                    .blend = {
                        .enabled = true,
                        .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                        .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    },
                }
            };

            golf_shader_pipeline_t pipeline;
            snprintf(pipeline.name, GOLF_MAX_NAME_LEN, "%s", "water");
            pipeline.sg_pipeline = sg_make_pipeline(&desc);
            vec_push(&shader->pipelines, pipeline);
        }
    }
    else if (strcmp(shader->file.path, "data/shaders/water_around_ball.glsl") == 0) {
        {
            sg_pipeline_desc desc = {
                .shader = shader->sg_shader,
                .layout = {
                    .attrs = {
                        [0] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                        [1] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                    },
                },
                .depth = {
                    .compare = SG_COMPAREFUNC_LESS_EQUAL,
                    .write_enabled = false,
                },
                .stencil = {
                    .front = {
                        .fail_op = SG_STENCILOP_KEEP,
                        .depth_fail_op = SG_STENCILOP_KEEP,
                        .pass_op = SG_STENCILOP_KEEP,
                        .compare = SG_COMPAREFUNC_EQUAL,
                    },
                    .back = {
                        .fail_op = SG_STENCILOP_KEEP,
                        .depth_fail_op = SG_STENCILOP_KEEP,
                        .pass_op = SG_STENCILOP_KEEP,
                        .compare = SG_COMPAREFUNC_EQUAL,
                    },
                    .enabled = true,
                    .write_mask = 255,
                    .read_mask = 255,
                    .ref = 255,
                },
                .colors[0] = {
                    .blend = {
                        .enabled = true,
                        .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                        .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    },
                }
            };

            golf_shader_pipeline_t pipeline;
            snprintf(pipeline.name, GOLF_MAX_NAME_LEN, "%s", "water_around_ball");
            pipeline.sg_pipeline = sg_make_pipeline(&desc);
            vec_push(&shader->pipelines, pipeline);
        }
    }
    else if (strcmp(shader->file.path, "data/shaders/water_ripple.glsl") == 0) {
        {
            sg_pipeline_desc desc = {
                .shader = shader->sg_shader,
                .layout = {
                    .attrs = {
                        [0] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                        [1] = { .format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1 },
                    },
                },
                .depth = {
                    .compare = SG_COMPAREFUNC_LESS_EQUAL,
                    .write_enabled = false,
                },
                .stencil = {
                    .front = {
                        .fail_op = SG_STENCILOP_KEEP,
                        .depth_fail_op = SG_STENCILOP_KEEP,
                        .pass_op = SG_STENCILOP_KEEP,
                        .compare = SG_COMPAREFUNC_EQUAL,
                    },
                    .back = {
                        .fail_op = SG_STENCILOP_KEEP,
                        .depth_fail_op = SG_STENCILOP_KEEP,
                        .pass_op = SG_STENCILOP_KEEP,
                        .compare = SG_COMPAREFUNC_EQUAL,
                    },
                    .enabled = true,
                    .write_mask = 255,
                    .read_mask = 255,
                    .ref = 255,
                },
                .colors[0] = {
                    .blend = {
                        .enabled = true,
                        .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                        .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    },
                }
            };

            golf_shader_pipeline_t pipeline;
            snprintf(pipeline.name, GOLF_MAX_NAME_LEN, "%s", "water_ripple");
            pipeline.sg_pipeline = sg_make_pipeline(&desc);
            vec_push(&shader->pipelines, pipeline);
        }
    }
    else if (strcmp(shader->file.path, "data/shaders/ball_hidden.glsl") == 0) {
        {
            sg_pipeline_desc desc = {
                .shader = shader->sg_shader,
                .layout = {
                    .attrs = {
                        [0] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0 },
                        [1] = { .format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 1 },
                    },
                },
                .depth = {
                    .compare = SG_COMPAREFUNC_GREATER,
                },
                .cull_mode = SG_CULLMODE_FRONT,
                .colors[0] = {
                    .blend = {
                        .enabled = true,
                        .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                        .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    },
                }
            };

            golf_shader_pipeline_t pipeline;
            snprintf(pipeline.name, GOLF_MAX_NAME_LEN, "%s", "ball_hidden");
            pipeline.sg_pipeline = sg_make_pipeline(&desc);
            vec_push(&shader->pipelines, pipeline);
        }
    }
    else {
        golf_log_warning("No pipelines created for shader: %s", shader->file.path);
    }

    return true;
}

golf_shader_uniform_value_t golf_shader_uniform_value_float(const char *name, float f) {
    golf_shader_uniform_value_t value;
    value.type = GOLF_SHADER_UNIFORM_VALUE_FLOAT;
    value.name = name;
    value.f = f;
    return value;
}

golf_shader_uniform_value_t golf_shader_uniform_value_vec2(const char *name, vec2 v2) {
    golf_shader_uniform_value_t value;
    value.type = GOLF_SHADER_UNIFORM_VALUE_VEC2;
    value.name = name;
    value.v2 = v2;
    return value;
}

golf_shader_uniform_value_t golf_shader_uniform_value_vec4(const char *name, vec4 v4) {
    golf_shader_uniform_value_t value;
    value.type = GOLF_SHADER_UNIFORM_VALUE_VEC4;
    value.name = name;
    value.v4 = v4;
    return value;
}

golf_shader_uniform_value_t golf_shader_uniform_value_mat4(const char *name, mat4 m4) {
    golf_shader_uniform_value_t value;
    value.type = GOLF_SHADER_UNIFORM_VALUE_MAT4;
    value.name = name;
    value.m4 = m4;
    return value;
}

golf_shader_uniform_t *golf_shader_vs_uniform_setup(golf_shader_t *shader, const char *name, int n, ...) {
    golf_shader_uniform_t *uniform = golf_shader_get_vs_uniform(shader, name);
    if (!uniform) {
        return NULL;
    }

    va_list args;
    va_start(args, n);
    for (int i = 0; i < n; i++) {
        golf_shader_uniform_value_t value = va_arg(args, golf_shader_uniform_value_t);
        switch (value.type) {
            case GOLF_SHADER_UNIFORM_VALUE_FLOAT:
                golf_shader_uniform_set_float(uniform, value.name, value.f);
                break;
            case GOLF_SHADER_UNIFORM_VALUE_VEC2:
                golf_shader_uniform_set_vec2(uniform, value.name, value.v2);
                break;
            case GOLF_SHADER_UNIFORM_VALUE_VEC4:
                golf_shader_uniform_set_vec4(uniform, value.name, value.v4);
                break;
            case GOLF_SHADER_UNIFORM_VALUE_MAT4:
                golf_shader_uniform_set_mat4(uniform, value.name, value.m4);
                break;
        }
    }
    va_end(args);

    return uniform;
}

golf_shader_uniform_t *golf_shader_fs_uniform_setup(golf_shader_t *shader, const char *name, int n, ...) {
    golf_shader_uniform_t *uniform = golf_shader_get_fs_uniform(shader, name);
    if (!uniform) {
        return NULL;
    }

    va_list args;
    va_start(args, n);
    for (int i = 0; i < n; i++) {
        golf_shader_uniform_value_t value = va_arg(args, golf_shader_uniform_value_t);
        switch (value.type) {
            case GOLF_SHADER_UNIFORM_VALUE_FLOAT:
                golf_shader_uniform_set_float(uniform, value.name, value.f);
                break;
            case GOLF_SHADER_UNIFORM_VALUE_VEC2:
                golf_shader_uniform_set_vec2(uniform, value.name, value.v2);
                break;
            case GOLF_SHADER_UNIFORM_VALUE_VEC4:
                golf_shader_uniform_set_vec4(uniform, value.name, value.v4);
                break;
            case GOLF_SHADER_UNIFORM_VALUE_MAT4:
                golf_shader_uniform_set_mat4(uniform, value.name, value.m4);
                break;
        }
    }
    va_end(args);

    return uniform;
}

void golf_shader_uniform_set_float(golf_shader_uniform_t *uniform, const char *name, float f) {
    golf_shader_uniform_member_t *member = NULL;
    for (int i = 0; i < uniform->members.length; i++) {
        if ((strcmp(uniform->members.data[i].name, name) == 0) && uniform->members.data[i].size == 4) {
            member = &uniform->members.data[i];
        }
    }
    if (!member) {
        golf_log_warning("Unable to find float member: %s", name);
        return;
    }
    memcpy(uniform->data + member->offset, &f, 4);
}

void golf_shader_uniform_set_vec2(golf_shader_uniform_t *uniform, const char *name, vec2 v) {
    golf_shader_uniform_member_t *member = NULL;
    for (int i = 0; i < uniform->members.length; i++) {
        if ((strcmp(uniform->members.data[i].name, name) == 0) && uniform->members.data[i].size == 8) {
            member = &uniform->members.data[i];
        }
    }
    if (!member) {
        golf_log_warning("Unable to find vec2 member: %s", name);
        return;
    }
    memcpy(uniform->data + member->offset, &v, 8);
}

void golf_shader_uniform_set_vec4(golf_shader_uniform_t *uniform, const char *name, vec4 v) {
    golf_shader_uniform_member_t *member = NULL;
    for (int i = 0; i < uniform->members.length; i++) {
        if ((strcmp(uniform->members.data[i].name, name) == 0) && uniform->members.data[i].size == 16) {
            member = &uniform->members.data[i];
        }
    }
    if (!member) {
        golf_log_warning("Unable to find vec4 member: %s", name);
        return;
    }
    memcpy(uniform->data + member->offset, &v, 16);
}

void golf_shader_uniform_set_mat4(golf_shader_uniform_t *uniform, const char *name, mat4 m) {
    golf_shader_uniform_member_t *member = NULL;
    for (int i = 0; i < uniform->members.length; i++) {
        if ((strcmp(uniform->members.data[i].name, name) == 0) && uniform->members.data[i].size == 64) {
            member = &uniform->members.data[i];
        }
    }
    if (!member) {
        golf_log_warning("Unable to find mat4 member: %s", name);
        return;
    }
    memcpy(uniform->data + member->offset, &m, 64);
}

golf_shader_uniform_t *golf_shader_get_vs_uniform(golf_shader_t *shader, const char *name) {
    for (int i = 0; i < shader->vs_uniforms.length; i++) {
        if (strcmp(shader->vs_uniforms.data[i].name, name) == 0) {
            return &shader->vs_uniforms.data[i];
        }
    }
    golf_log_warning("Could not find vs uniform: %s", name);
    return NULL;
}

golf_shader_uniform_t *golf_shader_get_fs_uniform(golf_shader_t *shader, const char *name) {
    for (int i = 0; i < shader->fs_uniforms.length; i++) {
        if (strcmp(shader->fs_uniforms.data[i].name, name) == 0) {
            return &shader->fs_uniforms.data[i];
        }
    }
    golf_log_warning("Could not find fs uniform: %s", name);
    return NULL;
}

golf_shader_pipeline_t *golf_shader_get_pipeline(golf_shader_t *shader, const char *name) {
    for (int i = 0; i < shader->pipelines.length; i++) {
        if (strcmp(shader->pipelines.data[i].name, name) == 0) {
            return &shader->pipelines.data[i];
        }
    }
    golf_log_warning("Could not find pipeline: %s", name);
    return NULL;
}

static bool _golf_shader_load_inputs(JSON_Array *inputs_arr, vec_golf_shader_input_t *inputs) {
    vec_init(inputs, "data");
    for (int i = 0; i < (int)json_array_get_count(inputs_arr); i++) {
        JSON_Object *input_obj = json_array_get_object(inputs_arr, i);
        const char *name = json_object_get_string(input_obj, "name");
        int location = (int)json_object_get_number(input_obj, "location");

        golf_shader_input_t input;
        snprintf(input.name, GOLF_MAX_NAME_LEN, "%s", name);
        input.location = location;

        vec_push(inputs, input);
    }
    return true;
}

static bool _golf_shader_load_uniforms(JSON_Array *uniforms_arr, vec_golf_shader_uniform_t *uniforms) {
    vec_init(uniforms, "data");
    for (int i = 0; i < (int)json_array_get_count(uniforms_arr); i++) {
        JSON_Object *uniform_obj = json_array_get_object(uniforms_arr, i);
        const char *name = json_object_get_string(uniform_obj, "name");
        int size = (int)json_object_get_number(uniform_obj, "size");
        int binding = (int)json_object_get_number(uniform_obj, "binding");

        golf_shader_uniform_t uniform;
        snprintf(uniform.name, GOLF_MAX_NAME_LEN, "%s", name);
        uniform.size = size;
        uniform.binding = binding;
        vec_init(&uniform.members, "data");

        JSON_Array *members_arr = json_object_get_array(uniform_obj, "members");
        for (int i = 0; i < (int)json_array_get_count(members_arr); i++) {
            JSON_Object *member_obj = json_array_get_object(members_arr, i);
            const char *name = json_object_get_string(member_obj, "name");
            int offset = (int)json_object_get_number(member_obj, "offset");
            int size = (int)json_object_get_number(member_obj, "size");

            golf_shader_uniform_member_t member;
            snprintf(member.name, GOLF_MAX_NAME_LEN, "%s", name);
            member.offset = offset;
            member.size = size;

            vec_push(&uniform.members, member);
        }

        vec_push(uniforms, uniform);
    }
    return true;
}

static bool _golf_shader_load_textures(JSON_Array *textures_arr, vec_golf_shader_texture_t *textures) {
    vec_init(textures, "data");
    for (int i = 0; i < (int)json_array_get_count(textures_arr); i++) {
        JSON_Object *texture_obj = json_array_get_object(textures_arr, i);
        const char *name = json_object_get_string(texture_obj, "name");
        int binding = (int)json_object_get_number(texture_obj, "binding");

        golf_shader_texture_t texture;
        snprintf(texture.name, GOLF_MAX_NAME_LEN, "%s", name);
        texture.binding = binding;

        vec_push(textures, texture);
    }
    return true;
}

static bool _golf_shader_load(void *ptr, const char *path, char *data, int data_len, char *meta_data, int meta_data_len) {
    GOLF_UNUSED(path);
    GOLF_UNUSED(data_len);
    GOLF_UNUSED(meta_data);
    GOLF_UNUSED(meta_data_len);

    golf_shader_t *shader = (golf_shader_t*) ptr;
    shader->file = golf_file(path);

    JSON_Value *val = json_parse_string(data);
    JSON_Object *obj = json_value_get_object(val);

    JSON_Object *fs_obj, *vs_obj;
#if SOKOL_GLCORE33
    fs_obj = json_object_dotget_object(obj, "glsl330.fs");
    vs_obj = json_object_dotget_object(obj, "glsl330.vs");
#elif SOKOL_GLES3
    fs_obj = json_object_dotget_object(obj, "gles300.fs");
    vs_obj = json_object_dotget_object(obj, "gles300.vs");
#else
#error Unknown sokol backend
#endif

    {
        const char *fs_source = json_object_get_string(fs_obj, "source");
        const char *vs_source = json_object_get_string(vs_obj, "source");

        int fs_source_len = (int)strlen(fs_source);
        shader->fs_source = golf_alloc(fs_source_len + 1);
        strcpy(shader->fs_source, fs_source);

        int vs_source_len = (int)strlen(vs_source);
        shader->vs_source = golf_alloc(vs_source_len + 1);
        strcpy(shader->vs_source, vs_source);
    }

    _golf_shader_load_inputs(json_object_get_array(fs_obj, "inputs"), &shader->fs_inputs);
    _golf_shader_load_uniforms(json_object_get_array(fs_obj, "uniforms"), &shader->fs_uniforms);
    _golf_shader_load_textures(json_object_get_array(fs_obj, "textures"), &shader->fs_textures);

    _golf_shader_load_inputs(json_object_get_array(vs_obj, "inputs"), &shader->vs_inputs);
    _golf_shader_load_uniforms(json_object_get_array(vs_obj, "uniforms"), &shader->vs_uniforms);

    json_value_free(val);
    return true;
}

static bool _golf_shader_unload(void *ptr) {
    golf_shader_t *shader = (golf_shader_t*) ptr;
    sg_destroy_shader(shader->sg_shader);
    return true;
}

//
// FONT
//

static void _stbi_write_func(void *context, void *data, int size) {
    vec_char_t *bmp = (vec_char_t*)context;
    vec_pusharr(bmp, (char*)data, size);
}

static JSON_Value *_golf_font_atlas_import(const char *file_data, int font_size, int img_size) {
    unsigned char *bitmap = golf_alloc(img_size * img_size);
    stbtt_bakedchar cdata[96];
    memset(cdata, 0, sizeof(cdata));
    stbtt_BakeFontBitmap((const unsigned char*)file_data, 0, (float)-font_size, bitmap, img_size, img_size, 32, 95, cdata);

    float ascent, descent, linegap;
    stbtt_GetScaledFontVMetrics((const unsigned char*)file_data, 0, (float)-font_size, &ascent, &descent, &linegap);

    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);
    json_object_set_number(obj, "font_size", font_size);
    json_object_set_number(obj, "ascent", ascent);
    json_object_set_number(obj, "descent", descent);
    json_object_set_number(obj, "linegap", linegap);

    JSON_Value *char_datas_val = json_value_init_array();
    JSON_Array *char_datas_array = json_value_get_array(char_datas_val);
    for (int i = 0; i < 96; i++) {
        JSON_Value *char_data_val = json_value_init_object();
        JSON_Object *char_data_obj = json_value_get_object(char_data_val);
        json_object_set_number(char_data_obj, "c", 32 + i);
        json_object_set_number(char_data_obj, "x0", cdata[i].x0);
        json_object_set_number(char_data_obj, "x1", cdata[i].x1);
        json_object_set_number(char_data_obj, "y0", cdata[i].y0);
        json_object_set_number(char_data_obj, "y1", cdata[i].y1);
        json_object_set_number(char_data_obj, "xoff", cdata[i].xoff);
        json_object_set_number(char_data_obj, "yoff", cdata[i].yoff);
        json_object_set_number(char_data_obj, "xadvance", cdata[i].xadvance);
        json_array_append_value(char_datas_array, char_data_val);
    }
    json_object_set_value(obj, "char_datas", char_datas_val);

    json_object_set_number(obj, "img_size", img_size);
    {
        vec_char_t img;
        vec_init(&img, "data");
        stbi_write_png_to_func(_stbi_write_func, &img, img_size, img_size, 1, bitmap, img_size);
        golf_json_object_set_data(obj, "img_data", (unsigned char*)img.data, img.length);
        vec_deinit(&img);
    }

    golf_free(bitmap);
    return val;
}

static bool _golf_font_import(const char *path, char *data, int data_len) {
    GOLF_UNUSED(path);
    GOLF_UNUSED(data_len);

    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    JSON_Value *atlases_val = json_value_init_array();
    JSON_Array *atlases_array = json_value_get_array(atlases_val);

    json_array_append_value(atlases_array, _golf_font_atlas_import(data, 16, 256));
    json_array_append_value(atlases_array, _golf_font_atlas_import(data, 24, 256));
    json_array_append_value(atlases_array, _golf_font_atlas_import(data, 32, 256));
    json_array_append_value(atlases_array, _golf_font_atlas_import(data, 40, 512));
    json_array_append_value(atlases_array, _golf_font_atlas_import(data, 48, 512));
    json_array_append_value(atlases_array, _golf_font_atlas_import(data, 56, 512));
    json_array_append_value(atlases_array, _golf_font_atlas_import(data, 64, 512));

    json_object_set_value(obj, "atlases", atlases_val);

    golf_string_t import_font_file_path;
    golf_string_initf(&import_font_file_path, "data", "%s.golf_data", path);
    json_serialize_to_file(val, import_font_file_path.cstr);
    golf_string_deinit(&import_font_file_path);

    json_value_free(val);
    return true;
}

static bool _golf_font_finalize(void *ptr) {
    golf_font_t *font = (golf_font_t*) ptr;
    for (int i = 0; i < font->atlases.length; i++) {
        golf_font_atlas_t *atlas = &font->atlases.data[i];  

        sg_image_desc img_desc = {
            .width = atlas->size,
            .height = atlas->size,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
            .data.subimage[0][0] = {
                .ptr = atlas->image_data,
                .size = 4 * atlas->size * atlas->size,
            }
        };
        atlas->sg_image = sg_make_image(&img_desc);
        free(atlas->image_data);
    }
    return true;
}

static void _golf_font_load_atlas(JSON_Object *atlas_obj, golf_font_atlas_t *atlas) {
    atlas->font_size = (float)json_object_get_number(atlas_obj, "font_size");
    atlas->ascent = (float)json_object_get_number(atlas_obj, "ascent");
    atlas->descent = (float)json_object_get_number(atlas_obj, "descent");
    atlas->linegap = (float)json_object_get_number(atlas_obj, "linegap");
    atlas->size = (int)json_object_get_number(atlas_obj, "img_size");

    JSON_Array *char_datas_array = json_object_get_array(atlas_obj, "char_datas");
    for (int i = 0; i < (int)json_array_get_count(char_datas_array); i++) {
        JSON_Object *char_data_obj = json_array_get_object(char_datas_array, i);
        int c = (int)json_object_get_number(char_data_obj, "c");
        if (c >= 0 && c < 256) {
            atlas->char_data[c].x0 = (float)json_object_get_number(char_data_obj, "x0");
            atlas->char_data[c].x1 = (float)json_object_get_number(char_data_obj, "x1");
            atlas->char_data[c].y0 = (float)json_object_get_number(char_data_obj, "y0");
            atlas->char_data[c].y1 = (float)json_object_get_number(char_data_obj, "y1");
            atlas->char_data[c].xoff = (float)json_object_get_number(char_data_obj, "xoff");
            atlas->char_data[c].yoff = (float)json_object_get_number(char_data_obj, "yoff");
            atlas->char_data[c].xadvance = (float)json_object_get_number(char_data_obj, "xadvance");
        }
    }

    unsigned char *img_data = NULL;
    int img_data_len;
    golf_json_object_get_data(atlas_obj, "img_data", &img_data, &img_data_len);

    int x, y, n;
    stbi_set_flip_vertically_on_load(0);
    atlas->image_data = stbi_load_from_memory(img_data, img_data_len, &x, &y, &n, 4);
    if (!atlas->image_data) {
        golf_log_error("STB Failed to load image");
    }

    golf_free(img_data);
}

static bool _golf_font_load(void *ptr, const char *path, char *data, int data_len, char *meta_data, int meta_data_len) {
    GOLF_UNUSED(path);
    GOLF_UNUSED(data_len);
    GOLF_UNUSED(meta_data);
    GOLF_UNUSED(meta_data_len);

    golf_font_t *font = (golf_font_t*) ptr;
    JSON_Value *val = json_parse_string(data);
    JSON_Object *obj = json_value_get_object(val);
    if (!val) {
        golf_log_warning("Unable to parse json for font file %s", path);
        return false;
    }

    JSON_Array *atlases_array = json_object_get_array(obj, "atlases");
    vec_init(&font->atlases, "data");
    for (int i = 0; i < (int)json_array_get_count(atlases_array); i++) {
        JSON_Object *atlas_obj = json_array_get_object(atlases_array, i);
        golf_font_atlas_t atlas;
        _golf_font_load_atlas(atlas_obj, &atlas);
        vec_push(&font->atlases, atlas);
    }

    json_value_free(val);

    return true;
}

static bool _golf_font_unload(void *ptr) {
    golf_font_t *font = (golf_font_t*) ptr;
    for (int i = 0; i < font->atlases.length; i++) {
        golf_font_atlas_t atlas = font->atlases.data[i];
        sg_destroy_image(atlas.sg_image);
    }
    vec_deinit(&font->atlases);
    return true;
}

//
// MODEL
//

typedef struct _model_material_data {
    const char *name;
    vec_float_t vertices;
} _model_material_data_t;
typedef vec_t(_model_material_data_t) _vec_model_material_data;

golf_model_group_t golf_model_group(const char *material_name, int start_vertex, int vertex_count) {
    golf_model_group_t group;
    snprintf(group.material_name, GOLF_MAX_NAME_LEN, "%s", material_name);
    group.start_vertex = start_vertex;
    group.vertex_count = vertex_count;
    return group;
}

golf_model_t golf_model_dynamic(vec_golf_group_t groups, vec_vec3_t positions, vec_vec3_t normals, vec_vec2_t texcoords) {
    golf_model_t model;
    model.groups = groups;
    model.positions = positions;
    model.normals = normals;
    model.texcoords = texcoords;
    model.is_water = false;
    model.sg_size = 0;
    return model;
}

golf_model_t golf_model_dynamic_water(vec_golf_group_t groups, vec_vec3_t positions, vec_vec3_t normals, vec_vec2_t texcoords, vec_vec3_t water_dir) {
    golf_model_t model;
    model.groups = groups;
    model.positions = positions;
    model.normals = normals;
    model.texcoords = texcoords;
    model.is_water = true;
    model.water_dir = water_dir;
    model.sg_size = 0;
    return model;
}

void golf_model_dynamic_finalize(golf_model_t *model) {
    model->sg_size = model->positions.length;

    if (model->sg_size > 0) {
        sg_buffer_desc desc = {
            .type = SG_BUFFERTYPE_VERTEXBUFFER,
            .usage = SG_USAGE_DYNAMIC,
        };

        desc.size = sizeof(vec3) * model->sg_size;
        model->sg_positions_buf = sg_make_buffer(&desc);

        desc.size = sizeof(vec3) * model->sg_size;
        model->sg_normals_buf = sg_make_buffer(&desc);

        desc.size = sizeof(vec2) * model->sg_size;
        model->sg_texcoords_buf = sg_make_buffer(&desc);

        golf_model_dynamic_update_sg_buf(model);
    }
}

void golf_model_dynamic_update_sg_buf(golf_model_t *model) {
    if (model->positions.length > model->sg_size) {
        if (model->sg_size > 0) {
            sg_destroy_buffer(model->sg_positions_buf);
            sg_destroy_buffer(model->sg_normals_buf);
            sg_destroy_buffer(model->sg_texcoords_buf);
        }

        model->sg_size = 2 * model->positions.length;

        sg_buffer_desc desc = {
            .type = SG_BUFFERTYPE_VERTEXBUFFER,
            .usage = SG_USAGE_DYNAMIC,
        };

        desc.size = sizeof(vec3) * model->sg_size;
        model->sg_positions_buf = sg_make_buffer(&desc);

        desc.size = sizeof(vec3) * model->sg_size;
        model->sg_normals_buf = sg_make_buffer(&desc);

        desc.size = sizeof(vec2) * model->sg_size;
        model->sg_texcoords_buf = sg_make_buffer(&desc);
    }

    if (model->positions.length > 0) {
        sg_update_buffer(model->sg_positions_buf, 
                &(sg_range) { model->positions.data, sizeof(vec3) * model->positions.length });
        sg_update_buffer(model->sg_normals_buf, 
                &(sg_range) { model->normals.data, sizeof(vec3) * model->normals.length });
        sg_update_buffer(model->sg_texcoords_buf, 
                &(sg_range) { model->texcoords.data, sizeof(vec2) * model->texcoords.length });
    }
}

/*
void golf_model_init(golf_model_t *model, int size) {
    vec_init(&model->groups, "data");
    vec_init(&model->positions, "data");
    vec_init(&model->normals, "data");
    vec_init(&model->texcoords, "data");
    model->sg_size = size;

    sg_buffer_desc desc = {
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .usage = SG_USAGE_DYNAMIC,
    };

    desc.size = sizeof(vec3) * size;
    model->sg_positions_buf = sg_make_buffer(&desc);

    desc.size = sizeof(vec3) * size;
    model->sg_normals_buf = sg_make_buffer(&desc);

    desc.size = sizeof(vec2) * size;
    model->sg_texcoords_buf = sg_make_buffer(&desc);
}

void golf_model_update_buf(golf_model_t *model) {
    if (model->positions.length > model->sg_size) {
        model->sg_size = 2 * model->positions.length;

        sg_buffer_desc desc = {
            .type = SG_BUFFERTYPE_VERTEXBUFFER,
            .usage = SG_USAGE_DYNAMIC,
        };

        desc.size = sizeof(vec3) * model->sg_size;
        sg_destroy_buffer(model->sg_positions_buf);
        model->sg_positions_buf = sg_make_buffer(&desc);

        desc.size = sizeof(vec3) * model->sg_size;
        sg_destroy_buffer(model->sg_normals_buf);
        model->sg_normals_buf = sg_make_buffer(&desc);

        desc.size = sizeof(vec2) * model->sg_size;
        sg_destroy_buffer(model->sg_texcoords_buf);
        model->sg_texcoords_buf = sg_make_buffer(&desc);
    }

    if (model->positions.length > 0) {
        sg_update_buffer(model->sg_positions_buf, 
                &(sg_range) { model->positions.data, sizeof(vec3) * model->positions.length });
        sg_update_buffer(model->sg_normals_buf, 
                &(sg_range) { model->normals.data, sizeof(vec3) * model->normals.length });
        sg_update_buffer(model->sg_texcoords_buf, 
                &(sg_range) { model->texcoords.data, sizeof(vec2) * model->texcoords.length });
    }
}
*/

static bool _golf_model_finalize(void *ptr) {
    golf_model_t *model = (golf_model_t*) ptr;
    sg_buffer_desc desc = {
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .usage = SG_USAGE_IMMUTABLE,
    };

    desc.data.ptr = model->positions.data,
    desc.data.size = sizeof(vec3) * model->positions.length,
    model->sg_positions_buf = sg_make_buffer(&desc);

    desc.data.ptr = model->normals.data,
    desc.data.size = sizeof(vec3) * model->normals.length,
    model->sg_normals_buf = sg_make_buffer(&desc);

    desc.data.ptr = model->texcoords.data,
    desc.data.size = sizeof(vec2) * model->texcoords.length,
    model->sg_texcoords_buf = sg_make_buffer(&desc);

    return true;
}

typedef struct _fast_obj_user_data {
    const char *path;
    char *data;
    int data_len, data_pos;
} _fast_obj_user_data_t;

static void *_fast_obj_file_open(const char *path, void *user_data) {
    _fast_obj_user_data_t *data = (_fast_obj_user_data_t*)user_data;
    if (strcmp(path, data->path) == 0) {
        return (void*)(uintptr_t)1;
    }
    else {
        return 0;
    }
}

static void _fast_obj_file_close(void *file, void *user_data) {
    GOLF_UNUSED(file);
    GOLF_UNUSED(user_data);
}

static size_t _fast_obj_file_read(void *file, void *dst, size_t bytes, void *user_data) {
    GOLF_UNUSED(file);

    _fast_obj_user_data_t *data = (_fast_obj_user_data_t*)user_data;
    if (data->data_pos + (int)bytes >= data->data_len) {
        bytes = (size_t)(data->data_len - data->data_pos);
    }

    memcpy(dst, data->data, bytes);
    data->data_pos += (int)bytes;
    return bytes;
}

static unsigned long _fast_obj_file_size(void *file, void *user_data) {
    GOLF_UNUSED(file);

    _fast_obj_user_data_t *data = (_fast_obj_user_data_t*)user_data;
    return (unsigned long)data->data_len;
}

static bool _golf_model_load(void *ptr, const char *path, char *data, int data_len, char *meta_data, int meta_data_len) {
    GOLF_UNUSED(path);
    GOLF_UNUSED(data_len);
    GOLF_UNUSED(meta_data);
    GOLF_UNUSED(meta_data_len);

    fastObjCallbacks callbacks;
    callbacks.file_open = _fast_obj_file_open;
    callbacks.file_close = _fast_obj_file_close;
    callbacks.file_read = _fast_obj_file_read;
    callbacks.file_size = _fast_obj_file_size;

    _fast_obj_user_data_t fast_obj_user_data;
    fast_obj_user_data.path = path;
    fast_obj_user_data.data = data;
    fast_obj_user_data.data_pos = 0;
    fast_obj_user_data.data_len = data_len;
    fastObjMesh *m = fast_obj_read_with_callbacks(path, &callbacks, &fast_obj_user_data);

    _vec_model_material_data model_materials;
    vec_init(&model_materials, "data");

    for (int i = 0; i < (int)m->group_count; i++) {
        const fastObjGroup grp = m->groups[i];

        int idx = 0;
        for (int j = 0; j < (int)grp.face_count; j++) {
            int fv = m->face_vertices[grp.face_offset + j];
            int fm = m->face_materials[grp.face_offset + j];
            const char *material_name = "default";
            if (fm < (int)m->material_count) {
                fastObjMaterial mat = m->materials[fm];
                material_name = mat.name;
            }

            _model_material_data_t *model_material = NULL;
            for (int i = 0; i < model_materials.length; i++) {
                if (strcmp(model_materials.data[i].name, material_name) == 0) {
                    model_material = &model_materials.data[i];
                }
            }
            if (!model_material) {
                _model_material_data_t mat;
                mat.name = material_name;
                vec_init(&mat.vertices, "data");
                vec_push(&model_materials, mat);
                model_material = &vec_last(&model_materials);
            }

            fastObjIndex m0 = m->indices[grp.index_offset + idx];
            vec3 p0 = vec3_create_from_array(&m->positions[3 * m0.p]);
            vec2 t0 = vec2_create_from_array(&m->texcoords[2 * m0.t]);
            vec3 n0 = vec3_create_from_array(&m->normals[3 * m0.n]);

            for (int k = 0; k < fv - 2; k++) {
                fastObjIndex m1 = m->indices[grp.index_offset + idx + k + 1];
                vec3 p1 = vec3_create_from_array(&m->positions[3 * m1.p]);
                vec2 t1 = vec2_create_from_array(&m->texcoords[2 * m1.t]);
                vec3 n1 = vec3_create_from_array(&m->normals[3 * m1.n]);

                fastObjIndex m2 = m->indices[grp.index_offset + idx + k + 2];
                vec3 p2 = vec3_create_from_array(&m->positions[3 * m2.p]);
                vec2 t2 = vec2_create_from_array(&m->texcoords[2 * m2.t]);
                vec3 n2 = vec3_create_from_array(&m->normals[3 * m2.n]);

                vec_push(&model_material->vertices, p0.x);
                vec_push(&model_material->vertices, p0.y);
                vec_push(&model_material->vertices, p0.z);
                vec_push(&model_material->vertices, n0.x);
                vec_push(&model_material->vertices, n0.y);
                vec_push(&model_material->vertices, n0.z);
                vec_push(&model_material->vertices, t0.x);
                vec_push(&model_material->vertices, t0.y);

                vec_push(&model_material->vertices, p1.x);
                vec_push(&model_material->vertices, p1.y);
                vec_push(&model_material->vertices, p1.z);
                vec_push(&model_material->vertices, n1.x);
                vec_push(&model_material->vertices, n1.y);
                vec_push(&model_material->vertices, n1.z);
                vec_push(&model_material->vertices, t1.x);
                vec_push(&model_material->vertices, t1.y);

                vec_push(&model_material->vertices, p2.x);
                vec_push(&model_material->vertices, p2.y);
                vec_push(&model_material->vertices, p2.z);
                vec_push(&model_material->vertices, n2.x);
                vec_push(&model_material->vertices, n2.y);
                vec_push(&model_material->vertices, n2.z);
                vec_push(&model_material->vertices, t2.x);
                vec_push(&model_material->vertices, t2.y);
            }

            idx += fv;
        }
    }

    golf_model_t *model = (golf_model_t*) ptr;
    model->is_water = false;
    vec_init(&model->groups, "data");
    vec_init(&model->positions, "data");
    vec_init(&model->normals, "data");
    vec_init(&model->texcoords, "data");
    for (int i = 0; i < model_materials.length; i++) {
        _model_material_data_t model_material = model_materials.data[i];
        const char *material_name = model_material.name;  
        
        golf_model_group_t model_group;
        snprintf(model_group.material_name, GOLF_MAX_NAME_LEN, "%s", material_name);
        model_group.start_vertex = model->positions.length;
        model_group.vertex_count = model_material.vertices.length / 8;
        for (int j = 0; j < model_material.vertices.length; j += 8) {
            vec3 p, n;
            vec2 t;
            p.x = model_material.vertices.data[j + 0];
            p.y = model_material.vertices.data[j + 1];
            p.z = model_material.vertices.data[j + 2];
            n.x = model_material.vertices.data[j + 3];
            n.y = model_material.vertices.data[j + 4];
            n.z = model_material.vertices.data[j + 5];
            t.x = model_material.vertices.data[j + 6];
            t.y = model_material.vertices.data[j + 7];
            vec_push(&model->positions, p);
            vec_push(&model->texcoords, t);
            vec_push(&model->normals, n);
        }
        vec_push(&model->groups, model_group);
    }

    fast_obj_destroy(m);
    vec_deinit(&model_materials);

    return true;
}

static bool _golf_model_unload(void *ptr) {
    golf_model_t *model = (golf_model_t*) ptr;
    sg_destroy_buffer(model->sg_positions_buf);
    sg_destroy_buffer(model->sg_normals_buf);
    sg_destroy_buffer(model->sg_texcoords_buf);
    vec_deinit(&model->positions);
    vec_deinit(&model->normals);
    vec_deinit(&model->texcoords);
    return true;
}

//
// UI PIXEL PACK
//

static void _golf_pixel_pack_pos_to_uvs(golf_pixel_pack_t *pp, int tex_w, int tex_h, vec2 p, vec2 *uv0, vec2 *uv1) {
    uv0->x = (pp->tile_size + pp->tile_padding) * p.x;
    uv0->x /= tex_w;
    uv0->y = (pp->tile_size + pp->tile_padding) * p.y;
    uv0->y /= tex_h;

    uv1->x = (pp->tile_size + pp->tile_padding) * (p.x + 1) - pp->tile_padding;
    uv1->x /= tex_w;
    uv1->y = (pp->tile_size + pp->tile_padding) * (p.y + 1) - pp->tile_padding;
    uv1->y /= tex_h;
}

static bool _golf_pixel_pack_load(void *ptr, const char *path, char *data, int data_len, char *meta_data, int meta_data_len) {
    GOLF_UNUSED(path);
    GOLF_UNUSED(data_len);
    GOLF_UNUSED(meta_data);
    GOLF_UNUSED(meta_data_len);

    golf_pixel_pack_t *pixel_pack = (golf_pixel_pack_t*) ptr;
    JSON_Value *val = json_parse_string(data);
    JSON_Object *obj = json_value_get_object(val);

    const char *texture_path = json_object_get_string(obj, "texture");
    _golf_data_thread_load_file(golf_file(texture_path));
    while (golf_data_get_load_state(texture_path) != GOLF_DATA_LOADED) 
        golf_thread_timer_wait(&_data_thread_timer, 10000000);
    pixel_pack->texture = golf_data_get_texture(texture_path);
    pixel_pack->tile_size = (float)json_object_get_number(obj, "tile_size");
    pixel_pack->tile_padding = (float)json_object_get_number(obj, "tile_padding");
    map_init(&pixel_pack->icons, "data");
    map_init(&pixel_pack->squares, "data");

    golf_texture_t *tex = pixel_pack->texture;

    JSON_Array *icons_array = json_object_get_array(obj, "icons");
    for (int i = 0; i < (int)json_array_get_count(icons_array); i++) {
        JSON_Object *icon_obj = json_array_get_object(icons_array, i);
        const char *name = json_object_get_string(icon_obj, "name");
        vec2 pos = golf_json_object_get_vec2(icon_obj, "pos");

        golf_pixel_pack_icon_t icon;
        _golf_pixel_pack_pos_to_uvs(pixel_pack, tex->width, tex->height, pos, &icon.uv0, &icon.uv1);
        map_set(&pixel_pack->icons, name, icon);
    }

    JSON_Array *squares_array = json_object_get_array(obj, "squares");
    for (int i = 0; i < (int)json_array_get_count(squares_array); i++) {
        JSON_Object *square_obj = json_array_get_object(squares_array, i);
        const char *name = json_object_get_string(square_obj, "name");
        vec2 tl = golf_json_object_get_vec2(square_obj, "top_left");
        vec2 tm = golf_json_object_get_vec2(square_obj, "top_mid");
        vec2 tr = golf_json_object_get_vec2(square_obj, "top_right");
        vec2 ml = golf_json_object_get_vec2(square_obj, "mid_left");
        vec2 mm = golf_json_object_get_vec2(square_obj, "mid_mid");
        vec2 mr = golf_json_object_get_vec2(square_obj, "mid_right");
        vec2 bl = golf_json_object_get_vec2(square_obj, "bot_left");
        vec2 bm = golf_json_object_get_vec2(square_obj, "bot_mid");
        vec2 br = golf_json_object_get_vec2(square_obj, "bot_right");

        golf_pixel_pack_square_t square;
        _golf_pixel_pack_pos_to_uvs(pixel_pack, tex->width, tex->height, tl, &square.tl_uv0, &square.tl_uv1);
        _golf_pixel_pack_pos_to_uvs(pixel_pack, tex->width, tex->height, tm, &square.tm_uv0, &square.tm_uv1);
        _golf_pixel_pack_pos_to_uvs(pixel_pack, tex->width, tex->height, tr, &square.tr_uv0, &square.tr_uv1);
        _golf_pixel_pack_pos_to_uvs(pixel_pack, tex->width, tex->height, ml, &square.ml_uv0, &square.ml_uv1);
        _golf_pixel_pack_pos_to_uvs(pixel_pack, tex->width, tex->height, mm, &square.mm_uv0, &square.mm_uv1);
        _golf_pixel_pack_pos_to_uvs(pixel_pack, tex->width, tex->height, mr, &square.mr_uv0, &square.mr_uv1);
        _golf_pixel_pack_pos_to_uvs(pixel_pack, tex->width, tex->height, bl, &square.bl_uv0, &square.bl_uv1);
        _golf_pixel_pack_pos_to_uvs(pixel_pack, tex->width, tex->height, bm, &square.bm_uv0, &square.bm_uv1);
        _golf_pixel_pack_pos_to_uvs(pixel_pack, tex->width, tex->height, br, &square.br_uv0, &square.br_uv1);
        map_set(&pixel_pack->squares, name, square);
    }

    json_value_free(val);
    return true;
}

static bool _golf_pixel_pack_unload(void *ptr) {
    golf_pixel_pack_t *pixel_pack = (golf_pixel_pack_t*) ptr;
    map_deinit(&pixel_pack->icons);
    map_deinit(&pixel_pack->squares);
    return true;
}

static bool _golf_ui_layout_load_entity(JSON_Object *entity_obj, golf_ui_layout_entity_t *entity) {
    const char *type = json_object_get_string(entity_obj, "type");
    if (!type) {
        golf_log_warning("No ui entity type %s", type);
        return false;
    }

    const char *name = json_object_get_string(entity_obj, "name");
    if (name) {
        snprintf(entity->name, GOLF_MAX_NAME_LEN, "%s", name);
    }
    else {
        entity->name[0] = 0;
    }

    const char *parent_name = json_object_get_string(entity_obj, "parent");
    if (parent_name) {
        snprintf(entity->parent_name, GOLF_MAX_NAME_LEN, "%s", parent_name);
    }
    else {
        entity->parent_name[0] = 0;
    }

    entity->pos = golf_json_object_get_vec2(entity_obj, "pos");
    entity->size = golf_json_object_get_vec2(entity_obj, "size");
    entity->anchor = golf_json_object_get_vec2(entity_obj, "anchor");

    if (strcmp(type, "pixel_pack_square") == 0) {
        const char *pixel_pack_path = json_object_get_string(entity_obj, "pixel_pack");
        const char *square_name = json_object_get_string(entity_obj, "square");

        entity->type = GOLF_UI_PIXEL_PACK_SQUARE;
        entity->pixel_pack_square.pixel_pack = golf_data_get_pixel_pack(pixel_pack_path);
        snprintf(entity->pixel_pack_square.square_name, GOLF_MAX_NAME_LEN, "%s", square_name);
        entity->pixel_pack_square.tile_size = (float)json_object_get_number(entity_obj, "tile_size");
        entity->pixel_pack_square.overlay_color = golf_json_object_get_vec4(entity_obj, "overlay_color");
    }
    else if (strcmp(type, "text") == 0) {
        const char *font_path = json_object_get_string(entity_obj, "font");
        const char *text = json_object_get_string(entity_obj, "text");

        entity->type = GOLF_UI_TEXT;
        entity->text.font = golf_data_get_font(font_path);
        golf_string_init(&entity->text.text, "ui_layout", text);
        entity->text.font_size = (float)json_object_get_number(entity_obj, "font_size");
        entity->text.color = golf_json_object_get_vec4(entity_obj, "color");
        entity->text.horiz_align = (int)json_object_get_number(entity_obj, "horiz_align");
        entity->text.vert_align = (int)json_object_get_number(entity_obj, "vert_align");
    }
    else if (strcmp(type, "button") == 0) {
        vec_init(&entity->button.up_entities, "ui_layout");
        vec_init(&entity->button.down_entities, "ui_layout");

        entity->type = GOLF_UI_BUTTON;

        JSON_Array *up_entities_arr = json_object_get_array(entity_obj, "up_entities");
        for (int i = 0; i < (int)json_array_get_count(up_entities_arr); i++) {
            JSON_Object *up_entity_obj = json_array_get_object(up_entities_arr, i);
            golf_ui_layout_entity_t up_entity;
            if (_golf_ui_layout_load_entity(up_entity_obj, &up_entity)) {
                vec_push(&entity->button.up_entities, up_entity);
            }
        }

        JSON_Array *down_entities_arr = json_object_get_array(entity_obj, "down_entities");
        for (int i = 0; i < (int)json_array_get_count(down_entities_arr); i++) {
            JSON_Object *down_entity_obj = json_array_get_object(down_entities_arr, i);
            golf_ui_layout_entity_t down_entity;
            if (_golf_ui_layout_load_entity(down_entity_obj, &down_entity)) {
                vec_push(&entity->button.down_entities, down_entity);
            }
        }
    }
    else if (strcmp(type, "gif_texture") == 0) {
        entity->type = GOLF_UI_GIF_TEXTURE;

        const char *texture_path = json_object_get_string(entity_obj, "texture");
        float total_time = (float)json_object_get_number(entity_obj, "time");

        entity->gif_texture.total_time = total_time;
        entity->gif_texture.texture = golf_data_get_gif_texture(texture_path);
        entity->gif_texture.t = 0;
    }
    else if (strcmp(type, "aim_circle") == 0) {
        entity->type = GOLF_UI_AIM_CIRCLE;

        const char *texture_path = json_object_get_string(entity_obj, "texture");
        float total_time = (float)json_object_get_number(entity_obj, "time");
        vec2 square_size = golf_json_object_get_vec2(entity_obj, "square_size");
        int num_squares = (int)json_object_get_number(entity_obj, "num_squares");

        entity->aim_circle.total_time = total_time;
        entity->aim_circle.texture = golf_data_get_texture(texture_path);
        entity->aim_circle.square_size = square_size;
        entity->aim_circle.num_squares = num_squares;
        entity->aim_circle.t = 0;
    }
    else if (strcmp(type, "level_select_scroll_box") == 0) {
        entity->type = GOLF_UI_LEVEL_SELECT_SCROLL_BOX;

        vec2 button_size = golf_json_object_get_vec2(entity_obj, "button_size");
        float button_tile_size = (float)json_object_get_number(entity_obj, "button_tile_size");
        const char *button_pixel_pack_path = json_object_get_string(entity_obj, "button_pixel_pack");
        const char *button_up_square = json_object_get_string(entity_obj, "button_up_square");
        const char *button_down_square = json_object_get_string(entity_obj, "button_down_square");
        const char *button_font_path = json_object_get_string(entity_obj, "button_font");
        float button_best_text_size = (float)json_object_get_number(entity_obj, "button_best_text_size");
        vec4 button_best_text_color = golf_json_object_get_vec4(entity_obj, "button_best_text_color");
        vec2 button_best_text_offset = golf_json_object_get_vec2(entity_obj, "button_best_text_offset");
        float button_num_text_size = (float)json_object_get_number(entity_obj, "button_num_text_size");
        vec4 button_num_text_color = golf_json_object_get_vec4(entity_obj, "button_num_text_color");
        vec2 button_num_text_offset = golf_json_object_get_vec2(entity_obj, "button_num_text_offset");
        vec2 button_down_text_offset = golf_json_object_get_vec2(entity_obj, "button_down_text_offset");
        float scroll_bar_background_width = (float)json_object_get_number(entity_obj, "scroll_bar_background_width");
        float scroll_bar_background_padding = (float)json_object_get_number(entity_obj, "scroll_bar_background_padding");
        float scroll_bar_tile_size = (float)json_object_get_number(entity_obj, "scroll_bar_tile_size");
        const char *scroll_bar_square_name = json_object_get_string(entity_obj, "scroll_bar_square");
        float scroll_bar_width = (float)json_object_get_number(entity_obj, "scroll_bar_width");
        float scroll_bar_height = (float)json_object_get_number(entity_obj, "scroll_bar_height");
        float scroll_bar_leeway = (float)json_object_get_number(entity_obj, "scroll_bar_leeway");
        float scroll_bar_leeway_fix_speed = (float)json_object_get_number(entity_obj, "scroll_bar_leeway_fix_speed");
        vec4 scroll_bar_down_overlay = golf_json_object_get_vec4(entity_obj, "scroll_bar_down_overlay");

        entity->level_select_scroll_box.down_delta = 0;
        entity->level_select_scroll_box.button_size = button_size;
        entity->level_select_scroll_box.button_tile_size = button_tile_size;
        entity->level_select_scroll_box.button_pixel_pack = golf_data_get_pixel_pack(button_pixel_pack_path);
        entity->level_select_scroll_box.button_font = golf_data_get_font(button_font_path);
        entity->level_select_scroll_box.button_best_text_size = button_best_text_size;
        entity->level_select_scroll_box.button_best_text_color = button_best_text_color;
        entity->level_select_scroll_box.button_best_text_offset = button_best_text_offset;
        entity->level_select_scroll_box.button_num_text_size = button_num_text_size;
        entity->level_select_scroll_box.button_num_text_color = button_num_text_color;
        entity->level_select_scroll_box.button_num_text_offset = button_num_text_offset;
        entity->level_select_scroll_box.button_down_text_offset = button_down_text_offset;
        snprintf(entity->level_select_scroll_box.button_up_square_name, GOLF_MAX_NAME_LEN, "%s", button_up_square);
        snprintf(entity->level_select_scroll_box.button_down_square_name, GOLF_MAX_NAME_LEN, "%s", button_down_square);
        entity->level_select_scroll_box.scroll_bar_background_width = scroll_bar_background_width;
        entity->level_select_scroll_box.scroll_bar_background_padding = scroll_bar_background_padding;
        entity->level_select_scroll_box.scroll_bar_tile_size = scroll_bar_tile_size;
        snprintf(entity->level_select_scroll_box.scroll_bar_square_name, GOLF_MAX_NAME_LEN, "%s", scroll_bar_square_name);
        entity->level_select_scroll_box.scroll_bar_width = scroll_bar_width;
        entity->level_select_scroll_box.scroll_bar_height = scroll_bar_height;
        entity->level_select_scroll_box.scroll_bar_leeway = scroll_bar_leeway;
        entity->level_select_scroll_box.scroll_bar_leeway_fix_speed = scroll_bar_leeway_fix_speed;
        entity->level_select_scroll_box.scroll_bar_down_overlay = scroll_bar_down_overlay;
    }
    else {
        golf_log_warning("Unknown ui entity type %s", type);
        return false;
    }

    return true;
}

static void _golf_ui_layout_get_entity_dependency(JSON_Object *entity_obj, vec_golf_file_t *deps) {
    const char *type = json_object_get_string(entity_obj, "type");
    if (type && strcmp(type, "pixel_pack_square") == 0) {
        const char *pixel_pack_path = json_object_get_string(entity_obj, "pixel_pack");
        if (pixel_pack_path) _golf_data_add_dependency(deps, golf_file(pixel_pack_path));
    }
    else if (type && strcmp(type, "text") == 0) {
        const char *font_path = json_object_get_string(entity_obj, "font");
        if (font_path) _golf_data_add_dependency(deps, golf_file(font_path));
    }
    else if (type && strcmp(type, "gif_texture") == 0) {
        const char *texture_path = json_object_get_string(entity_obj, "texture");
        if (texture_path) _golf_data_add_dependency(deps, golf_file(texture_path));
    }
    else if (type && strcmp(type, "button") == 0) {
        JSON_Array *up_entities_arr = json_object_get_array(entity_obj, "up_entities");
        for (int i = 0; i < (int)json_array_get_count(up_entities_arr); i++) {
            _golf_ui_layout_get_entity_dependency(json_array_get_object(up_entities_arr, i), deps);
        }

        JSON_Array *down_entities_arr = json_object_get_array(entity_obj, "down_entities");
        for (int i = 0; i < (int)json_array_get_count(down_entities_arr); i++) {
            _golf_ui_layout_get_entity_dependency(json_array_get_object(down_entities_arr, i), deps);
        }
    }
    else if (type && strcmp(type, "aim_circle") == 0) {
        const char *texture_path = json_object_get_string(entity_obj, "texture");
        if (texture_path) _golf_data_add_dependency(deps, golf_file(texture_path));
    }
    else if (type && strcmp(type, "level_select_scroll_box") == 0) {
        const char *texture_path = json_object_get_string(entity_obj, "button_pixel_pack");
        if (texture_path) _golf_data_add_dependency(deps, golf_file(texture_path));

        const char *font_path = json_object_get_string(entity_obj, "button_font");
        if (font_path) _golf_data_add_dependency(deps, golf_file(font_path));
    }

}

static bool _golf_ui_layout_load(void *ptr, const char *path, char *data, int data_len, char *meta_data, int meta_data_len) {
    GOLF_UNUSED(path);
    GOLF_UNUSED(data_len);
    GOLF_UNUSED(meta_data);
    GOLF_UNUSED(meta_data_len);

    golf_ui_layout_t *layout = (golf_ui_layout_t*) ptr;
    vec_init(&layout->entities, "ui_layout");

    JSON_Value *json_val = json_parse_string(data);
    JSON_Object *json_obj = json_value_get_object(json_val);

    JSON_Array *entities_arr = json_object_get_array(json_obj, "entities");

    vec_golf_file_t deps;
    vec_init(&deps, "data");
    for (int i = 0; i < (int)json_array_get_count(entities_arr); i++) {
        JSON_Object *entity_obj = json_array_get_object(entities_arr, i);
        _golf_ui_layout_get_entity_dependency(entity_obj, &deps);
    }
    for (int i = 0; i < deps.length; i++) {
        _golf_data_thread_load_file(deps.data[i]);
        while (golf_data_get_load_state(deps.data[i].path) != GOLF_DATA_LOADED)
            golf_thread_timer_wait(&_data_thread_timer, 10000000);
    }
    vec_deinit(&deps);

    for (int i = 0; i < (int)json_array_get_count(entities_arr); i++) {
        JSON_Object *entity_obj = json_array_get_object(entities_arr, i);

        golf_ui_layout_entity_t entity;
        if (_golf_ui_layout_load_entity(entity_obj, &entity)) {
            vec_push(&layout->entities, entity);
        }
    }

    return true;
}

static void _golf_ui_layout_unload_entity(golf_ui_layout_entity_t *entity) {
    switch (entity->type) {
        case GOLF_UI_PIXEL_PACK_SQUARE:
        case GOLF_UI_GIF_TEXTURE:
        case GOLF_UI_AIM_CIRCLE:
            break;
        case GOLF_UI_TEXT:
            golf_string_deinit(&entity->text.text);
            break;
        case GOLF_UI_BUTTON:
            for (int i = 0; i < entity->button.up_entities.length; i++) {
                _golf_ui_layout_unload_entity(&entity->button.up_entities.data[i]);
            }
            vec_deinit(&entity->button.up_entities);

            for (int i = 0; i < entity->button.down_entities.length; i++) {
                _golf_ui_layout_unload_entity(&entity->button.down_entities.data[i]);
            }
            vec_deinit(&entity->button.down_entities);
            break;
    }
}

static bool _golf_ui_layout_unload(void *ptr) {
    golf_ui_layout_t *layout = (golf_ui_layout_t*) ptr;
    for (int i = 0; i < layout->entities.length; i++) {
        _golf_ui_layout_unload_entity(&layout->entities.data[i]);
    }
    vec_deinit(&layout->entities);
    return true;
}

//
// CONFIG
//

static bool _golf_config_load(void *ptr, const char *path, char *data, int data_len, char *meta_data, int meta_data_len) {
    GOLF_UNUSED(path);
    GOLF_UNUSED(data_len);
    GOLF_UNUSED(meta_data);
    GOLF_UNUSED(meta_data_len);

    golf_config_t *config = (golf_config_t*) ptr;
    JSON_Value *val = json_parse_string(data);
    JSON_Object *obj = json_value_get_object(val);

    if (!val || !obj) {
        golf_log_warning("Can't parse json for config file %s", path);
        return false;
    }

    map_init(&config->properties, "data");

    for (int i = 0; i < (int)json_object_get_count(obj); i++) {
        const char *name = json_object_get_name(obj, i);
        JSON_Value *prop_val = json_object_get_value_at(obj, i);
        JSON_Value_Type type = json_value_get_type(prop_val);

        bool valid_property = false;
        golf_config_property_t data_property;  
        if (type == JSONNumber) {
            double prop_num = json_value_get_number(prop_val);
            data_property.type = GOLF_CONFIG_PROPERTY_NUM;
            data_property.num_val = (float)prop_num;
            valid_property = true;
        }
        else if (type == JSONString) {
            const char *prop_string = json_value_get_string(prop_val);
            data_property.type = GOLF_CONFIG_PROPERTY_STRING;
            data_property.string_val = golf_alloc(strlen(prop_string) + 1);
            strcpy(data_property.string_val, prop_string);
            valid_property = true;
        }
        else if (type == JSONArray) {
            JSON_Array *prop_array = json_value_get_array(prop_val);
            if (json_array_get_count(prop_array) == 2) {
                data_property.type = GOLF_CONFIG_PROPERTY_VEC2;
                data_property.vec2_val = golf_json_object_get_vec2(obj, name);
                valid_property = true;
            }
            else if (json_array_get_count(prop_array) == 3) {
                data_property.type = GOLF_CONFIG_PROPERTY_VEC3;
                data_property.vec3_val = golf_json_object_get_vec3(obj, name);
                valid_property = true;
            }
            else if (json_array_get_count(prop_array) == 4) {
                data_property.type = GOLF_CONFIG_PROPERTY_VEC4;
                data_property.vec4_val = golf_json_object_get_vec4(obj, name);
                valid_property = true;
            }
        }

        if (valid_property) {
            map_set(&config->properties, name, data_property);
        }
        else {
            golf_log_warning("Property %s is invalid", name);
        }
    }

    json_value_free(val);
    return true;
}

static bool _golf_config_unload(void *ptr) {
    golf_config_t *config = (golf_config_t*) ptr;
    const char *key;
    map_iter_t iter = map_iter(&config->properties);

    while ((key = map_next(&config->properties, &iter))) {
        golf_config_property_t *prop = map_get(&config->properties, key);
        switch (prop->type) {
            case GOLF_CONFIG_PROPERTY_NUM:
            case GOLF_CONFIG_PROPERTY_VEC2:
            case GOLF_CONFIG_PROPERTY_VEC3:
            case GOLF_CONFIG_PROPERTY_VEC4:
                break;
            case GOLF_CONFIG_PROPERTY_STRING:
                golf_free(prop->string_val);
                break;
        }
    }

    map_deinit(&config->properties);
    return true;
}

float golf_config_get_num(golf_config_t *cfg, const char *name) {
    golf_config_property_t *prop = map_get(&cfg->properties, name);
    if (prop && prop->type == GOLF_CONFIG_PROPERTY_NUM) {
        return prop->num_val;
    }
    else {
        golf_log_warning("Invalid config property %s", name);
        return 0.0f;
    }
}

const char *golf_config_get_string(golf_config_t *cfg, const char *name) {
    golf_config_property_t *prop = map_get(&cfg->properties, name);
    if (prop && prop->type == GOLF_CONFIG_PROPERTY_STRING) {
        return prop->string_val;
    }
    else {
        golf_log_warning("Invalid config property %s", name);
        return "";
    }
}

vec2 golf_config_get_vec2(golf_config_t *cfg, const char *name) {
    golf_config_property_t *prop = map_get(&cfg->properties, name);
    if (prop && prop->type == GOLF_CONFIG_PROPERTY_VEC2) {
        return prop->vec2_val;
    }
    else {
        golf_log_warning("Invalid config property %s", name);
        return V2(0, 0);
    }
}

vec3 golf_config_get_vec3(golf_config_t *cfg, const char *name) {
    golf_config_property_t *prop = map_get(&cfg->properties, name);
    if (prop && prop->type == GOLF_CONFIG_PROPERTY_VEC3) {
        return prop->vec3_val;
    }
    else {
        golf_log_warning("Invalid config property %s", name);
        return V3(0, 0, 0);
    }
}

vec4 golf_config_get_vec4(golf_config_t *cfg, const char *name) {
    golf_config_property_t *prop = map_get(&cfg->properties, name);
    if (prop && prop->type == GOLF_CONFIG_PROPERTY_VEC4) {
        return prop->vec4_val;
    }
    else {
        golf_log_warning("Invalid config property %s", name);
        return V4(0, 0, 0, 0);
    }
}

//
// LEVEL
//

static void _golf_json_object_get_transform(JSON_Object *obj, const char *name, golf_transform_t *transform) {
    JSON_Object *transform_obj = json_object_get_object(obj, name);

    transform->position = golf_json_object_get_vec3(transform_obj, "position");
    transform->scale = golf_json_object_get_vec3(transform_obj, "scale");
    transform->rotation = golf_json_object_get_quat(transform_obj, "rotation");
}

static void _golf_json_object_get_lightmap_section(JSON_Object *obj, const char *name, golf_lightmap_section_t *lightmap_section) {
    JSON_Object *section_obj = json_object_get_object(obj, name);

    const char *lightmap_name = json_object_get_string(section_obj, "lightmap_name");

    float *uvs_arr;
    int uvs_arr_len;
    golf_json_object_get_float_array(section_obj, "uvs", &uvs_arr, &uvs_arr_len);

    vec_vec2_t uvs;
    vec_init(&uvs, "level");
    for (int i = 0; i < uvs_arr_len; i += 2) {
        vec_push(&uvs, V2(uvs_arr[i], uvs_arr[i + 1]));
    }

    golf_free(uvs_arr);

    *lightmap_section = golf_lightmap_section(lightmap_name, uvs);
}

static void _golf_json_object_get_movement(JSON_Object *obj, const char *name, golf_movement_t *movement) {
    JSON_Object *movement_obj = json_object_get_object(obj, name);

    const char *type = json_object_get_string(movement_obj, "type");
    if (type && strcmp(type, "none") == 0) {
        *movement = golf_movement_none();
    }
    else if (type && strcmp(type, "linear") == 0) {
        float t0 = (float)json_object_get_number(movement_obj, "t0");
        float length = (float)json_object_get_number(movement_obj, "length");
        vec3 p0 = golf_json_object_get_vec3(movement_obj, "p0");
        vec3 p1 = golf_json_object_get_vec3(movement_obj, "p1");
        *movement = golf_movement_linear(t0, p0, p1, length);
    }
    else if (type && strcmp(type, "spinner") == 0) {
        float t0 = (float)json_object_get_number(movement_obj, "t0");
        float length = (float)json_object_get_number(movement_obj, "length");
        *movement = golf_movement_spinner(t0, length);
    }
    else {
        golf_log_warning("Invalid type for movement");
        *movement = golf_movement_none();
    }
}

static void _golf_json_object_get_geo(JSON_Object *obj, const char *name, golf_geo_t *geo, bool is_water) {
    JSON_Object *geo_obj = json_object_get_object(obj, name);

    vec_golf_geo_point_t points;
    vec_init(&points, "geo");

    vec_golf_geo_face_t faces;
    vec_init(&faces, "geo");

    vec_golf_geo_generator_data_arg_t args;
    vec_init(&args, "geo");

    golf_script_t *script = NULL;

    JSON_Array *p_arr = json_object_get_array(geo_obj, "p");
    for (int i = 0; i < (int)json_array_get_count(p_arr); i += 3) {
        float x = (float)json_array_get_number(p_arr, i);
        float y = (float)json_array_get_number(p_arr, i + 1);
        float z = (float)json_array_get_number(p_arr, i + 2);

        vec_push(&points, golf_geo_point(V3(x, y, z)));
    }

    JSON_Array *faces_arr = json_object_get_array(geo_obj, "faces");
    for (int i = 0; i < (int)json_array_get_count(faces_arr); i++) {
        JSON_Object *face_obj = json_array_get_object(faces_arr, i);
        const char *material_name = json_object_get_string(face_obj, "material_name");
        JSON_Array *idxs_arr = json_object_get_array(face_obj, "idxs");
        JSON_Array *uvs_arr = json_object_get_array(face_obj, "uvs");
        int idxs_count = (int)json_array_get_count(idxs_arr);
        vec_int_t idxs;
        vec_init(&idxs, "geo");
        vec_vec2_t uvs;
        vec_init(&uvs, "geo");
        for (int i = 0; i < idxs_count; i++) {
            vec_push(&idxs, (int)json_array_get_number(idxs_arr, i));
            vec_push(&uvs, V2((float)json_array_get_number(uvs_arr, 2*i), (float)json_array_get_number(uvs_arr, 2*i + 1)));
        }
        const char *uv_gen_type_str = json_object_get_string(face_obj, "uv_gen_type");
        golf_geo_face_uv_gen_type_t uv_gen_type = GOLF_GEO_FACE_UV_GEN_MANUAL;
        for (int i = 0; i < GOLF_GEO_FACE_UV_GEN_COUNT; i++) {
            if (uv_gen_type_str && strcmp(uv_gen_type_str, golf_geo_uv_gen_type_strings()[i]) == 0) {
                uv_gen_type = i;
            }
        }
        vec3 water_dir = golf_json_object_get_vec3(face_obj, "water_dir");

        vec_push(&faces, golf_geo_face(material_name, idxs, uv_gen_type, uvs, water_dir));
    }

    JSON_Object *generator_data_obj = json_object_get_object(geo_obj, "generator_data");
    if (generator_data_obj) {
        const char *script_path = json_object_get_string(generator_data_obj, "script");
        script = golf_data_get_script(script_path);

        JSON_Array *args_arr = json_object_get_array(generator_data_obj, "args");
        for (int i = 0; i < (int)json_array_get_count(args_arr); i++) {
            JSON_Object *arg_obj = json_array_get_object(args_arr, i);
            const char *name = json_object_get_string(arg_obj, "name");
            if (!name) {
                golf_log_warning("No name for generator data argument");
            }

            const char *type = json_object_get_string(arg_obj, "type");
            if (!type) {
                golf_log_warning("No type for generator data argument");
            }

            bool valid_val = true;
            gs_val_t val;

            if (strcmp(type, "bool") == 0) {
                val = gs_val_bool((bool)json_object_get_number(arg_obj, "val"));
            }
            else if (strcmp(type, "int") == 0) {
                val = gs_val_int((int)json_object_get_number(arg_obj, "val"));
            }
            else if (strcmp(type, "float") == 0) {
                val = gs_val_float((float)json_object_get_number(arg_obj, "val"));
            }
            else if (strcmp(type, "vec2") == 0) {
                val = gs_val_vec2(golf_json_object_get_vec2(arg_obj, "val"));
            }
            else if (strcmp(type, "vec3") == 0) {
                val = gs_val_vec3(golf_json_object_get_vec3(arg_obj, "val"));
            }
            else {
                valid_val = false;
                golf_log_warning("Invalid type for generator data argument: %s", type);
            }

            if (valid_val) {
                golf_geo_generator_data_arg_t arg;
                snprintf(arg.name, GOLF_MAX_NAME_LEN, "%s", name);
                arg.val = val;
                vec_push(&args, arg);
            }
        }
    }

    golf_geo_generator_data_t generator_data = golf_geo_generator_data(script, args);

    *geo = golf_geo(points, faces, generator_data, is_water);
}

static bool _golf_level_finalize(void *ptr) {
    golf_level_t *level = (golf_level_t*) ptr;
    for (int i = 0; i < level->lightmap_images.length; i++) {
        golf_lightmap_image_finalize(&level->lightmap_images.data[i]);
    }
    for (int i = 0; i < level->entities.length; i++) {
        golf_entity_t *entity = &level->entities.data[i];

        golf_lightmap_section_t *lightmap_section = golf_entity_get_lightmap_section(entity); 
        if (lightmap_section) {
            golf_lightmap_section_finalize(lightmap_section);
        }

        golf_geo_t *geo = golf_entity_get_geo(entity);
        if (geo) {
            golf_geo_finalize(geo);
        }
    }
    return true;
}

static bool _golf_level_load(void *ptr, const char *path, char *data, int data_len, char *meta_data, int meta_data_len) {
    GOLF_UNUSED(path);
    GOLF_UNUSED(data_len);
    GOLF_UNUSED(meta_data);
    GOLF_UNUSED(meta_data_len);

    golf_level_t *level = (golf_level_t*) ptr;

    vec_init(&level->materials, "level");
    vec_init(&level->lightmap_images, "level");
    vec_init(&level->entities, "level");

    JSON_Value *json_val = json_parse_string(data);
    JSON_Object *json_obj = json_value_get_object(json_val);

    JSON_Array *json_materials_arr = json_object_get_array(json_obj, "materials");
    JSON_Array *json_lightmap_images_arr = json_object_get_array(json_obj, "lightmap_images");
    JSON_Array *json_entities_arr = json_object_get_array(json_obj, "entities");

    // load dependencies
    {
        vec_init(&level->deps, "level");
        _golf_data_add_dependency(&level->deps, golf_file("data/textures/hole_lightmap.png"));
        _golf_data_add_dependency(&level->deps, golf_file("data/models/hole.obj"));
        _golf_data_add_dependency(&level->deps, golf_file("data/models/hole-cover.obj"));
        _golf_data_add_dependency(&level->deps, golf_file("data/models/sphere.obj"));
        for (int i = 0; i < (int)json_array_get_count(json_materials_arr); i++) {
            JSON_Object *obj = json_array_get_object(json_materials_arr, i);
            const char *type = json_object_get_string(obj, "type");
            if (type && strcmp(type, "texture") == 0) {
                const char *texture = json_object_get_string(obj, "texture");
                if (texture) _golf_data_add_dependency(&level->deps, golf_file(texture));
            }
            else if (type && strcmp(type, "environment") == 0) {
                const char *texture = json_object_get_string(obj, "texture");
                if (texture) _golf_data_add_dependency(&level->deps, golf_file(texture));
            }
        }
        for (int i = 0; i < (int)json_array_get_count(json_entities_arr); i++) {
            JSON_Object *obj = json_array_get_object(json_entities_arr, i);
            const char *type = json_object_get_string(obj, "type");
            if (type && strcmp(type, "model") == 0) {
                const char *model = json_object_get_string(obj, "model");
                if (model) _golf_data_add_dependency(&level->deps, golf_file(model));
            }
        }
        for (int i = 0; i < level->deps.length; i++) {
            _golf_data_thread_load_file(level->deps.data[i]);
            while (golf_data_get_load_state(level->deps.data[i].path) != GOLF_DATA_LOADED) 
                golf_thread_timer_wait(&_data_thread_timer, 10000000);
        }
    }

    for (int i = 0; i < (int)json_array_get_count(json_materials_arr); i++) {
        JSON_Object *obj = json_array_get_object(json_materials_arr, i);
        const char *type = json_object_get_string(obj, "type");
        const char *name = json_object_get_string(obj, "name");
        float friction = (float)json_object_get_number(obj, "friction");
        float restitution = (float)json_object_get_number(obj, "restitution");
        float vel_scale = (float)json_object_get_number(obj, "vel_scale");

        bool valid_material = false;
        golf_material_t material;
        if (type && strcmp(type, "texture") == 0) {
            const char *texture_path = json_object_get_string(obj, "texture");
            material = golf_material_texture(name, friction, restitution, vel_scale, texture_path);
            valid_material = true;
        }
        else if (type && strcmp(type, "color") == 0) {
            vec4 color = golf_json_object_get_vec4(obj, "color");
            material = golf_material_color(name, friction, restitution, vel_scale, color);
            valid_material = true;
        }
        else if (type && strcmp(type, "diffuse_color") == 0) {
            vec4 color = golf_json_object_get_vec4(obj, "color");
            material = golf_material_diffuse_color(name, friction, restitution, vel_scale, color);
            valid_material = true;
        }
        else if (type && strcmp(type, "environment") == 0) {
            const char *texture_path = json_object_get_string(obj, "texture");
            material = golf_material_environment(name, friction, restitution, vel_scale, texture_path);
            valid_material = true;
        }

        if (valid_material) {
            vec_push(&level->materials, material);
        }
        else {
            golf_log_warning("Invalid material. type: %s, name: %s", type, name);
        }
    }

    for (int i = 0; i < (int)json_array_get_count(json_lightmap_images_arr); i++) {
        JSON_Object *obj = json_array_get_object(json_lightmap_images_arr, i);
        const char *name = json_object_get_string(obj, "name");
        int resolution = (int)json_object_get_number(obj, "resolution");
        float time_length = (float)json_object_get_number(obj, "time_length");
        bool repeats = (bool)json_object_get_boolean(obj, "repeats");

        int width, height, c;
        JSON_Array *datas_arr = json_object_get_array(obj, "datas");
        int num_samples = (int)json_array_get_count(datas_arr);
        unsigned char **image_datas = golf_alloc(sizeof(unsigned char*) * num_samples);
        for (int i = 0; i < num_samples; i++) {
            unsigned char *data;
            int data_len;
            golf_json_array_get_data(datas_arr, i, &data, &data_len);
            unsigned char *image_data = stbi_load_from_memory(data, data_len, &width, &height, &c, 1);
            image_datas[i] = image_data;
            golf_free(data);
        }

        sg_image *sg_images = golf_alloc(sizeof(sg_image) * num_samples);

        vec_push(&level->lightmap_images, golf_lightmap_image(name, resolution, width, height, time_length, repeats, num_samples, image_datas, sg_images));
    }

    for (int i = 0; i < (int)json_array_get_count(json_entities_arr); i++) {
        JSON_Object *obj = json_array_get_object(json_entities_arr, i);
        const char *type = json_object_get_string(obj, "type");
        const char *name = json_object_get_string(obj, "name");
        int parent_idx = (int)json_object_get_number(obj, "parent_idx");

        bool valid_entity = false;
        golf_entity_t entity;  
        entity.active = true;  
        if (type && strcmp(type, "model") == 0) {
            golf_transform_t transform;
            _golf_json_object_get_transform(obj, "transform", &transform);

            const char *model_path = json_object_get_string(obj, "model");

            float uv_scale = (float)json_object_get_number(obj, "uv_scale");

            golf_lightmap_section_t lightmap_section;
            _golf_json_object_get_lightmap_section(obj, "lightmap_section", &lightmap_section);

            golf_movement_t movement;
            _golf_json_object_get_movement(obj, "movement", &movement);

            entity = golf_entity_model(name, transform, model_path, uv_scale, lightmap_section, movement);
            valid_entity = true;
        }
        else if (type && strcmp(type, "ball-start") == 0) {
            golf_transform_t transform;
            _golf_json_object_get_transform(obj, "transform", &transform);

            entity = golf_entity_ball_start(name, transform);
            valid_entity = true;
        }
        else if (type && strcmp(type, "hole") == 0) {
            golf_transform_t transform;
            _golf_json_object_get_transform(obj, "transform", &transform);

            entity = golf_entity_hole(name, transform);
            valid_entity = true;
        }
        else if (type && strcmp(type, "geo") == 0) {
            golf_transform_t transform;
            _golf_json_object_get_transform(obj, "transform", &transform);

            golf_movement_t movement;
            _golf_json_object_get_movement(obj, "movement", &movement);

            golf_geo_t geo;
            _golf_json_object_get_geo(obj, "geo", &geo, false);

            golf_lightmap_section_t lightmap_section;
            _golf_json_object_get_lightmap_section(obj, "lightmap_section", &lightmap_section);

            entity = golf_entity_geo(name, transform, movement, geo, lightmap_section);
            valid_entity = true;
        }
        else if (type && strcmp(type, "group") == 0) {
            golf_transform_t transform;
            _golf_json_object_get_transform(obj, "transform", &transform);

            entity = golf_entity_group(name, transform);

            valid_entity = true;
        }
        else if (type && strcmp(type, "water") == 0) {
            golf_transform_t transform;
            _golf_json_object_get_transform(obj, "transform", &transform);

            golf_geo_t geo;
            _golf_json_object_get_geo(obj, "geo", &geo, true);

            golf_lightmap_section_t lightmap_section;
            _golf_json_object_get_lightmap_section(obj, "lightmap_section", &lightmap_section);

            entity = golf_entity_water(name, transform, geo, lightmap_section);

            valid_entity = true;
        }
        entity.parent_idx = parent_idx;

        if (valid_entity) {
            vec_push(&level->entities, entity);
        }
        else {
            golf_log_warning("Invalid entity. type: %s, name: %s", type, name);
        }
    }

    json_value_free(json_val);
    return true;
}

static bool _golf_level_unload(void *ptr) {
    golf_level_t *level = (golf_level_t*) ptr;

    for (int i = 0; i < level->deps.length; i++) {
        golf_data_unload(level->deps.data[i].path);
    }

    for (int i = 0; i < level->lightmap_images.length; i++) {
        golf_lightmap_image_t *lightmap_image = &level->lightmap_images.data[i];
        for (int i = 0; i < lightmap_image->num_samples; i++) {
            sg_destroy_image(lightmap_image->sg_image[i]);
            free(lightmap_image->data[i]);
        }
        golf_free(lightmap_image->data);
        golf_free(lightmap_image->sg_image);
    }

    for (int i = 0; i < level->entities.length; i++) {
        golf_entity_t *entity = &level->entities.data[i];

        golf_lightmap_section_t *lightmap_section = golf_entity_get_lightmap_section(entity);
        if (lightmap_section) {
            vec_deinit(&lightmap_section->uvs);
        }

        golf_geo_t *geo = golf_entity_get_geo(entity);
        if (geo) {
            for (int i = 0; i < geo->faces.length; i++) {
                golf_geo_face_t *face = &geo->faces.data[i];
                vec_deinit(&face->idx);
                vec_deinit(&face->uvs);
            }

            vec_deinit(&geo->points);
            vec_deinit(&geo->faces);
            vec_deinit(&geo->generator_data.args);

            vec_deinit(&geo->model.groups);
            vec_deinit(&geo->model.positions);
            vec_deinit(&geo->model.normals);
            vec_deinit(&geo->model.texcoords);
            if (geo->model.is_water) {
                vec_deinit(&geo->model.water_dir);
            }

            if (geo->model.sg_size > 0) {
                sg_destroy_buffer(geo->model.sg_positions_buf);
                sg_destroy_buffer(geo->model.sg_normals_buf);
                sg_destroy_buffer(geo->model.sg_texcoords_buf);
            }
        }
    }

    vec_deinit(&level->materials);
    vec_deinit(&level->lightmap_images);
    vec_deinit(&level->entities);
    vec_deinit(&level->deps);

    return true;
}

static bool _golf_static_data_load(void *ptr, const char *path, char *data, int data_len, char *meta_data, int meta_data_len) {
    GOLF_UNUSED(path);
    GOLF_UNUSED(data_len);
    GOLF_UNUSED(meta_data);
    GOLF_UNUSED(meta_data_len);

    golf_static_data_t *static_data = (golf_static_data_t*) ptr;
    JSON_Value *val = json_parse_string(data);
    JSON_Array *arr = json_value_get_array(val);

    vec_init(&static_data->data_paths, "data");
    for (int i = 0; i < (int)json_array_get_count(arr); i++) {
        const char *data_path = json_array_get_string(arr, i);
        if (data_path) {
            char *data_path_copy = golf_alloc(strlen(data_path) + 1);
            strcpy(data_path_copy, data_path);
            vec_push(&static_data->data_paths, data_path_copy);
            _golf_data_thread_load_file(golf_file(data_path_copy));
        }
    }

    while (true) {
        bool all_loaded = true;
        for (int i = 0; i < static_data->data_paths.length; i++) {
            if (golf_data_get_load_state(static_data->data_paths.data[i]) != GOLF_DATA_LOADED) {
                all_loaded = false;
            }
        }
        if (all_loaded) break;
        golf_thread_timer_wait(&_data_thread_timer, 10000000);
    }

    json_value_free(val);

    return true;
}

static bool _golf_static_data_unload(void *ptr) {
    GOLF_UNUSED(ptr);
    /*
    golf_static_data_t *static_data = (golf_static_data_t*) ptr;
    for (int i = 0; i < static_data->data_paths.length; i++) {
        char *data_path = static_data->data_paths.data[i];
        golf_data_unload(data_path);
        golf_free(data_path);
    }
    vec_deinit(&static_data->data_paths);
    */
    return true;
}

static bool _golf_script_data_load(void *ptr, const char *path, char *data, int data_len, char *meta_data, int meta_data_len) {
    GOLF_UNUSED(meta_data);
    GOLF_UNUSED(meta_data_len);

    golf_script_t *script = (golf_script_t*) ptr;
    bool r = golf_script_load(script, path, data, data_len);
    if (script->error) {
        golf_log_warning("Error loading script: %s", script->error);
    }
    return r;
}

static bool _golf_script_data_unload(void *ptr) {
    golf_script_t *script = (golf_script_t*) ptr;
    return golf_script_unload(script);
}

//
// DATA
//

static _file_event_t _file_event(_file_event_type type, golf_file_t file) {
    _file_event_t event;
    event.type = type;
    event.file = file;
    return event;
}

static assetsys_error_t _golf_assetsys_file_load(const char *path, char **data, int *data_len) {
    char assetsys_path[GOLF_FILE_MAX_PATH];
    snprintf(assetsys_path, GOLF_FILE_MAX_PATH, "/%s", path);

    golf_mutex_lock(&_assetsys_lock);
    assetsys_file_t asset_file;
    assetsys_file(_assetsys, assetsys_path, &asset_file);
    int size = assetsys_file_size(_assetsys, asset_file);
    *data = (char*) golf_alloc(size + 1);
    *data_len = 0;
    assetsys_error_t error = assetsys_file_load(_assetsys, asset_file, data_len, *data, size);
    (*data)[size] = 0;
    golf_mutex_unlock(&_assetsys_lock);
    return error;
}

typedef struct _data_loader {
    const char *ext;
    golf_data_type_t data_type;
    int data_size;
    bool (*finalize_fn)(void* ptr);
    bool (*load_fn)(void* ptr, const char *path, char *data, int data_len, char *meta_data, int meta_data_len);
    bool (*unload_fn)(void *ptr);
    bool (*import_fn)(const char *path, char *data, int data_len);
    bool reload_on;
} _data_loader_t;

static _data_loader_t _loaders[] = {
    {
        .ext = ".gif",
        .data_type = GOLF_DATA_GIF_TEXTURE,
        .data_size = sizeof(golf_gif_texture_t),
        .finalize_fn = _golf_gif_texture_finalize,
        .load_fn = _golf_gif_texture_load,
        .unload_fn = _golf_gif_texture_unload,
        .import_fn = NULL,
        .reload_on = true,
    },
    {
        .ext = ".png",
        .data_type = GOLF_DATA_TEXTURE,
        .data_size = sizeof(golf_texture_t),
        .finalize_fn = _golf_texture_finalize,
        .load_fn = _golf_texture_load,
        .unload_fn = _golf_texture_unload,
        .import_fn = NULL,
        .reload_on = true,
    },
    {
        .ext = ".jpg",
        .data_type = GOLF_DATA_TEXTURE,
        .data_size = sizeof(golf_texture_t),
        .finalize_fn = _golf_texture_finalize,
        .load_fn = _golf_texture_load,
        .unload_fn = _golf_texture_unload,
        .import_fn = NULL,
        .reload_on = true,
    },
    {
        .data_type = GOLF_DATA_TEXTURE,
        .ext = ".bmp",
        .data_size = sizeof(golf_texture_t),
        .finalize_fn = _golf_texture_finalize,
        .load_fn = _golf_texture_load,
        .unload_fn = _golf_texture_unload,
        .import_fn = NULL,
        .reload_on = true,
    },
    {
        .ext = ".glsl",
        .data_type = GOLF_DATA_SHADER,
        .data_size = sizeof(golf_shader_t),
        .finalize_fn = _golf_shader_finalize,
        .load_fn = _golf_shader_load,
        .unload_fn = _golf_shader_unload,
        .import_fn = _golf_shader_import,
        .reload_on = true,
    },
    {
        .ext = ".ttf",
        .data_type = GOLF_DATA_FONT,
        .data_size = sizeof(golf_font_t),
        .finalize_fn = _golf_font_finalize,
        .load_fn = _golf_font_load,
        .unload_fn = _golf_font_unload,
        .import_fn = _golf_font_import,
        .reload_on = true,
    },
    {
        .ext = ".obj",
        .data_type = GOLF_DATA_MODEL,
        .data_size = sizeof(golf_model_t),
        .finalize_fn = _golf_model_finalize,
        .load_fn = _golf_model_load,
        .unload_fn = _golf_model_unload,
        //.import_fn = _golf_model_import,
        .reload_on = true,
    },
    {
        .ext = ".level",
        .data_type = GOLF_DATA_LEVEL,
        .data_size = sizeof(golf_level_t),
        .finalize_fn = _golf_level_finalize,
        .load_fn = _golf_level_load,
        .unload_fn = _golf_level_unload,
        .import_fn = NULL,
        .reload_on = true,
    },
    {
        .ext = ".static_data",
        .data_type = GOLF_DATA_STATIC_DATA,
        .data_size = sizeof(golf_static_data_t),
        .finalize_fn = NULL,
        .load_fn = _golf_static_data_load,
        .unload_fn = _golf_static_data_unload,
        .import_fn = NULL,
        .reload_on = true,
    },
    {
        .ext = ".gs",
        .data_type = GOLF_DATA_SCRIPT,
        .data_size = sizeof(golf_script_t),
        .finalize_fn = NULL,
        .load_fn = _golf_script_data_load,
        .unload_fn = _golf_script_data_unload,
        .import_fn = NULL,
        .reload_on = true,
    },
    {
        .ext = ".cfg",
        .data_type = GOLF_DATA_CONFIG,
        .data_size = sizeof(golf_config_t),
        .finalize_fn = NULL,
        .load_fn = _golf_config_load,
        .unload_fn = _golf_config_unload,
        .import_fn = NULL,
        .reload_on = true,
    },
    {
        .ext = ".pixel_pack",
        .data_type = GOLF_DATA_PIXEL_PACK,
        .data_size = sizeof(golf_pixel_pack_t),
        .finalize_fn = NULL,
        .load_fn = _golf_pixel_pack_load,
        .unload_fn = _golf_pixel_pack_unload,
        .import_fn = NULL,
        .reload_on = true,
    },
    {
        .ext = ".ui",
        .data_type = GOLF_DATA_UI_LAYOUT,
        .data_size = sizeof(golf_ui_layout_t),
        .finalize_fn = NULL,
        .load_fn = _golf_ui_layout_load,
        .unload_fn = _golf_ui_layout_unload,
        .import_fn = NULL,
        .reload_on = true,
    },
};

static _data_loader_t *_get_data_loader(const char *ext) {
    for (int i = 0; i < (int) (sizeof(_loaders) / sizeof(_loaders[0])); i++) {
        _data_loader_t *loader = &_loaders[i];
        if (strcmp(loader->ext, ext) == 0) {
            return loader;
        }
    }
    return NULL;
}

static int _file_alpha_cmp(const void *a, const void *b) {
    const golf_file_t *file_a = (const golf_file_t*)a;
    const golf_file_t *file_b = (const golf_file_t*)b;
    return strcmp(file_a->path, file_b->path);
}

static void _golf_data_handle_file(const char *file_path, void *udata) {
    bool push_events = *((bool*)udata);

    golf_file_t file = golf_file(file_path);
    uint64_t file_time = golf_file_get_time(file_path);
    if (strcmp(file.ext, ".golf_data") == 0) {
        return;
    }

    _data_loader_t *loader = _get_data_loader(file.ext);
    if (loader && loader->import_fn) {
        golf_file_t import_file = golf_file_append_extension(file_path, ".golf_data");
        uint64_t import_file_time = golf_file_get_time(import_file.path);

        if (import_file_time < file_time) {
            char *data;
            int data_len;
            if (golf_file_load_data(file_path, &data, &data_len)) {
                golf_log_note("Importing %s\n", file_path);
                loader->import_fn(file_path, data, data_len);
                golf_free(data);
            }
        }
    }

    uint64_t *last_file_time = map_get(&_file_time_map, file_path);
    if (!last_file_time) {
        map_set(&_file_time_map, file_path, file_time);

        golf_file_t file = golf_file(file_path);
        golf_mutex_lock(&_seen_files_lock);
        vec_push(&_seen_files, file);
        golf_mutex_unlock(&_seen_files_lock);
        qsort(_seen_files.data, _seen_files.length, sizeof(golf_file_t), _file_alpha_cmp);

        _file_event_type event_type = FILE_CREATED;
        // Consider updates to .golf_meta files to be updates to the actual data file
        if (strcmp(file.ext, ".golf_meta") == 0) {
            char actual_file_path[GOLF_FILE_MAX_PATH];
            strcpy(actual_file_path, file.path);
            int len = (int)strlen(actual_file_path);
            actual_file_path[len - strlen(".golf_meta")] = 0;
            file = golf_file(actual_file_path);
            event_type = FILE_UPDATED;
        }

        if (push_events) {
            golf_mutex_lock(&_file_events_lock);
            vec_push(&_file_events, _file_event(event_type, file));
            golf_mutex_unlock(&_file_events_lock);
        }
    }
    else if (*last_file_time < file_time) {
        map_set(&_file_time_map, file_path, file_time);

        golf_file_t file = golf_file(file_path);
        // Consider updates to .golf_meta files to be updates to the actual data file
        if (strcmp(file.ext, ".golf_meta") == 0) {
            char actual_file_path[GOLF_FILE_MAX_PATH];
            strcpy(actual_file_path, file.path);
            int len = (int)strlen(actual_file_path);
            actual_file_path[len - strlen(".golf_meta")] = 0;
            file = golf_file(actual_file_path);
        }

        if (push_events) {
            golf_mutex_lock(&_file_events_lock);
            vec_push(&_file_events, _file_event(FILE_UPDATED, file));
            golf_mutex_unlock(&_file_events_lock);
        }
    }
}

static void _golf_data_thread_load_file(golf_file_t file) {
    {
        golf_mutex_lock(&_loaded_data_lock);
        bool done = false;
        golf_data_t *loaded_data = map_get(&_loaded_data, file.path);
        if (loaded_data) {
            loaded_data->load_count++;
            golf_log_note("Loading file %s, count: %d", file.path, loaded_data->load_count);
            done = true;
        }
        golf_mutex_unlock(&_loaded_data_lock);
        if (done) {
            return;
        }
    }
    golf_log_note("Loading file %s, count: 1", file.path);

    _data_loader_t *loader = _get_data_loader(file.ext);
    if (!loader) {
        golf_log_warning("No loader for file %s", file.path);
        return;
    }

    golf_file_t file_to_load = file;
    if (loader->import_fn) {
        file_to_load = golf_file_append_extension(file.path, ".golf_data");
    }

    char *data = NULL;
    int data_len = 0;
    assetsys_error_t error = _golf_assetsys_file_load(file_to_load.path, &data, &data_len);
    if (error == ASSETSYS_SUCCESS) {
        golf_file_t meta_file = golf_file_append_extension(file.path, ".golf_meta");
        char *meta_data;
        int meta_data_len;
        _golf_assetsys_file_load(meta_file.path, &meta_data, &meta_data_len);

        golf_data_t golf_data;
        golf_data.load_count = 1;
        golf_data.file = file_to_load;
        golf_data.type = loader->data_type;
        golf_data.ptr = golf_alloc(loader->data_size);
        golf_data.is_loaded = false;

        golf_mutex_lock(&_loaded_data_lock);
        map_set(&_loaded_data, file.path, golf_data);
        golf_mutex_unlock(&_loaded_data_lock);

        loader->load_fn(golf_data.ptr, file.path, data, data_len, meta_data, meta_data_len);

        golf_mutex_lock(&_file_events_lock);
        vec_push(&_file_events, _file_event(FILE_LOADED, file));
        golf_mutex_unlock(&_file_events_lock);

        golf_free(meta_data);
    }
    else {
        golf_log_warning("Assetys unable to load file %s", file_to_load.path);
    }
    golf_free(data);
}

static golf_thread_result_t _golf_data_thread_fn(void *udata) {
    GOLF_UNUSED(udata);

    vec_golf_file_t files_to_load;
    vec_init(&files_to_load, "data_thread");
    uint64_t last_run_time = stm_now();

    while (true) {
        files_to_load.length = 0;

        golf_mutex_lock(&_files_to_load_lock);
        for (int i = 0; i < _files_to_load.length; i++) {
            vec_push(&files_to_load, _files_to_load.data[i]);
        }
        _files_to_load.length = 0;
        golf_mutex_unlock(&_files_to_load_lock);

        for (int i = 0; i < files_to_load.length; i++) {
            _golf_data_thread_load_file(files_to_load.data[i]);
        }

        golf_thread_timer_wait(&_data_thread_timer, 10000000);

        double time_since_last_run = stm_sec(stm_since(last_run_time));
        if (time_since_last_run > 1) {
            bool push_events = true;
            golf_dir_recurse("data", _golf_data_handle_file, &push_events); 
            last_run_time = stm_now();
        }
    }
    return GOLF_THREAD_RESULT_SUCCESS;
}

void golf_data_turn_off_reload(const char *ext) {
    for (int i = 0; i < (int) (sizeof(_loaders) / sizeof(_loaders[0])); i++) {
        _data_loader_t *loader = &_loaders[i];
        if (strcmp(loader->ext, ext) == 0) {
            loader->reload_on = false;
            return;
        }
    }
}

void golf_data_init(void) {
    golf_thread_timer_init(&_main_thread_timer);
    golf_thread_timer_init(&_data_thread_timer);
    golf_mutex_init(&_loaded_data_lock);
    map_init(&_loaded_data, "data");
    golf_mutex_init(&_files_to_load_lock);
    vec_init(&_files_to_load, "data");
    golf_mutex_init(&_seen_files_lock);
    vec_init(&_seen_files, "data");
    golf_mutex_init(&_file_events_lock);
    vec_init(&_file_events, "data");
    golf_mutex_init(&_assetsys_lock);
    _assetsys = assetsys_create(NULL);
    map_init(&_file_time_map, "data");
#if GOLF_PLATFORM_IOS | GOLF_PLATFORM_ANDROID
    assetsys_error_t error = assetsys_mount(_assetsys, "data.zip", golf_data_zip, sizeof(golf_data_zip), "/data");
#else
    assetsys_error_t error = assetsys_mount(_assetsys, "data", NULL, 0, "/data");
#endif
    if (error != ASSETSYS_SUCCESS) {
        golf_log_error("Unable to mount data, error: %d", (int)error);
    }

    bool push_events = false;
    golf_dir_recurse("data", _golf_data_handle_file, &push_events); 
    qsort(_seen_files.data, _seen_files.length, sizeof(golf_file_t), _file_alpha_cmp);
    golf_thread_create(_golf_data_thread_fn, NULL, "_golf_data_thread_fn");
}

void golf_data_update(float dt) {
    GOLF_UNUSED(dt);

    golf_mutex_lock(&_file_events_lock);  
    for (int i = 0; i < _file_events.length; i++) {
        _file_event_t event = _file_events.data[i];
        switch (event.type) {
            case FILE_CREATED: {
                golf_mutex_lock(&_assetsys_lock);
                assetsys_dismount(_assetsys, "data", "/data");
                assetsys_error_t error = assetsys_mount(_assetsys, "data", NULL, 0, "/data");
                golf_mutex_unlock(&_assetsys_lock);
                if (error != ASSETSYS_SUCCESS) {
                    golf_log_error("Unable to mount data");
                }
                break;
            }
            case FILE_UPDATED: {
                void *ptr;

                _data_loader_t *loader = _get_data_loader(event.file.ext);
                golf_mutex_lock(&_loaded_data_lock);
                golf_data_t *data = map_get(&_loaded_data, event.file.path);
                if (data) {
                    ptr = data->ptr;
                }
                else {
                    ptr = NULL;
                }
                golf_mutex_unlock(&_loaded_data_lock);
                if (ptr && loader && loader->reload_on) {
                    golf_log_note("Reloading %s", event.file.path);

                    golf_file_t file_to_load = event.file;
                    if (loader->import_fn) {
                        file_to_load = golf_file_append_extension(file_to_load.path, ".golf_data");
                    }

                    loader->unload_fn(ptr);
                    char *bytes = NULL;
                    int bytes_len = 0;
                    assetsys_error_t error = _golf_assetsys_file_load(file_to_load.path, &bytes, &bytes_len);
                    if (error == ASSETSYS_SUCCESS) {
                        golf_file_t meta_file = golf_file_append_extension(event.file.path, ".golf_meta");
                        char *meta_data;
                        int meta_data_len;
                        _golf_assetsys_file_load(meta_file.path, &meta_data, &meta_data_len);

                        loader->load_fn(ptr, event.file.path, bytes, bytes_len, meta_data, meta_data_len);
                        if (loader->finalize_fn) {
                            loader->finalize_fn(ptr);
                        }

                        golf_free(meta_data);
                    }
                    else {
                        golf_log_warning("Assetys unable to load file %s", file_to_load.path);
                    }
                    golf_free(bytes);
                }
                break;
            }
            case FILE_LOADED: {
                golf_data_t *data;

                golf_mutex_lock(&_loaded_data_lock);
                data = map_get(&_loaded_data, event.file.path);
                void *ptr = data->ptr;
                golf_mutex_unlock(&_loaded_data_lock);

                _data_loader_t *loader = _get_data_loader(event.file.ext);
                if (loader->finalize_fn) {
                    loader->finalize_fn(ptr);
                }

                golf_mutex_lock(&_loaded_data_lock);
                data = map_get(&_loaded_data, event.file.path);
                data->is_loaded = true;
                golf_mutex_unlock(&_loaded_data_lock);
                break;
            }
        }
    }
    _file_events.length = 0;
    golf_mutex_unlock(&_file_events_lock);  
}

void golf_data_load(const char *path, bool load_async) {
    golf_mutex_lock(&_files_to_load_lock);
    vec_push(&_files_to_load, golf_file(path));
    golf_mutex_unlock(&_files_to_load_lock);

    if (!load_async) {
        while (golf_data_get_load_state(path) != GOLF_DATA_LOADED) {
            golf_data_update(0);
            golf_thread_timer_wait(&_main_thread_timer, 10000000);
        }
    }
}

golf_data_load_state_t golf_data_get_load_state(const char *path) {
    golf_mutex_lock(&_loaded_data_lock);
    golf_data_t *data = map_get(&_loaded_data, path);
    golf_data_load_state_t state;
    if (!data) {
        state = GOLF_DATA_UNLOADED;
    }
    else if (!data->is_loaded) {
        state = GOLF_DATA_LOADING;
    }
    else {
        state = GOLF_DATA_LOADED;
    }
    golf_mutex_unlock(&_loaded_data_lock);
    return state;
}

void golf_data_unload(const char *path) {
    golf_log_note("Unloading file %s", path);

    golf_data_t *golf_data = map_get(&_loaded_data, path); 
    if (!golf_data) {
        golf_log_warning("Unloading file %s that is not loaded", path);
        return;
    }

    golf_data->load_count--;
    if (golf_data->load_count == 0) {
        golf_file_t file = golf_file(path);
        _data_loader_t *loader = _get_data_loader(file.ext);
        if (!loader) {
            golf_log_warning("Unable to unload file %s", path);
            return;
        }

        loader->unload_fn(golf_data->ptr);
        golf_free(golf_data->ptr);
        map_remove(&_loaded_data, path);
    }
}

static void *_golf_data_get_ptr(const char *path, golf_data_type_t type) {
    void *ptr = NULL;
    golf_mutex_lock(&_loaded_data_lock);
    golf_data_t *data_file = map_get(&_loaded_data, path);
    if (!data_file || !data_file->is_loaded || data_file->type != type) {
        ptr = NULL;
    }
    else {
        ptr = data_file->ptr;
    }
    golf_mutex_unlock(&_loaded_data_lock);
    return ptr;
}

void *golf_data_get_ptr(const char *path, golf_data_type_t type) {
    return _golf_data_get_ptr(path, type);
}

golf_gif_texture_t *golf_data_get_gif_texture(const char *path) {
    golf_gif_texture_t *texture = _golf_data_get_ptr(path, GOLF_DATA_GIF_TEXTURE);
    if (!texture) {
        golf_log_error("Could not find gif texture %s", path);
    }
    return texture;
}

golf_texture_t *golf_data_get_texture(const char *path) {
    static const char *fallback = "data/textures/fallback.png";

    golf_texture_t *texture = _golf_data_get_ptr(path, GOLF_DATA_TEXTURE);
    if (!texture) {
        texture = _golf_data_get_ptr(fallback, GOLF_DATA_TEXTURE);
        if (!texture) {
            golf_log_error("Could not find fallback texture");
        }
    }
    return texture;
}

golf_pixel_pack_t *golf_data_get_pixel_pack(const char *path) {
    static const char *fallback = "data/textures/pixel_pack.pixel_pack";

    golf_pixel_pack_t *pixel_pack = _golf_data_get_ptr(path, GOLF_DATA_PIXEL_PACK);
    if (!pixel_pack) {
        golf_log_warning("Could not find pixel pack %s", path);
        pixel_pack = _golf_data_get_ptr(fallback, GOLF_DATA_PIXEL_PACK);
        if (!pixel_pack) {
            golf_log_error("Could not find fallback pixel pack");
        }
    }
    return pixel_pack;
}

golf_model_t *golf_data_get_model(const char *path) {
    static const char *fallback = "data/models/cube.obj";

    golf_model_t *model = _golf_data_get_ptr(path, GOLF_DATA_MODEL);
    if (!model) {
        golf_log_warning("Could not find model %s", path);
        model = _golf_data_get_ptr(fallback, GOLF_DATA_MODEL);
        if (!model) {
            golf_log_error("Could not find fallback model");
        }
    }
    return model;
}

golf_shader_t *golf_data_get_shader(const char *path) {
    golf_shader_t *shader = _golf_data_get_ptr(path, GOLF_DATA_SHADER);
    if (!shader) {
        golf_log_error("Could not find shader %s", path);
    }
    return shader;
}

golf_font_t *golf_data_get_font(const char *path) {
    static const char *fallback = "data/font/DroidSerif-Bold.ttf";

    golf_font_t *font = _golf_data_get_ptr(path, GOLF_DATA_FONT);
    if (!font) {
        font = _golf_data_get_ptr(fallback, GOLF_DATA_FONT);
        if (!font) {
            golf_log_error("Could not find fallback font");
        }
    }
    return font;
}

golf_config_t *golf_data_get_config(const char *path) {
    golf_config_t *config = _golf_data_get_ptr(path, GOLF_DATA_CONFIG);
    if (!config) {
        golf_log_error("Could not find config %s", path);
    }
    return config;
}

golf_level_t *golf_data_get_level(const char *path) {
    golf_level_t *level = _golf_data_get_ptr(path, GOLF_DATA_LEVEL);
    if (!level) {
        golf_log_error("Could not find level %s", path);
    }
    return level;
}

golf_script_t *golf_data_get_script(const char *path) {
    golf_script_t *script = _golf_data_get_ptr(path, GOLF_DATA_SCRIPT);
    if (!script) {
        golf_log_error("Could not find script %s", path);
    }
    return script;
}

golf_ui_layout_t *golf_data_get_ui_layout(const char *path) {
    golf_ui_layout_t *ui_layout = _golf_data_get_ptr(path, GOLF_DATA_UI_LAYOUT);
    if (!ui_layout) {
        golf_log_error("Could not find ui_layout %s", path);
    }
    return ui_layout;
}

void golf_data_get_all_matching(golf_data_type_t type, const char *str, vec_golf_file_t *files) {
    golf_mutex_lock(&_seen_files_lock);
    for (int i = 0; i < _seen_files.length; i++) {
        golf_file_t file = _seen_files.data[i];
        _data_loader_t *loader = _get_data_loader(file.ext);
        if (loader && loader->data_type == type) {
            if (strstr(file.path, str)) {
                vec_push(files, file);
            }
        }
    }
    golf_mutex_unlock(&_seen_files_lock);
}

void golf_data_force_remount(void) {
    golf_mutex_lock(&_assetsys_lock);
    assetsys_dismount(_assetsys, "data", "/data");
    assetsys_mount(_assetsys, "data", NULL, 0, "/data");
    golf_mutex_unlock(&_assetsys_lock);
}

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"

void golf_data_debug_console_tab(void) {
    /*
    if (igCollapsingHeader_TreeNodeFlags("Textures", ImGuiTreeNodeFlags_None)) {
        const char *key;
        map_iter_t iter = map_iter(&_loaded_data);

        while ((key = map_next(&_loaded_data, &iter))) {
            golf_data_t *loaded_file = map_get(&_loaded_data, key);
            if (loaded_file->type != GOLF_DATA_TEXTURE) {
                continue;
            }

            golf_texture_t *texture = loaded_file->ptr;
            if (igTreeNode_Str(key)) {
                igText("Width: %d", texture->width);
                igText("Height: %d", texture->height);
                igImage((ImTextureID)(intptr_t)texture->sg_image.id, (ImVec2){(float)texture->width, (float)texture->height}, (ImVec2){0, 0}, (ImVec2){1, 1}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 1});
                igTreePop();
            }
        }
    }

    if (igCollapsingHeader_TreeNodeFlags("Fonts", ImGuiTreeNodeFlags_None)) {
        const char *key;
        map_iter_t iter = map_iter(&_loaded_data);

        while ((key = map_next(&_loaded_data, &iter))) {
            golf_data_t *loaded_file = map_get(&_loaded_data, key);
            if (loaded_file->type != GOLF_DATA_FONT) {
                continue;
            }

            golf_font_t *font = loaded_file->ptr;
            if (igTreeNode_Str(key)) {
                for (int i = 0; i < font->atlases.length; i++) {
                    golf_font_atlas_t *atlas = &font->atlases.data[i];
                    igText("Font Size: %f", atlas->font_size);
                    igImage((ImTextureID)(intptr_t)atlas->sg_image.id, (ImVec2){(float)atlas->size, (float)atlas->size}, (ImVec2){0, 0}, (ImVec2){1, 1}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 1});
                }
                igTreePop();
            }
        }
    }

    if (igCollapsingHeader_TreeNodeFlags("Models", ImGuiTreeNodeFlags_None)) {
        const char *key;
        map_iter_t iter = map_iter(&_loaded_data);

        while ((key = map_next(&_loaded_data, &iter))) {
            golf_data_t *loaded_file = map_get(&_loaded_data, key);
            if (loaded_file->type != GOLF_DATA_MODEL) {
                continue;
            }

            golf_model_t *model = loaded_file->ptr;
            if (igTreeNode_Str(key)) {
                if (igTreeNode_Str("Positions: ")) {
                    for (int i = 0; i < model->positions.length; i++) {
                        vec3 position = model->positions.data[i];
                        igText("<%.3f, %.3f, %.3f>", position.x, position.y, position.z);
                    }
                    igTreePop();
                }
                if (igTreeNode_Str("Normals: ")) {
                    for (int i = 0; i < model->normals.length; i++) {
                        vec3 normal = model->normals.data[i];
                        igText("<%.3f, %.3f, %.3f>", normal.x, normal.y, normal.z);
                    }
                    igTreePop();
                }
                if (igTreeNode_Str("Texcoords: ")) {
                    for (int i = 0; i < model->texcoords.length; i++) {
                        vec2 texcoord = model->texcoords.data[i];
                        igText("<%.3f, %.3f>", texcoord.x, texcoord.y);
                    }
                    igTreePop();
                }
                igTreePop();
            }
        }
    }

    if (igCollapsingHeader_TreeNodeFlags("Shaders", ImGuiTreeNodeFlags_None)) {
        const char *key;
        map_iter_t iter = map_iter(&_loaded_data);

        while ((key = map_next(&_loaded_data, &iter))) {
            golf_data_t *loaded_file = map_get(&_loaded_data, key);
            if (loaded_file->type != GOLF_DATA_SHADER) {
                continue;
            }
            if (igTreeNode_Str(key)) {
                igTreePop();
            }
        }
    }

    if (igCollapsingHeader_TreeNodeFlags("Pixel Packs", ImGuiTreeNodeFlags_None)) {
        const char *key;
        map_iter_t iter = map_iter(&_loaded_data);

        while ((key = map_next(&_loaded_data, &iter))) {
            golf_data_t *loaded_file = map_get(&_loaded_data, key);
            if (loaded_file->type != GOLF_DATA_PIXEL_PACK) {
                continue;
            }

            golf_pixel_pack_t *pp = loaded_file->ptr;
            golf_texture_t *t = pp->texture;

            if (igTreeNode_Str(key)) {
                if (igTreeNode_Str("Icons")) {
                    const char *icon_key;
                    map_iter_t icons_iter = map_iter(&pp->icons);
                    while ((icon_key = map_next(&pp->icons, &icons_iter))) {
                        if (igTreeNode_Str(icon_key)) {
                            golf_pixel_pack_icon_t *i = map_get(&pp->icons, icon_key);
                            igImage((ImTextureID)(intptr_t)t->sg_image.id, (ImVec2){40, 40}, (ImVec2){i->uv0.x, i->uv0.y}, (ImVec2){i->uv1.x, i->uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 1});
                            igTreePop();
                        }
                    }
                    igTreePop();
                }

                if (igTreeNode_Str("Squares")) {
                    const char *square_key;
                    map_iter_t squares_iter = map_iter(&pp->squares);
                    while ((square_key = map_next(&pp->squares, &squares_iter))) {
                        if (igTreeNode_Str(square_key)) {
                            golf_pixel_pack_square_t *s = map_get(&pp->squares, square_key);

                            igPushStyleVar_Vec2(ImGuiStyleVar_ItemSpacing, (ImVec2){0, 0});

                            igImage((ImTextureID)(intptr_t)t->sg_image.id, (ImVec2){40, 40}, (ImVec2){s->tl_uv0.x, s->tl_uv0.y}, (ImVec2){s->tl_uv1.x, s->tl_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
                            igSameLine(0, 0);
                            igImage((ImTextureID)(intptr_t)t->sg_image.id, (ImVec2){40, 40}, (ImVec2){s->tm_uv0.x, s->tm_uv0.y}, (ImVec2){s->tm_uv1.x, s->tm_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
                            igSameLine(0, 0);
                            igImage((ImTextureID)(intptr_t)t->sg_image.id, (ImVec2){40, 40}, (ImVec2){s->tr_uv0.x, s->tr_uv0.y}, (ImVec2){s->tr_uv1.x, s->tr_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});

                            igImage((ImTextureID)(intptr_t)t->sg_image.id, (ImVec2){40, 40}, (ImVec2){s->ml_uv0.x, s->ml_uv0.y}, (ImVec2){s->ml_uv1.x, s->ml_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
                            igSameLine(0, 0);
                            igImage((ImTextureID)(intptr_t)t->sg_image.id, (ImVec2){40, 40}, (ImVec2){s->mm_uv0.x, s->mm_uv0.y}, (ImVec2){s->mm_uv1.x, s->mm_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
                            igSameLine(0, 0);
                            igImage((ImTextureID)(intptr_t)t->sg_image.id, (ImVec2){40, 40}, (ImVec2){s->mr_uv0.x, s->mr_uv0.y}, (ImVec2){s->mr_uv1.x, s->mr_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});

                            igImage((ImTextureID)(intptr_t)t->sg_image.id, (ImVec2){40, 40}, (ImVec2){s->bl_uv0.x, s->bl_uv0.y}, (ImVec2){s->bl_uv1.x, s->bl_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
                            igSameLine(0, 0);
                            igImage((ImTextureID)(intptr_t)t->sg_image.id, (ImVec2){40, 40}, (ImVec2){s->bm_uv0.x, s->bm_uv0.y}, (ImVec2){s->bm_uv1.x, s->bm_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
                            igSameLine(0, 0);
                            igImage((ImTextureID)(intptr_t)t->sg_image.id, (ImVec2){40, 40}, (ImVec2){s->br_uv0.x, s->br_uv0.y}, (ImVec2){s->br_uv1.x, s->br_uv1.y}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});

                            igPopStyleVar(1);
                            igTreePop();
                        }
                    }
                    igTreePop();
                }

                igTreePop();
            }
        }
    }
    */
}
