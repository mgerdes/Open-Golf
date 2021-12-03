#include "golf/gi.h"

#include <assert.h>
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "lightmapper/lightmapper.h"
#include "xatlas/xatlas_wrapper.h"
#include "golf/log.h"

static void _gi_set_is_running(golf_gi_t *gi, bool is_running) {
    thread_mutex_lock(&gi->lock);
    gi->is_running = is_running;
    thread_mutex_unlock(&gi->lock);
}

static void _gi_inc_uv_gen_progress(golf_gi_t *gi) {
    thread_mutex_lock(&gi->lock);
    gi->uv_gen_progress = gi->uv_gen_progress + 1;
    thread_mutex_unlock(&gi->lock);
}

static void _gi_inc_lm_gen_progress(golf_gi_t *gi) {
    thread_mutex_lock(&gi->lock);
    gi->lm_gen_progress = gi->lm_gen_progress + 1;
    thread_mutex_unlock(&gi->lock);
}

void golf_gi_init(golf_gi_t *gi,
        bool reset_lightmaps, bool create_uvs, float gamma, 
        int num_iterations, int num_dilates, int num_smooths,
        int hemisphere_size, float z_near, float z_far,
        int interpolation_passes, float interpolation_threshold,
        float camera_to_surface_distance_modifier) {
    gi->reset_lightmaps = reset_lightmaps;
    gi->create_uvs = create_uvs;
    gi->gamma = gamma;
    gi->num_iterations = num_iterations;
    gi->num_dilates = num_dilates;
    gi->num_smooths = num_smooths;
    gi->hemisphere_size = hemisphere_size;
    gi->z_near = z_near;
    gi->z_far = z_far;
    gi->interpolation_passes = interpolation_passes;
    gi->interpolation_threshold = interpolation_threshold;
    gi->camera_to_surface_distance_modifier = camera_to_surface_distance_modifier;

    gi->has_cur_entity = false;
    vec_init(&gi->entities);

    thread_mutex_init(&gi->lock);
    gi->is_running = false;
    gi->uv_gen_progress = 0;
    gi->lm_gen_progress = 0;
    gi->lm_gen_progress_pct = 0;
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

static int _gi_run(void *user_data) {
    golf_gi_t *gi = (golf_gi_t*)user_data;

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

    for (int i = 0; i < gi->entities.length; i++) {
        golf_gi_entity_t *entity = &gi->entities.data[i];

        vec_vec3_t *positions = &entity->positions;
        vec_vec2_t *lightmap_uvs = &entity->lightmap_uvs;
        _gi_inc_uv_gen_progress(gi);
        if (gi->create_uvs) {
            int resolution = entity->resolution;
            int *image_width = &entity->image_width;
            int *image_height = &entity->image_height;
            vec2 *lightmap_uv = lightmap_uvs->data;
            vec3 *vertices = positions->data;
            int num_vertices = positions->length;
            xatlas_wrapper_generate_lightmap_uvs(resolution, lightmap_uv, vertices, num_vertices, image_width, image_height);

            entity->image_data = malloc(sizeof(float) * entity->image_width * entity->image_height);
            for (int i = 0; i < entity->image_width * entity->image_height; i++) {
                entity->image_data[i] = 1.0f;
            }
        }

        GLuint positions_vbo;
        glGenBuffers(1, &positions_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, positions_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * positions->length, positions->data, GL_STATIC_DRAW);

        GLuint lightmap_uvs_vbo;
        glGenBuffers(1, &lightmap_uvs_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, lightmap_uvs_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * lightmap_uvs->length, lightmap_uvs->data, GL_STATIC_DRAW);

        int image_width = entity->image_width;
        int image_height = entity->image_height;
        float *image_data = entity->image_data;
        if (gi->reset_lightmaps) {
            for (int i = 0; i < image_width * image_height; i++) {
                image_data[i] = 0.0f;
            }
        }

        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, image_width, image_height, 0, GL_RED, GL_FLOAT, image_data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        entity->gl_position_vbo = positions_vbo;
        entity->gl_lightmap_uv_vbo = lightmap_uvs_vbo;
        entity->gl_tex = tex;
    }

    lm_context *ctx = lmCreate(gi->hemisphere_size,
            gi->z_near, 
            gi->z_far,
            1.0f, 1.0f, 1.0f,
            gi->interpolation_passes,
            gi->interpolation_threshold,
            gi->camera_to_surface_distance_modifier);
    for (int b = 0; b < gi->num_iterations; b++) {
        for (int i = 0; i < gi->entities.length; i++) {
            _gi_inc_lm_gen_progress(gi);
            golf_gi_entity_t *entity = &gi->entities.data[i];
            if (entity->positions.length == 0) {
                continue;
            }

            float *image_data = entity->image_data;
            int image_width = entity->image_width;
            int image_height = entity->image_height;

            for (int i = 0; i < image_width * image_height; i++) {
                image_data[i] = 0.0f;
            }
            lmSetTargetLightmap(ctx, image_data, image_width, image_height, 1);
            lmSetGeometry(ctx, mat4_transpose(mat4_identity()).m,
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

                for (int i = 0; i < gi->entities.length; i++) {
                    golf_gi_entity_t *entity = &gi->entities.data[i];
                    if (entity->positions.length == 0) {
                        continue;
                    }

                    glUniformMatrix4fv(mvp_mat_id, 1, GL_TRUE, proj_view_mat.m);
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

        for (int i = 0; i < gi->entities.length; i++) {
            golf_gi_entity_t *entity = &gi->entities.data[i];
            int image_width = entity->image_width;
            int image_height = entity->image_height;
            float *image_data = entity->image_data;
            glBindTexture(GL_TEXTURE_2D, entity->gl_tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_FLOAT, image_width, image_height, 0, GL_RED, GL_FLOAT, image_data);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    for (int i = 0; i < gi->entities.length; i++) {
        golf_gi_entity_t *entity = &gi->entities.data[i];
        int image_width = entity->image_width;
        int image_height = entity->image_height;
        float *image_data = entity->image_data;
        float *temp = malloc(sizeof(float) * image_width * image_height);
        for (int i = 0; i < gi->num_dilates; i++) {
            lmImageDilate(image_data, temp, image_width, image_height, 1);
            lmImageDilate(temp, image_data, image_width, image_height, 1);
        }
        for (int i = 0; i < gi->num_smooths; i++) {
            lmImageSmooth(image_data, temp, image_width, image_height, 1);
            lmImageSmooth(temp, image_data, image_width, image_height, 1);
        }
        lmImagePower(image_data, image_width, image_height, 1, gi->gamma, LM_ALL_CHANNELS);
        free(temp);
    }

    for (int i = 0; i < gi->entities.length; i++) {
        golf_gi_entity_t *entity = &gi->entities.data[i];
        glDeleteBuffers(1, (GLuint*)&entity->gl_position_vbo);
        glDeleteBuffers(1, (GLuint*)&entity->gl_lightmap_uv_vbo);
        glDeleteTextures(1, (GLuint*)&entity->gl_tex);
    }
    glDeleteVertexArrays(1, &dummy_vao);
    glDeleteProgram(program);
    lmDestroy(ctx);
    glfwTerminate();

    _gi_set_is_running(gi, false);
    return 0;
}

void golf_gi_start(golf_gi_t *gi) {
    if (golf_gi_is_running(gi)) {
        golf_log_error("Cannot start gi when it's already running.");
    }

    _gi_set_is_running(gi, true);
    gi->thread = thread_create0(_gi_run, gi, "gi", THREAD_STACK_SIZE_DEFAULT);
}

void golf_gi_start_lightmap(golf_gi_t *gi, golf_lightmap_image_t *lightmap_image) {
    if (gi->has_cur_entity) {
        return;
    }

    golf_gi_entity_t entity;
    vec_init(&entity.positions);
    vec_init(&entity.normals);
    vec_init(&entity.lightmap_uvs);
    vec_init(&entity.gi_lightmap_sections);

    entity.resolution = lightmap_image->resolution;
    entity.image_width = lightmap_image->width;
    entity.image_height = lightmap_image->height;
    entity.lightmap_image = lightmap_image;

    gi->has_cur_entity = true;
    gi->cur_entity = entity;
}

void golf_gi_end_lightmap(golf_gi_t *gi) {
    if (!gi->has_cur_entity) {
        return;
    }

    vec_push(&gi->entities, gi->cur_entity);
    gi->has_cur_entity = false;
}

void golf_gi_add_lightmap_section(golf_gi_t *gi, golf_lightmap_section_t *lightmap_section, golf_model_t *model, mat4 model_mat) {
    if (!gi->has_cur_entity) {
        return;
    }

    golf_gi_lightmap_section_t gi_lightmap_section;
    gi_lightmap_section.lightmap_section = lightmap_section;
    gi_lightmap_section.start = gi->cur_entity.positions.length;
    gi_lightmap_section.count = model->positions.length;
    vec_push(&gi->cur_entity.gi_lightmap_sections, gi_lightmap_section);

    for (int i = 0; i < model->positions.length; i++) {
        vec3 position = vec3_apply_mat4(model->positions.data[i], 1, model_mat);
        vec3 normal = model->normals.data[i];
        vec2 uv = V2(0, 0);
        vec_push(&gi->cur_entity.positions, position);
        vec_push(&gi->cur_entity.normals, normal);
        vec_push(&gi->cur_entity.lightmap_uvs, uv);
    }
}

void golf_gi_deinit(golf_gi_t *gi) {
    thread_join(gi->thread);
    thread_destroy(gi->thread);
}

int golf_gi_get_lm_gen_progress(golf_gi_t *gi) {
    thread_mutex_lock(&gi->lock);
    int lm_gen_progress = gi->lm_gen_progress;
    thread_mutex_unlock(&gi->lock);
    return lm_gen_progress;
}

int golf_gi_get_uv_gen_progress(golf_gi_t *gi) {
    thread_mutex_lock(&gi->lock);
    int uv_gen_progress = gi->uv_gen_progress;
    thread_mutex_unlock(&gi->lock);
    return uv_gen_progress;
}

bool golf_gi_is_running(golf_gi_t *gi) {
    thread_mutex_lock(&gi->lock);
    bool is_running = gi->is_running;
    thread_mutex_unlock(&gi->lock);
    return is_running;
}
