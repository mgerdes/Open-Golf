#if defined(_WIN32)
#define _CRT_NO_SECURE_WARNINGS

#include "lightmaps.h"

#include <windows.h>

#include "array.h"
#include "hole.h"
#include "glad.h"
#include "GLFW/glfw3.h"
#include "maths.h"
#include "xatlas_wrapper.h"

void lightmap_generator_data_init(struct hole *hole, struct lightmap_generator_data *data, 
        bool reset_lightmaps, bool create_uvs, float gamma,
        int num_iterations, int num_dilates, int num_smooths) {
    data->reset_lightmaps = reset_lightmaps;
    data->create_uvs = create_uvs;
    data->gamma = gamma;
    data->num_iterations = num_iterations;
    data->num_dilates = num_dilates;
    data->num_smooths = num_smooths;

    array_init(&data->entities);
    for (int i = 0; i < hole->terrain_entities.length; i++) {
        struct terrain_entity *entity = &hole->terrain_entities.data[i];
        struct terrain_model *model = &entity->terrain_model;
        struct lightmap *lightmap = &entity->lightmap;

        struct lightmap_entity lightmap_entity;
        array_init(&lightmap_entity.positions);
        array_init(&lightmap_entity.normals);

        {
            struct array_vec2 texture_coords;
            struct array_float material_idxs;
            array_init(&texture_coords);
            array_init(&material_idxs);
            terrain_model_generate_triangle_data(model, &lightmap_entity.positions, &lightmap_entity.normals,
                    &texture_coords, &material_idxs);
            array_deinit(&texture_coords);
            array_deinit(&material_idxs);
        }

        array_init(&lightmap_entity.lightmap_uvs);
        for (int i = 0; i < lightmap->uvs.length; i++) {
            array_push(&lightmap_entity.lightmap_uvs, lightmap->uvs.data[i]);
        }
        lightmap_entity.model_mat = terrain_entity_get_transform(entity);
        lightmap_entity.lightmap_width = lightmap->width;
        lightmap_entity.lightmap_height = lightmap->height;
        lightmap_entity.lightmap_data = malloc(sizeof(float) * lightmap->width * lightmap->height);
        for (int j = 0; j < lightmap->width * lightmap->height; j++) {
            lightmap_entity.lightmap_data[j] = lightmap->images.data[0].data[j] / 255.0f;
        }
        lightmap_entity.lightmap = lightmap;

        array_push(&data->entities, lightmap_entity);
    }

    {
        struct lightmap_entity lightmap_entity;
        array_init(&lightmap_entity.positions);
        array_init(&lightmap_entity.normals);
        array_init(&lightmap_entity.lightmap_uvs);

        for (int i = 0; i < hole->environment_entities.length; i++) {
            struct environment_entity *entity = &hole->environment_entities.data[i];
            struct model *model = entity->model;

            mat4 model_mat = environment_entity_get_transform(entity);
            mat4 normal_model_mat = mat4_transpose(mat4_inverse(model_mat));
            for (int i = 0; i < model->num_points; i++) {
                vec3 position = vec3_apply_mat4(model->positions[i], 1.0f, model_mat);
                vec3 normal = vec3_apply_mat4(model->normals[i], 0.0f, normal_model_mat);
                array_push(&lightmap_entity.positions, position);
                array_push(&lightmap_entity.normals, normal);
            }
        }

        struct lightmap *lightmap = &hole->environment_lightmap;
        for (int i = 0; i < lightmap->uvs.length; i++) {
            array_push(&lightmap_entity.lightmap_uvs, lightmap->uvs.data[i]);
        }
        lightmap_entity.model_mat = mat4_identity();
        lightmap_entity.lightmap_width = lightmap->width;
        lightmap_entity.lightmap_height = lightmap->height;
        lightmap_entity.lightmap_data = malloc(sizeof(float) * lightmap->width * lightmap->height);
        for (int j = 0; j < lightmap->width * lightmap->height; j++) {
            lightmap_entity.lightmap_data[j] = lightmap->images.data[0].data[j] / 255.0f;
        }
        lightmap_entity.lightmap = lightmap;

        array_push(&data->entities, lightmap_entity);
    }

    array_init(&data->multi_entities);
    for (int i = 0; i < hole->multi_terrain_entities.length; i++) {
        struct multi_terrain_entity *multi_entity = &hole->multi_terrain_entities.data[i];
        struct terrain_model *static_model = &multi_entity->static_terrain_model;
        struct terrain_model *moving_model = &multi_entity->moving_terrain_model;
        struct lightmap *static_lightmap = &multi_entity->static_lightmap;
        struct lightmap *moving_lightmap = &multi_entity->moving_lightmap;

        struct lightmap_multi_entity lightmap_entity;

        {
            {
                array_init(&lightmap_entity.moving_positions);
                array_init(&lightmap_entity.moving_normals);
                struct array_vec2 texture_coords;
                struct array_float material_idxs;
                array_init(&texture_coords);
                array_init(&material_idxs);
                terrain_model_generate_triangle_data(moving_model, &lightmap_entity.moving_positions, 
                        &lightmap_entity.moving_normals, &texture_coords, &material_idxs);
                array_deinit(&texture_coords);
                array_deinit(&material_idxs);
            }

            array_init(&lightmap_entity.moving_lightmap_uvs);
            for (int i = 0; i < moving_lightmap->uvs.length; i++) {
                array_push(&lightmap_entity.moving_lightmap_uvs, moving_lightmap->uvs.data[i]);
            }

            array_init(&lightmap_entity.moving_lightmap_data);
            array_init(&lightmap_entity.moving_model_mats);
            for (int i = 0; i < moving_lightmap->images.length; i++) {
                float a = ((float) i) / (moving_lightmap->images.length - 1);
                float t = a*multi_entity->movement_data.length;
                if (multi_entity->movement_data.type == MOVEMENT_TYPE_PENDULUM ||
                        multi_entity->movement_data.type == MOVEMENT_TYPE_TO_AND_FROM ||
                        multi_entity->movement_data.type == MOVEMENT_TYPE_RAMP) {
                    // For these movement types the second half of the movement is just the first have but in reverse 
                    // So only calculate the lightmaps for the first half
                    t = 0.5f*t;
                }

                float *lightmap_data = malloc(sizeof(float) * moving_lightmap->width * moving_lightmap->height);
                for (int j = 0; j < moving_lightmap->width * moving_lightmap->height; j++) {
                    lightmap_data[j] = moving_lightmap->images.data[i].data[j] / 255.0f;
                }

                mat4 model_mat = multi_terrain_entity_get_moving_transform(multi_entity, t);

                array_push(&lightmap_entity.moving_model_mats, model_mat);
                array_push(&lightmap_entity.moving_lightmap_data, lightmap_data);
            }
            lightmap_entity.moving_lightmap_width = moving_lightmap->width;
            lightmap_entity.moving_lightmap_height = moving_lightmap->height;
            lightmap_entity.moving_lightmap = moving_lightmap;
        }

        {
            {
                array_init(&lightmap_entity.static_positions);
                array_init(&lightmap_entity.static_normals);
                struct array_vec2 texture_coords;
                struct array_float material_idxs;
                array_init(&texture_coords);
                array_init(&material_idxs);
                terrain_model_generate_triangle_data(static_model, &lightmap_entity.static_positions, 
                        &lightmap_entity.static_normals, &texture_coords, &material_idxs);
                array_deinit(&texture_coords);
                array_deinit(&material_idxs);
            }

            array_init(&lightmap_entity.static_lightmap_uvs);
            for (int i = 0; i < static_lightmap->uvs.length; i++) {
                array_push(&lightmap_entity.static_lightmap_uvs, static_lightmap->uvs.data[i]);
            }

            array_init(&lightmap_entity.static_lightmap_data);
            for (int i = 0; i < static_lightmap->images.length; i++) {
                float *lightmap_data = malloc(sizeof(float) * static_lightmap->width * static_lightmap->height);
                for (int j = 0; j < static_lightmap->width * static_lightmap->height; j++) {
                    lightmap_data[j] = static_lightmap->images.data[i].data[j] / 255.0f;
                }
                array_push(&lightmap_entity.static_lightmap_data, lightmap_data);
            }
            lightmap_entity.static_model_mat = multi_terrain_entity_get_static_transform(multi_entity);
            lightmap_entity.static_lightmap_width = static_lightmap->width;
            lightmap_entity.static_lightmap_height = static_lightmap->height;
            lightmap_entity.static_lightmap = static_lightmap;
        }

        array_push(&data->multi_entities, lightmap_entity);
    }
}

