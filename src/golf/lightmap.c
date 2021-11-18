#include "golf/lightmap.h"

#include <assert.h>
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "lightmapper/lightmapper.h"
#include "xatlas/xatlas_wrapper.h"
#include "golf/log.h"

static void _lm_gen_set_is_running(golf_lightmap_generator_t *generator, bool is_running) {
    thread_mutex_lock(&generator->lock);
    generator->is_running = is_running;
    thread_mutex_unlock(&generator->lock);
}

static void _lm_gen_inc_uv_gen_progress(golf_lightmap_generator_t *generator) {
    thread_mutex_lock(&generator->lock);
    generator->uv_gen_progress = generator->uv_gen_progress + 1;
    thread_mutex_unlock(&generator->lock);
}

static void _lm_gen_inc_lm_gen_progress(golf_lightmap_generator_t *generator) {
    thread_mutex_lock(&generator->lock);
    generator->lm_gen_progress = generator->lm_gen_progress + 1;
    thread_mutex_unlock(&generator->lock);
}

void golf_lightmap_generator_init(golf_lightmap_generator_t *generator,
        bool reset_lightmaps, bool create_uvs, float gamma, 
        int num_iterations, int num_dilates, int num_smooths) {
    generator->reset_lightmaps = reset_lightmaps;
    generator->create_uvs = create_uvs;
    generator->gamma = gamma;
    generator->num_iterations = num_iterations;
    generator->num_dilates = num_dilates;
    generator->num_smooths = num_smooths;
    vec_init(&generator->entities);

    thread_mutex_init(&generator->lock);
    generator->is_running = false;
    generator->uv_gen_progress = 0;
    generator->lm_gen_progress = 0;
    generator->lm_gen_progress_pct = 0;
}

static GLuint _load_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    assert(shader != 0);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    assert(compiled);
    return shader;
}

