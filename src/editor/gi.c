#include "editor/gi.h"

#include <assert.h>
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "lightmapper/lightmapper.h"
#include "xatlas/xatlas_wrapper.h"
#include "common/log.h"

static void _gi_set_is_running(golf_gi_t *gi, bool is_running) {
    golf_mutex_lock(&gi->lock);
    gi->is_running = is_running;
    golf_mutex_unlock(&gi->lock);
}

static void _gi_inc_uv_gen_progress(golf_gi_t *gi) {
    golf_mutex_lock(&gi->lock);
    gi->uv_gen_progress = gi->uv_gen_progress + 1;
    golf_mutex_unlock(&gi->lock);
}

static void _gi_inc_lm_gen_progress(golf_gi_t *gi) {
    golf_mutex_lock(&gi->lock);
    gi->lm_gen_progress = gi->lm_gen_progress + 1;
    golf_mutex_unlock(&gi->lock);
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
    vec_init(&gi->entities, "gi");

    golf_mutex_init(&gi->lock);
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

static golf_thread_result_t _gi_run(void *user_data) {
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
            "in vec2 frag_ignore;"
            "uniform sampler2D ao_map;"
            "out vec4 g_fragment_color;"
            "void main() {"
            "    if (frag_ignore.x > 0) discard;"
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
            "in vec2 ignore;"
            "uniform mat4 mvp_mat;"
            "out vec2 frag_texture_coord;"
            "out vec2 frag_ignore;"
            "void main() {"
            "    frag_texture_coord = texture_coord;"
            "    frag_ignore = ignore;"   
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

    GLint ignore_id = glGetAttribLocation(program, "ignore");
    assert(ignore_id >= 0);

    GLuint dummy_vao;
    glGenVertexArrays(1, &dummy_vao);

    for (int i = 0; i < gi->entities.length; i++) {
        golf_gi_entity_t *entity = &gi->entities.data[i];

        for (int i = 0; i < entity->gi_lightmap_sections.length; i++) {
            golf_gi_lightmap_section_t *section = &entity->gi_lightmap_sections.data[i];
            mat4 model_mat = golf_transform_get_model_mat(section->transform);
            for (int i = 0; i < section->positions.length; i++) {
                vec3 p = section->positions.data[i];
                p = vec3_apply_mat4(p, 1, model_mat);
                vec3 n = section->normals.data[i];
                n = vec3_normalize(vec3_apply_mat4(n, 0, mat4_transpose(mat4_inverse(model_mat))));
                vec2 uv = section->lightmap_uvs.data[i];
                vec_push(&entity->positions, p);
                vec_push(&entity->normals, n);
                vec_push(&entity->lightmap_uvs, uv);

                vec2 ignore = section->should_draw ? V2(0, 0) : V2(1, 1);
                vec_push(&entity->ignore, ignore);
            }
        }

        vec_vec3_t *positions = &entity->positions;
        vec_vec2_t *lightmap_uvs = &entity->lightmap_uvs;
        vec_vec2_t *ignore = &entity->ignore;
        _gi_inc_uv_gen_progress(gi);
        if (gi->create_uvs) {
            int resolution = entity->resolution;
            int *image_width = &entity->image_width;
            int *image_height = &entity->image_height;
            vec2 *lightmap_uv = lightmap_uvs->data;
            vec3 *vertices = positions->data;
            int num_vertices = positions->length;
            xatlas_wrapper_generate_lightmap_uvs(resolution, (float*)lightmap_uv, (float*)vertices, num_vertices, image_width, image_height);
        }

        entity->image_data = malloc(sizeof(float*) * entity->num_samples);
        for (int s = 0; s < entity->num_samples; s++) {
            entity->image_data[s] = malloc(sizeof(float) * entity->image_width * entity->image_height);
            for (int i = 0; i < entity->image_width * entity->image_height; i++) {
                entity->image_data[s][i] = 1.0f;
            }
        }

        int uv_idx = 0;
        for (int i = 0; i < entity->gi_lightmap_sections.length; i++) {
            golf_gi_lightmap_section_t *section = &entity->gi_lightmap_sections.data[i];
            for (int i = 0; i < section->positions.length; i++) {
                section->lightmap_uvs.data[i] = entity->lightmap_uvs.data[uv_idx++];
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

        GLuint ignore_vbo;
        glGenBuffers(1, &ignore_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, ignore_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * ignore->length, ignore->data, GL_STATIC_DRAW);

        int image_width = entity->image_width;
        int image_height = entity->image_height;
        int num_samples = entity->num_samples;
        float **image_data = entity->image_data;
        if (gi->reset_lightmaps) {
            for (int s = 0; s < num_samples; s++) {
                for (int i = 0; i < image_width * image_height; i++) {
                    image_data[s][i] = 0.0f;
                }
            }
        }

        entity->gl_tex = malloc(sizeof(int*) * num_samples);
        for (int s = 0; s < num_samples; s++) {
            GLuint tex;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, image_width, image_height, 0, GL_RED, GL_FLOAT, image_data[s]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);
            entity->gl_tex[s] = tex;
        }

        entity->gl_position_vbo = positions_vbo;
        entity->gl_lightmap_uv_vbo = lightmap_uvs_vbo;
        entity->gl_ignore_vbo = ignore_vbo;
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

            float **image_data = entity->image_data;
            int num_samples = entity->num_samples;
            int image_width = entity->image_width;
            int image_height = entity->image_height;

            for (int s = 0; s < num_samples; s++) {
                for (int i = 0; i < gi->entities.length; i++) {
                    golf_gi_entity_t *entity = &gi->entities.data[i];

                    entity->positions.length = 0;
                    entity->normals.length = 0;

                    for (int i = 0; i < entity->gi_lightmap_sections.length; i++) {
                        golf_gi_lightmap_section_t *section = &entity->gi_lightmap_sections.data[i];
                        golf_transform_t transform = section->transform;
                        golf_movement_t movement = section->movement;
                        float a = 0;
                        if (entity->num_samples > 1) {
                            a = ((float) s) / (entity->num_samples - 1);
                        }

                        float t = a * entity->time_length;
                        if (entity->repeats) {
                            t = 0.5f * t;
                        }
                        transform = golf_transform_apply_movement(transform, movement, t);
                        mat4 model_mat = golf_transform_get_model_mat(transform);

                        for (int i = 0; i < section->positions.length; i++) {
                            vec3 p = section->positions.data[i];
                            p = vec3_apply_mat4(p, 1, model_mat);
                            vec3 n = section->normals.data[i];
                            n = vec3_normalize(vec3_apply_mat4(n, 0, mat4_transpose(mat4_inverse(model_mat))));
                            vec_push(&entity->positions, p);
                            vec_push(&entity->normals, n);
                        }
                    }

                    glBindBuffer(GL_ARRAY_BUFFER, entity->gl_position_vbo);
                    glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * entity->positions.length, entity->positions.data, GL_STATIC_DRAW);
                }

                for (int i = 0; i < image_width * image_height; i++) {
                    image_data[s][i] = 0.0f;
                }

                lmSetTargetLightmap(ctx, image_data[s], image_width, image_height, 1);
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
                        glBindTexture(GL_TEXTURE_2D, entity->gl_tex[s]);

                        glBindBuffer(GL_ARRAY_BUFFER, entity->gl_position_vbo);
                        glVertexAttribPointer(position_id, 3, GL_FLOAT, GL_FALSE, 0, NULL);
                        glEnableVertexAttribArray(position_id);

                        glBindBuffer(GL_ARRAY_BUFFER, entity->gl_lightmap_uv_vbo);
                        glVertexAttribPointer(texture_coord_id, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                        glEnableVertexAttribArray(texture_coord_id);

                        glBindBuffer(GL_ARRAY_BUFFER, entity->gl_ignore_vbo);
                        glVertexAttribPointer(ignore_id, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                        glEnableVertexAttribArray(ignore_id);

                        glBindVertexArray(dummy_vao);

                        glDrawArrays(GL_TRIANGLES, 0, entity->positions.length);
                    }

                    lmEnd(ctx);
                }
            }
        }

        for (int i = 0; i < gi->entities.length; i++) {
            golf_gi_entity_t *entity = &gi->entities.data[i];
            int image_width = entity->image_width;
            int image_height = entity->image_height;
            int num_samples = entity->num_samples;
            float **image_data = entity->image_data;
            for (int s = 0; s < num_samples; s++) {
                glBindTexture(GL_TEXTURE_2D, entity->gl_tex[s]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_FLOAT, image_width, image_height, 0, GL_RED, GL_FLOAT, image_data[s]);
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }
    }

    for (int i = 0; i < gi->entities.length; i++) {
        golf_gi_entity_t *entity = &gi->entities.data[i];
        int image_width = entity->image_width;
        int image_height = entity->image_height;
        int num_samples = entity->num_samples;
        float **image_data = entity->image_data;
        float *temp = malloc(sizeof(float) * image_width * image_height);
        for (int s = 0;  s < num_samples; s++) {
            for (int i = 0; i < gi->num_dilates; i++) {
                lmImageDilate(image_data[s], temp, image_width, image_height, 1);
                lmImageDilate(temp, image_data[s], image_width, image_height, 1);
            }
            for (int i = 0; i < gi->num_smooths; i++) {
                lmImageSmooth(image_data[s], temp, image_width, image_height, 1);
                lmImageSmooth(temp, image_data[s], image_width, image_height, 1);
            }
            lmImagePower(image_data[s], image_width, image_height, 1, gi->gamma, LM_ALL_CHANNELS);
        }
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

    return GOLF_THREAD_RESULT_SUCCESS;
}

void golf_gi_start(golf_gi_t *gi) {
    if (golf_gi_is_running(gi)) {
        golf_log_error("Cannot start gi when it's already running.");
    }

    _gi_set_is_running(gi, true);
    gi->thread = golf_thread_create(_gi_run, gi, "gi");
}

void golf_gi_start_lightmap(golf_gi_t *gi, golf_lightmap_image_t *lightmap_image) {
    if (gi->has_cur_entity) {
        return;
    }

    golf_gi_entity_t entity;
    vec_init(&entity.positions, "gi");
    vec_init(&entity.normals, "gi");
    vec_init(&entity.lightmap_uvs, "gi");
    vec_init(&entity.ignore, "gi");
    vec_init(&entity.gi_lightmap_sections, "gi");

    entity.time_length = lightmap_image->time_length;
    entity.repeats = lightmap_image->repeats;
    entity.num_samples = lightmap_image->num_samples;
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

void golf_gi_add_lightmap_section(golf_gi_t *gi, golf_lightmap_section_t *lightmap_section, golf_model_t *model, golf_transform_t transform, golf_movement_t movement, bool should_draw) {
    if (!gi->has_cur_entity) {
        return;
    }

    golf_gi_lightmap_section_t section;
    vec_init(&section.positions, "gi");
    vec_init(&section.normals, "gi");
    vec_init(&section.lightmap_uvs, "gi");
    vec_pusharr(&section.positions, model->positions.data, model->positions.length);
    vec_pusharr(&section.normals, model->normals.data, model->normals.length);
    for (int i = 0; i < model->positions.length; i++) {
        vec_push(&section.lightmap_uvs, V2(0, 0));
    }
    section.transform = transform;
    section.movement = movement;
    section.lightmap_section = lightmap_section;
    section.should_draw = should_draw;
    vec_push(&gi->cur_entity.gi_lightmap_sections, section);
}

void golf_gi_deinit(golf_gi_t *gi) {
    for (int i = 0; i < gi->entities.length; i++) {
        golf_gi_entity_t *entity = &gi->entities.data[i];
        for (int i = 0; i < entity->gi_lightmap_sections.length; i++) {
            golf_gi_lightmap_section_t *section = &entity->gi_lightmap_sections.data[i];
            vec_deinit(&section->positions);
            vec_deinit(&section->normals);
            vec_deinit(&section->lightmap_uvs);
        }
        vec_deinit(&entity->gi_lightmap_sections);
        vec_deinit(&entity->positions);
        vec_deinit(&entity->normals);
        vec_deinit(&entity->lightmap_uvs);
        vec_deinit(&entity->ignore);
    }
    vec_deinit(&gi->entities);
    golf_thread_join(gi->thread);
    golf_thread_destroy(gi->thread);
}

int golf_gi_get_lm_gen_progress(golf_gi_t *gi) {
    golf_mutex_lock(&gi->lock);
    int lm_gen_progress = gi->lm_gen_progress;
    golf_mutex_unlock(&gi->lock);
    return lm_gen_progress;
}

int golf_gi_get_uv_gen_progress(golf_gi_t *gi) {
    golf_mutex_lock(&gi->lock);
    int uv_gen_progress = gi->uv_gen_progress;
    golf_mutex_unlock(&gi->lock);
    return uv_gen_progress;
}

bool golf_gi_is_running(golf_gi_t *gi) {
    golf_mutex_lock(&gi->lock);
    bool is_running = gi->is_running;
    golf_mutex_unlock(&gi->lock);
    return is_running;
}