void lightmap_generator_data_deinit(struct lightmap_generator_data *data) {
    for (int i = 0; i < data->entities.length; i++) {
        struct lightmap_entity *entity = &data->entities.data[i];
        array_deinit(&entity->positions);
        array_deinit(&entity->normals);
        array_deinit(&entity->lightmap_uvs);
        free(entity->lightmap_data);
    }
    array_deinit(&data->entities);

    for (int i = 0; i < data->multi_entities.length; i++) {
        struct lightmap_multi_entity *entity = &data->multi_entities.data[i];
        array_deinit(&entity->moving_positions);
        array_deinit(&entity->moving_normals);
        array_deinit(&entity->moving_lightmap_uvs);
        array_deinit(&entity->static_positions);
        array_deinit(&entity->static_normals);
        array_deinit(&entity->static_lightmap_uvs);

        for (int i = 0; i < entity->moving_lightmap_data.length; i++) {
            free(entity->moving_lightmap_data.data[i]);
        }
        array_deinit(&entity->moving_lightmap_data);

        for (int i = 0; i < entity->static_lightmap_data.length; i++) {
            free(entity->static_lightmap_data.data[i]);
        }
        array_deinit(&entity->static_lightmap_data);
    }
    array_deinit(&data->multi_entities);
}

#define LIGHTMAPPER_IMPLEMENTATION
#include "lightmapper.h"

array_t(GLuint, array_GLuint)

static GLuint load_shader(GLenum type, const char *source) {
	GLuint shader = glCreateShader(type);
    assert(shader != 0);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	GLint compiled;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    assert(compiled);
	return shader;
}

static volatile long is_running, uv_gen_progress_idx, lm_gen_progress_idx, lm_gen_progress_pct;
long uv_gen_progress_count, lm_gen_progress_count;