static int _lightmap_generator_run(void *user_data) {
    golf_lightmap_generator_t *generator = (golf_lightmap_generator_t*)user_data;

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
        GLuint vertex_shader = _load_shader(GL_VERTEX_SHADER, vs);
        GLuint fragment_shader = _load_shader(GL_FRAGMENT_SHADER, fs);
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

    for (int i = 0; i < generator->entities.length; i++) {
        golf_lightmap_entity_t *entity = &generator->entities.data[i];

        vec_vec3_t *positions = &entity->positions;
        vec_vec2_t *lightmap_uvs = &entity->lightmap_uvs;
        _lm_gen_inc_uv_gen_progress(generator);
        if (generator->create_uvs) {
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

        int lightmap_size = entity->lightmap_size;
        float *lightmap_data = entity->lightmap_data;
        if (generator->reset_lightmaps) {
            for (int i = 0; i < lightmap_size * lightmap_size; i++) {
                lightmap_data[i] = 0.0f;
            }
        }

        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, lightmap_size, lightmap_size, 0, GL_RED, GL_FLOAT, lightmap_data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        entity->gl_position_vbo = positions_vbo;
        entity->gl_lightmap_uv_vbo = lightmap_uvs_vbo;
        entity->gl_tex = tex;
    }

    lm_context *ctx = lmCreate(64, 0.01f, 100.0f, 1.0f, 1.0f, 1.0f, 2, 0.01f, 0.0f);
    for (int b = 0; b < generator->num_iterations; b++) {
        for (int i = 0; i < generator->entities.length; i++) {
            _lm_gen_inc_lm_gen_progress(generator);
            golf_lightmap_entity_t *entity = &generator->entities.data[i];
            if (entity->positions.length == 0) {
                continue;
            }

            float *lm_data = entity->lightmap_data;
            int lm_size = entity->lightmap_size;

            for (int i = 0; i < lm_size * lm_size; i++) {
                lm_data[i] = 0.0f;
            }
            lmSetTargetLightmap(ctx, lm_data, lm_size, lm_size, 1);
            lmSetGeometry(ctx, mat4_transpose(entity->model_mat).m,
                    LM_FLOAT, entity->positions.data, sizeof(vec3),
                    LM_FLOAT, entity->normals.data, sizeof(vec3),
                    LM_FLOAT, entity->lightmap_uvs.data, sizeof(vec2),
                    entity->positions.length, LM_NONE, 0);

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

                for (int i = 0; i < generator->entities.length; i++) {
                    golf_lightmap_entity_t *entity = &generator->entities.data[i];
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

                lmEnd(ctx);
            }
        }

        for (int i = 0; i < generator->entities.length; i++) {
            golf_lightmap_entity_t *entity = &generator->entities.data[i];
            int lm_size = entity->lightmap_size;
            float *lm_data = entity->lightmap_data;
            float *temp = malloc(sizeof(float) * lm_size * lm_size);
            for (int i = 0; i < generator->num_dilates; i++) {
                lmImageDilate(lm_data, temp, lm_size, lm_size, 1);
                lmImageDilate(temp, lm_data, lm_size, lm_size, 1);
            }
            for (int i = 0; i < generator->num_smooths; i++) {
                lmImageSmooth(lm_data, temp, lm_size, lm_size, 1);
                lmImageSmooth(temp, lm_data, lm_size, lm_size, 1);
            }
            lmImagePower(lm_data, lm_size, lm_size, 1, generator->gamma, LM_ALL_CHANNELS);

            glBindTexture(GL_TEXTURE_2D, entity->gl_tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_FLOAT, lm_size, lm_size, 0, GL_RED, GL_FLOAT, lm_data);
            glBindTexture(GL_TEXTURE_2D, 0);
            free(temp);
        }
    }

    for (int i = 0; i < generator->entities.length; i++) {
        golf_lightmap_entity_t *entity = &generator->entities.data[i];
        glDeleteBuffers(1, (GLuint*)&entity->gl_position_vbo);
        glDeleteBuffers(1, (GLuint*)&entity->gl_lightmap_uv_vbo);
        glDeleteTextures(1, (GLuint*)&entity->gl_tex);
    }
    glDeleteVertexArrays(1, &dummy_vao);
    glDeleteProgram(program);
    lmDestroy(ctx);
    glfwTerminate();

    _lm_gen_set_is_running(generator, false);
    return 0;
}

void golf_lightmap_generator_start(golf_lightmap_generator_t *generator) {
    if (golf_lightmap_generator_is_running(generator)) {
        golf_log_error("Cannot start lightmap generator when it's already running.");
    }

    _lm_gen_set_is_running(generator, true);
    generator->thread = thread_create0(_lightmap_generator_run, generator, "Lightmap Generator", THREAD_STACK_SIZE_DEFAULT);
}

void golf_lightmap_generator_add_entity(golf_lightmap_generator_t *generator, golf_model_t *model, mat4 model_mat, golf_lightmap_t *lightmap) {
    golf_lightmap_entity_t entity;
    vec_init(&entity.positions);
    vec_init(&entity.normals);
    vec_init(&entity.lightmap_uvs);
    entity.lightmap_data = malloc(sizeof(float) * lightmap->size * lightmap->size);

    entity.model_mat = model_mat;
    vec_pusharr(&entity.positions, model->positions.data, model->positions.length);
    vec_pusharr(&entity.normals, model->normals.data, model->normals.length);
    vec_pusharr(&entity.lightmap_uvs, lightmap->uvs.data, lightmap->uvs.length);
    for (int i = entity.lightmap_uvs.length; i < entity.positions.length; i++) {
        vec_push(&entity.lightmap_uvs, V2(0, 0));
    }
    entity.lightmap_size = lightmap->size;
    for (int i = 0; i < lightmap->size * lightmap->size; i++) {
        entity.lightmap_data[i] = ((float)lightmap->data[i]) / 0xFF;
    }
    entity.lightmap = lightmap;

    vec_push(&generator->entities, entity);
}

void golf_lightmap_generator_deinit(golf_lightmap_generator_t *generator) {
    thread_join(generator->thread);
    thread_destroy(generator->thread);
}

int golf_lightmap_generator_get_lm_gen_progress(golf_lightmap_generator_t *generator) {
    thread_mutex_lock(&generator->lock);
    int lm_gen_progress = generator->lm_gen_progress;
    thread_mutex_unlock(&generator->lock);
    return lm_gen_progress;
}

int golf_lightmap_generator_get_uv_gen_progress(golf_lightmap_generator_t *generator) {
    thread_mutex_lock(&generator->lock);
    int uv_gen_progress = generator->uv_gen_progress;
    thread_mutex_unlock(&generator->lock);
    return uv_gen_progress;
}

bool golf_lightmap_generator_is_running(golf_lightmap_generator_t *generator) {
    thread_mutex_lock(&generator->lock);
    bool is_running = generator->is_running;
    thread_mutex_unlock(&generator->lock);
    return is_running;
}