static int lightmap_generator_run(void* user_data) {
    struct lightmap_generator_data *data = (struct lightmap_generator_data *) user_data;

    bool init = glfwInit();
    assert(init);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow *offscreen_context = glfwCreateWindow(1920, 1080, "", NULL, NULL);
    assert(offscreen_context);
    glfwMakeContextCurrent(offscreen_context);

    GLuint program;
    {
        const char *fs = 
            "#version 150 core\n"
            "precision highp float;"
            "in vec2 frag_texture_coord;"
            "uniform sampler2D ao_map;"
            "out vec4 g_fragment_color;"
            "void main() {"
            "    float ao = texture(ao_map, frag_texture_coord).r;"
            "    if (gl_FrontFacing) {"
            "        g_fragment_color = vec4(ao, ao, ao, 1.0);"
            "    }"
            "    else {"
            "        g_fragment_color = vec4(0.0, 0.0, 0.0, 0.0);"
            "    }"
            "}";
        const char *vs =
            "#version 150 core\n"
            "in vec3 position;"
            "in vec2 texture_coord;"
            "uniform mat4 mvp_mat;"
            "out vec2 frag_texture_coord;"
            "void main() {"
            "    frag_texture_coord = texture_coord;"
            "    gl_Position = mvp_mat * vec4(position, 1.0);"
            "}";
        program = glCreateProgram();
        GLuint vertex_shader = load_shader(GL_VERTEX_SHADER, vs);
        GLuint fragment_shader = load_shader(GL_FRAGMENT_SHADER, fs);
        glAttachShader(program, vertex_shader);
        glAttachShader(program, fragment_shader);
        glLinkProgram(program);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        GLint linked;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        assert(linked);
    }
    glUseProgram(program);

    GLint mvp_mat_id = glGetUniformLocation(program, "mvp_mat");
    assert(mvp_mat_id >= 0);

    GLint ao_map_id = glGetUniformLocation(program, "ao_map");
    assert(ao_map_id >= 0);

    GLint position_id = glGetAttribLocation(program, "position");
    assert(position_id >= 0);

    GLint texture_coord_id = glGetAttribLocation(program, "texture_coord");
    assert(texture_coord_id >= 0);

    GLuint dummy_vao;
    glGenVertexArrays(1, &dummy_vao);

    for (int i = 0; i < data->entities.length; i++) {
        struct lightmap_entity *entity = &data->entities.data[i];

        struct array_vec3 *positions = &entity->positions;
        struct array_vec2 *lightmap_uvs = &entity->lightmap_uvs;
        InterlockedIncrement(&uv_gen_progress_idx);
        if (data->create_uvs) {
            xatlas_wrapper_generate_lightmap_uvs(lightmap_uvs->data, positions->data, positions->length);
        }

        GLuint positions_vbo;
        glGenBuffers(1, &positions_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, positions_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * positions->length, positions->data, GL_STATIC_DRAW);

        GLuint lightmap_uvs_vbo;
        glGenBuffers(1, &lightmap_uvs_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, lightmap_uvs_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * lightmap_uvs->length, lightmap_uvs->data, GL_STATIC_DRAW);

        int lightmap_width = entity->lightmap_width;
        int lightmap_height = entity->lightmap_height;
        float *lightmap_data = entity->lightmap_data;
        if (data->reset_lightmaps) {
            for (int i = 0; i < lightmap_width * lightmap_height; i++) {
                lightmap_data[i] = 0.0f;
            }
        }

        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, lightmap_width, lightmap_height, 
                0, GL_RED, GL_FLOAT, lightmap_data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        entity->gl_position_vbo = positions_vbo;
        entity->gl_lightmap_uv_vbo = lightmap_uvs_vbo;
        entity->gl_tex = tex;
    }

    for (int i = 0; i < data->multi_entities.length; i++) {
        struct lightmap_multi_entity *entity = &data->multi_entities.data[i];

        {
            struct lightmap *moving_lightmap = entity->moving_lightmap;
            struct array_vec3 *moving_positions = &entity->moving_positions;
            struct array_vec2 *moving_lightmap_uvs = &entity->moving_lightmap_uvs;
            InterlockedIncrement(&uv_gen_progress_idx);
            if (data->create_uvs) {
                xatlas_wrapper_generate_lightmap_uvs(moving_lightmap_uvs->data, moving_positions->data, 
                        moving_positions->length);
            }

            GLuint moving_positions_vbo;
            glGenBuffers(1, &moving_positions_vbo);
            glBindBuffer(GL_ARRAY_BUFFER, moving_positions_vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * moving_positions->length, moving_positions->data, 
                    GL_STATIC_DRAW);

            GLuint moving_lightmap_uvs_vbo;
            glGenBuffers(1, &moving_lightmap_uvs_vbo);
            glBindBuffer(GL_ARRAY_BUFFER, moving_lightmap_uvs_vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * moving_lightmap_uvs->length, moving_lightmap_uvs->data, 
                    GL_STATIC_DRAW);

            array_init(&entity->gl_moving_tex);
            for (int i = 0; i < moving_lightmap->images.length; i++) {
                int lightmap_width = entity->moving_lightmap_width;
                int lightmap_height = entity->moving_lightmap_height;
                float *lightmap_data = entity->moving_lightmap_data.data[i];
                if (data->reset_lightmaps) {
                    for (int i = 0; i < lightmap_width * lightmap_height; i++) {
                        lightmap_data[i] = 0.0f;
                    }
                }

                GLuint tex;
                glGenTextures(1, &tex);
                glBindTexture(GL_TEXTURE_2D, tex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, lightmap_width, lightmap_height, 
                        0, GL_RED, GL_FLOAT, lightmap_data);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glBindTexture(GL_TEXTURE_2D, 0);

                array_push(&entity->gl_moving_tex, tex);
            }

            entity->gl_moving_position_vbo = moving_positions_vbo;
            entity->gl_moving_lightmap_uv_vbo = moving_lightmap_uvs_vbo;
        }

        {
            struct lightmap *static_lightmap = entity->static_lightmap;
            struct array_vec3 *static_positions = &entity->static_positions;
            struct array_vec2 *static_lightmap_uvs = &entity->static_lightmap_uvs;
            InterlockedIncrement(&uv_gen_progress_idx);
            if (data->create_uvs) {
                xatlas_wrapper_generate_lightmap_uvs(static_lightmap_uvs->data, static_positions->data, 
                        static_positions->length);
            }

            GLuint static_positions_vbo;
            glGenBuffers(1, &static_positions_vbo);
            glBindBuffer(GL_ARRAY_BUFFER, static_positions_vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * static_positions->length, static_positions->data, 
                    GL_STATIC_DRAW);

            GLuint static_lightmap_uvs_vbo;
            glGenBuffers(1, &static_lightmap_uvs_vbo);
            glBindBuffer(GL_ARRAY_BUFFER, static_lightmap_uvs_vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * static_lightmap_uvs->length, static_lightmap_uvs->data, 
                    GL_STATIC_DRAW);

            array_init(&entity->gl_static_tex);
            for (int i = 0; i < static_lightmap->images.length; i++) {
                int lightmap_width = entity->static_lightmap_width;
                int lightmap_height = entity->static_lightmap_height;
                float *lightmap_data = entity->static_lightmap_data.data[i];
                if (data->reset_lightmaps) {
                    for (int i = 0; i < lightmap_width * lightmap_height; i++) {
                        lightmap_data[i] = 0.0f;
                    }
                }

                GLuint tex;
                glGenTextures(1, &tex);
                glBindTexture(GL_TEXTURE_2D, tex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, lightmap_width, lightmap_height, 
                        0, GL_RED, GL_FLOAT, lightmap_data);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glBindTexture(GL_TEXTURE_2D, 0);

                array_push(&entity->gl_static_tex, tex);
            }

            entity->gl_static_position_vbo = static_positions_vbo;
            entity->gl_static_lightmap_uv_vbo = static_lightmap_uvs_vbo;
        }
    }

    lm_context *ctx = lmCreate(
            64,
            0.001f, 100.0f,
            1.0f, 1.0f, 1.0f,
            2, 0.01f,
            0.0f);
    for (int b = 0; b < data->num_iterations; b++) {
        for (int i = 0; i < data->entities.length; i++) {
            InterlockedIncrement(&lm_gen_progress_idx);
            struct lightmap_entity *entity = &data->entities.data[i];

            struct array_vec3 positions = entity->positions;
            struct array_vec3 normals = entity->normals;
            struct array_vec2 lightmap_uvs = entity->lightmap_uvs;
            mat4 model_mat = entity->model_mat;
            int lightmap_width = entity->lightmap_width;
            int lightmap_height = entity->lightmap_height;
            float *lightmap_data = entity->lightmap_data;
            if (positions.length == 0) {
                continue;
            }

            memset(lightmap_data, 0, sizeof(float) * lightmap_width * lightmap_height);
            lmSetTargetLightmap(ctx, lightmap_data, lightmap_width, lightmap_height, 1);
            lmSetGeometry(ctx, mat4_transpose(model_mat).m,
                    LM_FLOAT, positions.data, sizeof(vec3),
                    LM_FLOAT, normals.data, sizeof(vec3),
                    LM_FLOAT, lightmap_uvs.data, sizeof(vec2),
                    positions.length, LM_NONE, 0);

            int vp[4];
            mat4 view, proj;
            while (lmBegin(ctx, vp, view.m, proj.m)) {
                glUseProgram(program);
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(GL_LEQUAL);

                view = mat4_transpose(view);
                proj = mat4_transpose(proj);
                mat4 proj_view_mat = mat4_multiply_n(2, proj, view);
                glViewport(vp[0], vp[1], vp[2], vp[3]);

                for (int i = 0; i < data->entities.length; i++) {
                    struct lightmap_entity *entity = &data->entities.data[i];
                    if (entity->positions.length == 0) {
                        continue;
                    }

                    mat4 model_mat = entity->model_mat;
                    mat4 mvp_mat = mat4_multiply(proj_view_mat, model_mat);
                    glUniformMatrix4fv(mvp_mat_id, 1, GL_TRUE, mvp_mat.m);
                    glUniform1i(ao_map_id, 0);

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, entity->gl_tex);

                    glBindBuffer(GL_ARRAY_BUFFER, entity->gl_position_vbo);
                    glVertexAttribPointer(position_id, 3, GL_FLOAT, GL_FALSE, 0, NULL);
                    glEnableVertexAttribArray(position_id);

                    glBindBuffer(GL_ARRAY_BUFFER, entity->gl_lightmap_uv_vbo);
                    glVertexAttribPointer(texture_coord_id, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                    glEnableVertexAttribArray(texture_coord_id);

                    glBindVertexArray(dummy_vao);

                    glDrawArrays(GL_TRIANGLES, 0, entity->positions.length);
                }

                for (int i = 0; i < data->multi_entities.length; i++) {
                    struct lightmap_multi_entity *entity = &data->multi_entities.data[i];
                    if (entity->static_positions.length == 0) {
                        continue;
                    }

                    mat4 model_mat = entity->static_model_mat;
                    mat4 mvp_mat = mat4_multiply(proj_view_mat, model_mat);
                    glUniformMatrix4fv(mvp_mat_id, 1, GL_TRUE, mvp_mat.m);
                    glUniform1i(ao_map_id, 0);

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, entity->gl_static_tex.data[0]);

                    glBindBuffer(GL_ARRAY_BUFFER, entity->gl_static_position_vbo);
                    glVertexAttribPointer(position_id, 3, GL_FLOAT, GL_FALSE, 0, NULL);
                    glEnableVertexAttribArray(position_id);

                    glBindBuffer(GL_ARRAY_BUFFER, entity->gl_static_lightmap_uv_vbo);
                    glVertexAttribPointer(texture_coord_id, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                    glEnableVertexAttribArray(texture_coord_id);

                    glBindVertexArray(dummy_vao);

                    glDrawArrays(GL_TRIANGLES, 0, entity->static_positions.length);
                }

                int progress_pct = (int) (lmProgress(ctx) * 100.0f * 100.0f);
                InterlockedExchange(&lm_gen_progress_pct, progress_pct);

                lmEnd(ctx);
            }
        }

        for (int i = 0; i < data->multi_entities.length; i++) {
            struct lightmap_multi_entity *entity = &data->multi_entities.data[i];

            for (int ti = 0; ti < entity->moving_lightmap_data.length; ti++) {
                InterlockedIncrement(&lm_gen_progress_idx);

                struct array_vec3 positions = entity->moving_positions;
                struct array_vec3 normals = entity->moving_normals;
                struct array_vec2 lightmap_uvs = entity->moving_lightmap_uvs;
                mat4 model_mat = entity->moving_model_mats.data[ti];
                int lightmap_width = entity->moving_lightmap_width;
                int lightmap_height = entity->moving_lightmap_height;
                float *lightmap_data = entity->moving_lightmap_data.data[ti];
                if (positions.length == 0) {
                    continue;
                }

                memset(lightmap_data, 0, sizeof(float) * lightmap_width * lightmap_height);
                lmSetTargetLightmap(ctx, lightmap_data, lightmap_width, lightmap_height, 1);
                lmSetGeometry(ctx, mat4_transpose(model_mat).m,
                        LM_FLOAT, positions.data, sizeof(vec3),
                        LM_FLOAT, normals.data, sizeof(vec3),
                        LM_FLOAT, lightmap_uvs.data, sizeof(vec2),
                        positions.length, LM_NONE, 0);

                int vp[4];
                mat4 view, proj;
                while (lmBegin(ctx, vp, view.m, proj.m)) {
                    glUseProgram(program);
                    glEnable(GL_DEPTH_TEST);
                    glDepthFunc(GL_LEQUAL);

                    view = mat4_transpose(view);
                    proj = mat4_transpose(proj);
                    mat4 proj_view_mat = mat4_multiply_n(2, proj, view);
                    glViewport(vp[0], vp[1], vp[2], vp[3]);

                    for (int i = 0; i < data->entities.length; i++) {
                        struct lightmap_entity *draw_entity = &data->entities.data[i];
                        if (draw_entity->positions.length == 0) {
                            continue;
                        }

                        mat4 model_mat = draw_entity->model_mat;
                        mat4 mvp_mat = mat4_multiply(proj_view_mat, model_mat);
                        glUniformMatrix4fv(mvp_mat_id, 1, GL_TRUE, mvp_mat.m);
                        glUniform1i(ao_map_id, 0);

                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, draw_entity->gl_tex);

                        glBindBuffer(GL_ARRAY_BUFFER, draw_entity->gl_position_vbo);
                        glVertexAttribPointer(position_id, 3, GL_FLOAT, GL_FALSE, 0, NULL);
                        glEnableVertexAttribArray(position_id);

                        glBindBuffer(GL_ARRAY_BUFFER, draw_entity->gl_lightmap_uv_vbo);
                        glVertexAttribPointer(texture_coord_id, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                        glEnableVertexAttribArray(texture_coord_id);

                        glBindVertexArray(dummy_vao);

                        glDrawArrays(GL_TRIANGLES, 0, draw_entity->positions.length);
                    }

                    for (int i = 0; i < data->multi_entities.length; i++) {
                        struct lightmap_multi_entity *draw_entity = &data->multi_entities.data[i];
                        if (draw_entity != entity) {
                            continue;
                        }

                        if (draw_entity->static_positions.length > 0) {
                            mat4 model_mat = draw_entity->static_model_mat;
                            mat4 mvp_mat = mat4_multiply(proj_view_mat, model_mat);
                            glUniformMatrix4fv(mvp_mat_id, 1, GL_TRUE, mvp_mat.m);
                            glUniform1i(ao_map_id, 0);

                            glActiveTexture(GL_TEXTURE0);
                            glBindTexture(GL_TEXTURE_2D, draw_entity->gl_static_tex.data[ti]);

                            glBindBuffer(GL_ARRAY_BUFFER, draw_entity->gl_static_position_vbo);
                            glVertexAttribPointer(position_id, 3, GL_FLOAT, GL_FALSE, 0, NULL);
                            glEnableVertexAttribArray(position_id);

                            glBindBuffer(GL_ARRAY_BUFFER, draw_entity->gl_static_lightmap_uv_vbo);
                            glVertexAttribPointer(texture_coord_id, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                            glEnableVertexAttribArray(texture_coord_id);

                            glBindVertexArray(dummy_vao);

                            glDrawArrays(GL_TRIANGLES, 0, draw_entity->static_positions.length);
                        }

                        if (draw_entity->moving_positions.length > 0) {
                            mat4 model_mat = draw_entity->moving_model_mats.data[ti];
                            mat4 mvp_mat = mat4_multiply(proj_view_mat, model_mat);
                            glUniformMatrix4fv(mvp_mat_id, 1, GL_TRUE, mvp_mat.m);
                            glUniform1i(ao_map_id, 0);

                            glActiveTexture(GL_TEXTURE0);
                            glBindTexture(GL_TEXTURE_2D, draw_entity->gl_moving_tex.data[ti]);

                            glBindBuffer(GL_ARRAY_BUFFER, draw_entity->gl_moving_position_vbo);
                            glVertexAttribPointer(position_id, 3, GL_FLOAT, GL_FALSE, 0, NULL);
                            glEnableVertexAttribArray(position_id);

                            glBindBuffer(GL_ARRAY_BUFFER, draw_entity->gl_moving_lightmap_uv_vbo);
                            glVertexAttribPointer(texture_coord_id, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                            glEnableVertexAttribArray(texture_coord_id);

                            glBindVertexArray(dummy_vao);

                            glDrawArrays(GL_TRIANGLES, 0, draw_entity->moving_positions.length);
                        }
                    }

                    int progress_pct = (int) (lmProgress(ctx) * 100.0f * 100.0f);
                    InterlockedExchange(&lm_gen_progress_pct, progress_pct);

                    lmEnd(ctx);
                }
            }

            for (int ti = 0; ti < entity->static_lightmap_data.length; ti++) {
                InterlockedIncrement(&lm_gen_progress_idx);

                struct array_vec3 positions = entity->static_positions;
                struct array_vec3 normals = entity->static_normals;
                struct array_vec2 lightmap_uvs = entity->static_lightmap_uvs;
                mat4 model_mat = entity->static_model_mat;
                int lightmap_width = entity->static_lightmap_width;
                int lightmap_height = entity->static_lightmap_height;
                float *lightmap_data = entity->static_lightmap_data.data[ti];
                if (positions.length == 0) {
                    continue;
                }

                memset(lightmap_data, 0, sizeof(float) * lightmap_width * lightmap_height);
                lmSetTargetLightmap(ctx, lightmap_data, lightmap_width, lightmap_height, 1);
                lmSetGeometry(ctx, mat4_transpose(model_mat).m,
                        LM_FLOAT, positions.data, sizeof(vec3),
                        LM_FLOAT, normals.data, sizeof(vec3),
                        LM_FLOAT, lightmap_uvs.data, sizeof(vec2),
                        positions.length, LM_NONE, 0);

                int vp[4];
                mat4 view, proj;
                while (lmBegin(ctx, vp, view.m, proj.m)) {
                    glUseProgram(program);
                    glEnable(GL_DEPTH_TEST);
                    glDepthFunc(GL_LEQUAL);

                    view = mat4_transpose(view);
                    proj = mat4_transpose(proj);
                    mat4 proj_view_mat = mat4_multiply_n(2, proj, view);
                    glViewport(vp[0], vp[1], vp[2], vp[3]);

                    for (int i = 0; i < data->entities.length; i++) {
                        struct lightmap_entity *draw_entity = &data->entities.data[i];
                        if (draw_entity->positions.length == 0) {
                            continue;
                        }

                        mat4 model_mat = draw_entity->model_mat;
                        mat4 mvp_mat = mat4_multiply(proj_view_mat, model_mat);
                        glUniformMatrix4fv(mvp_mat_id, 1, GL_TRUE, mvp_mat.m);
                        glUniform1i(ao_map_id, 0);

                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, draw_entity->gl_tex);

                        glBindBuffer(GL_ARRAY_BUFFER, draw_entity->gl_position_vbo);
                        glVertexAttribPointer(position_id, 3, GL_FLOAT, GL_FALSE, 0, NULL);
                        glEnableVertexAttribArray(position_id);

                        glBindBuffer(GL_ARRAY_BUFFER, draw_entity->gl_lightmap_uv_vbo);
                        glVertexAttribPointer(texture_coord_id, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                        glEnableVertexAttribArray(texture_coord_id);

                        glBindVertexArray(dummy_vao);

                        glDrawArrays(GL_TRIANGLES, 0, draw_entity->positions.length);
                    }

                    for (int i = 0; i < data->multi_entities.length; i++) {
                        struct lightmap_multi_entity *draw_entity = &data->multi_entities.data[i];
                        if (draw_entity != entity) {
                            continue;
                        }

                        if (draw_entity->static_positions.length > 0) {
                            mat4 model_mat = draw_entity->static_model_mat;
                            mat4 mvp_mat = mat4_multiply(proj_view_mat, model_mat);
                            glUniformMatrix4fv(mvp_mat_id, 1, GL_TRUE, mvp_mat.m);
                            glUniform1i(ao_map_id, 0);

                            glActiveTexture(GL_TEXTURE0);
                            glBindTexture(GL_TEXTURE_2D, draw_entity->gl_static_tex.data[ti]);

                            glBindBuffer(GL_ARRAY_BUFFER, draw_entity->gl_static_position_vbo);
                            glVertexAttribPointer(position_id, 3, GL_FLOAT, GL_FALSE, 0, NULL);
                            glEnableVertexAttribArray(position_id);

                            glBindBuffer(GL_ARRAY_BUFFER, draw_entity->gl_static_lightmap_uv_vbo);
                            glVertexAttribPointer(texture_coord_id, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                            glEnableVertexAttribArray(texture_coord_id);

                            glBindVertexArray(dummy_vao);

                            glDrawArrays(GL_TRIANGLES, 0, draw_entity->static_positions.length);
                        }

                        if (draw_entity->moving_positions.length > 0) {
                            mat4 model_mat = draw_entity->moving_model_mats.data[ti];
                            mat4 mvp_mat = mat4_multiply(proj_view_mat, model_mat);
                            glUniformMatrix4fv(mvp_mat_id, 1, GL_TRUE, mvp_mat.m);
                            glUniform1i(ao_map_id, 0);

                            glActiveTexture(GL_TEXTURE0);
                            glBindTexture(GL_TEXTURE_2D, draw_entity->gl_moving_tex.data[ti]);

                            glBindBuffer(GL_ARRAY_BUFFER, draw_entity->gl_moving_position_vbo);
                            glVertexAttribPointer(position_id, 3, GL_FLOAT, GL_FALSE, 0, NULL);
                            glEnableVertexAttribArray(position_id);

                            glBindBuffer(GL_ARRAY_BUFFER, draw_entity->gl_moving_lightmap_uv_vbo);
                            glVertexAttribPointer(texture_coord_id, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                            glEnableVertexAttribArray(texture_coord_id);

                            glBindVertexArray(dummy_vao);

                            glDrawArrays(GL_TRIANGLES, 0, draw_entity->moving_positions.length);
                        }
                    }

                    int progress_pct = (int) (lmProgress(ctx) * 100.0f * 100.0f);
                    InterlockedExchange(&lm_gen_progress_pct, progress_pct);

                    lmEnd(ctx);
                }
            }
        }

        for (int i = 0; i < data->entities.length; i++) {
            struct lightmap_entity *entity = &data->entities.data[i];

            int lightmap_width = entity->lightmap_width;
            int lightmap_height = entity->lightmap_height;
            float *lightmap_data = entity->lightmap_data;
            float *temp = malloc(sizeof(float) * lightmap_width * lightmap_height);
            for (int j = 0; j < data->num_dilates; j++) {
                lmImageDilate(lightmap_data, temp, lightmap_width, lightmap_height, 1);
                lmImageDilate(temp, lightmap_data, lightmap_width, lightmap_height, 1);
            }
            for (int j = 0; j < data->num_smooths; j++) {
                lmImageSmooth(lightmap_data, temp, lightmap_width, lightmap_height, 1);
                lmImageSmooth(temp, lightmap_data, lightmap_width, lightmap_height, 1);
            }
            lmImagePower(lightmap_data, lightmap_width, lightmap_height, 1, data->gamma, LM_ALL_CHANNELS);

            glBindTexture(GL_TEXTURE_2D, entity->gl_tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_FLOAT, lightmap_width, lightmap_height,
                    0, GL_RED, GL_FLOAT, lightmap_data);
            glBindTexture(GL_TEXTURE_2D, 0);
            free(temp);
        }

        for (int i = 0; i < data->multi_entities.length; i++) {
            struct lightmap_multi_entity *entity = &data->multi_entities.data[i];

            {
                int lightmap_width = entity->static_lightmap_width;
                int lightmap_height = entity->static_lightmap_height;
                float *temp = malloc(sizeof(float) * lightmap_width * lightmap_height);

                for (int ti = 0; ti < entity->gl_static_tex.length; ti++) {
                    float *lightmap_data = entity->static_lightmap_data.data[ti];
                    for (int j = 0; j < data->num_dilates; j++) {
                        lmImageDilate(lightmap_data, temp, lightmap_width, lightmap_height, 1);
                        lmImageDilate(temp, lightmap_data, lightmap_width, lightmap_height, 1);
                    }
                    for (int j = 0; j < data->num_smooths; j++) {
                        lmImageSmooth(lightmap_data, temp, lightmap_width, lightmap_height, 1);
                        lmImageSmooth(temp, lightmap_data, lightmap_width, lightmap_height, 1);
                    }
                    lmImagePower(lightmap_data, lightmap_width, lightmap_height, 1, data->gamma, LM_ALL_CHANNELS);

                    glBindTexture(GL_TEXTURE_2D, entity->gl_static_tex.data[ti]);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_FLOAT, lightmap_width, lightmap_height,
                            0, GL_RED, GL_FLOAT, lightmap_data);
                    glBindTexture(GL_TEXTURE_2D, 0);
                }

                free(temp);
            }

            {
                int lightmap_width = entity->moving_lightmap_width;
                int lightmap_height = entity->moving_lightmap_height;
                float *temp = malloc(sizeof(float) * lightmap_width * lightmap_height);

                for (int ti = 0; ti < entity->gl_moving_tex.length; ti++) {
                    float *lightmap_data = entity->moving_lightmap_data.data[ti];
                    for (int j = 0; j < data->num_dilates; j++) {
                        lmImageDilate(lightmap_data, temp, lightmap_width, lightmap_height, 1);
                        lmImageDilate(temp, lightmap_data, lightmap_width, lightmap_height, 1);
                    }
                    for (int j = 0; j < data->num_smooths; j++) {
                        lmImageSmooth(lightmap_data, temp, lightmap_width, lightmap_height, 1);
                        lmImageSmooth(temp, lightmap_data, lightmap_width, lightmap_height, 1);
                    }
                    lmImagePower(lightmap_data, lightmap_width, lightmap_height, 1, data->gamma, LM_ALL_CHANNELS);

                    glBindTexture(GL_TEXTURE_2D, entity->gl_moving_tex.data[ti]);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_FLOAT, lightmap_width, lightmap_height,
                            0, GL_RED, GL_FLOAT, lightmap_data);
                    glBindTexture(GL_TEXTURE_2D, 0);
                }

                free(temp);
            }
        }
    }
    lmDestroy(ctx);

    /*
    for (int i = 0; i < data->entities.length; i++) {
        struct lightmap_entity *entity = &data->entities.data[i];

        int width = entity->lightmap_width;
        int height = entity->lightmap_height;
        float *data = entity->lightmap_data;

        char filename[1024];
        snprintf(filename, 1024, "lightmap_%d.tga", i);
        lmImageSaveTGAf(filename, data, width, height, 1, 0.0f);
    }
    */

    /*
    for (int i = 0; i < data->multi_entities.length; i++) {
        struct lightmap_multi_entity *entity = &data->multi_entities.data[i];
        for (int ti = 0; ti < entity->moving_lightmap_data.length; ti++) {
            int width = entity->moving_lightmap_width;
            int height = entity->moving_lightmap_height;
            float *data = entity->moving_lightmap_data.data[ti];

            char filename[1024];
            snprintf(filename, 1024, "lightmap_moving_%d_%d.tga", i, ti);
            lmImageSaveTGAf(filename, data, width, height, 1, 0.0f);
        }

        for (int ti = 0; ti < entity->static_lightmap_data.length; ti++) {
            int width = entity->static_lightmap_width;
            int height = entity->static_lightmap_height;
            float *data = entity->static_lightmap_data.data[ti];

            char filename[1024];
            snprintf(filename, 1024, "lightmap_static_%d_%d.tga", i, ti);
            lmImageSaveTGAf(filename, data, width, height, 1, 0.0f);
        }
    }
    */

    for (int i = 0; i < data->entities.length; i++) {
        struct lightmap_entity *entity = &data->entities.data[i];
        glDeleteBuffers(1, &entity->gl_position_vbo);
        glDeleteBuffers(1, &entity->gl_lightmap_uv_vbo);
        glDeleteTextures(1, &entity->gl_tex);
    }
    for (int i = 0; i < data->multi_entities.length; i++) {
        struct lightmap_multi_entity *entity = &data->multi_entities.data[i];
        glDeleteBuffers(1, &entity->gl_moving_position_vbo);
        glDeleteBuffers(1, &entity->gl_moving_lightmap_uv_vbo);
        glDeleteBuffers(1, &entity->gl_static_position_vbo);
        glDeleteBuffers(1, &entity->gl_static_lightmap_uv_vbo);
        for (int i = 0; i < entity->gl_moving_tex.length; i++) {
            glDeleteTextures(1, &entity->gl_moving_tex.data[i]);
        }
        for (int i = 0; i < entity->gl_static_tex.length; i++) {
            glDeleteTextures(1, &entity->gl_static_tex.data[i]);
        }
        array_deinit(&entity->gl_moving_tex);
        array_deinit(&entity->gl_static_tex);
    }
    glDeleteVertexArrays(1, &dummy_vao);
    glDeleteProgram(program);

    glfwTerminate();
    InterlockedExchange(&is_running, 0);
    return 0;
}

void lightmap_generator_init() {
    InterlockedExchange(&is_running, 0);
}

void lightmap_generator_start(struct lightmap_generator_data *data) {
    bool running = lightmap_generator_is_running();
    assert(!running);

    InterlockedExchange(&is_running, 1);
    InterlockedExchange(&uv_gen_progress_idx, 0);
    InterlockedExchange(&lm_gen_progress_idx, 0);
    InterlockedExchange(&lm_gen_progress_pct, 0);

    uv_gen_progress_count = data->entities.length + 2 * data->multi_entities.length;
    lm_gen_progress_count = data->entities.length;
    for (int i = 0; i < data->multi_entities.length; i++) {
        struct lightmap_multi_entity *entity = &data->multi_entities.data[i];
        lm_gen_progress_count += entity->static_lightmap->images.length;
        lm_gen_progress_count += entity->moving_lightmap->images.length;
    }

    DWORD thread_id;
    CreateThread(NULL, 0U, (LPTHREAD_START_ROUTINE)(uintptr_t) lightmap_generator_run, data, 0, &thread_id);
    assert(thread_id);
}

bool lightmap_generator_is_running() {
    return InterlockedCompareExchange(&is_running, 0, 0) == 1;
}

void lightmap_generator_get_progress_string(char *buffer, int buffer_len) {
    int create_uvs_idx = InterlockedCompareExchange(&uv_gen_progress_idx, 0, 0);
    int create_lightmaps_idx = InterlockedCompareExchange(&lm_gen_progress_idx, 0, 0);
    float create_lightmaps_pct = ((float) InterlockedCompareExchange(&lm_gen_progress_pct, 0, 0)) / 100.0f;
    snprintf(buffer, buffer_len,
            "1) Create UVs: %d/%d\n"
            "2) Create Lightmaps: %d/%d (%0.2f %%)\n",
            create_uvs_idx, uv_gen_progress_count, 
            create_lightmaps_idx, lm_gen_progress_count, create_lightmaps_pct);
    buffer[buffer_len - 1] = 0;
}

#endif
