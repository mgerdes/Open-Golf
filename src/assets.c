#define _CRT_SECURE_NO_WARNINGS

#include "assets.h"

#include <assert.h>
#include <stdbool.h>

#include "array.h"
#include "file.h"
#include "map.h"
#include "profiler.h"
#include "stb_image.h"

//
// Model Asset
//
static void model_create_buffers(struct model *model) {
    sg_buffer_desc desc = {
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
    };

    desc.size = sizeof(vec3) * model->num_points;
    desc.content = model->positions;
    model->positions_buf = sg_make_buffer(&desc);

    desc.size = sizeof(vec3) * model->num_points;
    desc.content = model->normals;
    model->normals_buf = sg_make_buffer(&desc);

    desc.size = sizeof(vec2) * model->num_points;
    desc.content = model->texture_coords;
    model->texture_coords_buf = sg_make_buffer(&desc);
}

//
// Texture asset
//
static bool texture_create(struct file *file, struct texture *texture) {
    int x, y, n;
    int force_channels = 4;
    stbi_set_flip_vertically_on_load(0);
    if (!file_load_data(file)) {
        return false;
    }
    unsigned char *tex_data = stbi_load_from_memory((unsigned char*) file->data, file->data_len, &x, &y, &n,
            force_channels);
    file_delete_data(file);
    assert(tex_data);

    sg_filter filter = SG_FILTER_LINEAR;
    if (strcmp(file->name, "UIpackSheet_transparent.png") == 0) {
        filter = SG_FILTER_NEAREST;
    }

    sg_image_desc img_desc = {
        .width = x,
        .height = y,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = filter,
        .mag_filter = filter,
        .wrap_u = SG_WRAP_REPEAT,
        .wrap_v = SG_WRAP_REPEAT,
        .content.subimage[0][0] = {
            .ptr = tex_data,
            .size = sizeof(char)*x*y,
        },
    };

    strcpy(texture->name, file->name);
    texture->data = tex_data;
    texture->width = x;
    texture->height = y;
    texture->image = sg_make_image(&img_desc);
    return true;
}

typedef map_t(struct model) map_model_t;
typedef map_t(struct texture) map_texture_t;
typedef map_t(struct shader) map_shader_t;

struct asset_store {
    map_model_t models;
    map_texture_t textures;
    map_shader_t shaders;
};
static struct asset_store *_store = NULL;

static void line_parse_floats(const char *line, float *vals, int num_vals) {
    for (int i = 0; i < num_vals; i++) {
        vals[i] = (float) atof(line);
        if (i != num_vals - 1) {
            while (*(line++) != ' ');
        }
    }
}

static void init_models(void) {
    {
        int res = 10;
        int num_points = 12 * res;
        vec3 *positions = malloc(sizeof(vec3) * num_points);
        vec3 *normals = malloc(sizeof(vec3) * num_points);
        vec2 *texture_coords = malloc(sizeof(vec2) * num_points);

        int k = 0;
        for (int i = 0; i < res; i++) {
            float theta0 = 2.0f * MF_PI * (float) i / res;
            float theta1 = 2.0f * MF_PI * (float) (i + 1) / res;

            vec3 p0 = V3(0.0f, 0.5f, 0.0f);
            vec3 p1 = V3(cosf(theta1), 0.5f, sinf(theta1));
            vec3 p2 = V3(cosf(theta0), 0.5f, sinf(theta0));
            vec3 p3 = V3(0.0f, -0.5f, 0.0f);
            vec3 p4 = V3(cosf(theta1), -0.5f, sinf(theta1));
            vec3 p5 = V3(cosf(theta0), -0.5f, sinf(theta0));

            vec3 n0 = vec3_normalize(vec3_cross(vec3_sub(p2, p0), vec3_sub(p1, p0)));
            positions[k + 0] = p0;
            positions[k + 1] = p1;
            positions[k + 2] = p2;
            normals[k + 0] = n0;
            normals[k + 1] = n0;
            normals[k + 2] = n0;

            vec3 n1 = vec3_normalize(vec3_cross(vec3_sub(p5, p3), vec3_sub(p4, p3)));
            positions[k + 3] = p3;
            positions[k + 4] = p4;
            positions[k + 5] = p5;
            normals[k + 3] = n1;
            normals[k + 4] = n1;
            normals[k + 5] = n1;

            vec3 n2 = vec3_normalize(vec3_cross(vec3_sub(p2, p1), vec3_sub(p5, p1)));
            positions[k + 6] = p1;
            positions[k + 7] = p5;
            positions[k + 8] = p2;
            normals[k + 6] = n2;
            normals[k + 7] = n2;
            normals[k + 8] = n2;

            vec3 n3 = vec3_normalize(vec3_cross(vec3_sub(p5, p1), vec3_sub(p4, p1)));
            positions[k + 9] = p1;
            positions[k + 10] = p4;
            positions[k + 11] = p5;
            normals[k + 9] = n3;
            normals[k + 10] = n3;
            normals[k + 11] = n3;

            texture_coords[k + 0] = V2(0.0f, 0.0f);
            texture_coords[k + 1] = V2(0.0f, 0.0f);
            texture_coords[k + 2] = V2(0.0f, 0.0f);
            texture_coords[k + 3] = V2(0.0f, 0.0f);
            texture_coords[k + 4] = V2(0.0f, 0.0f);
            texture_coords[k + 5] = V2(0.0f, 0.0f);
            texture_coords[k + 6] = V2(0.0f, 0.0f);
            texture_coords[k + 7] = V2(0.0f, 0.0f);
            texture_coords[k + 8] = V2(0.0f, 0.0f);
            texture_coords[k + 9] = V2(0.0f, 0.0f);
            texture_coords[k + 10] = V2(0.0f, 0.0f);
            texture_coords[k + 11] = V2(0.0f, 0.0f);

            k += 12;
        }

        const char *name = "cylinder";
        struct model model;
        strcpy(model.name, name);
        model.num_points = num_points;
        model.positions = positions;
        model.normals = normals;
        model.texture_coords = texture_coords;
        model_create_buffers(&model);
        map_set(&_store->models, name, model);
    }

    //
    //    2
    //  0   1
    //
    {
        int res = 32;
        int num_points = 6 * res;
        vec3 *positions = malloc(sizeof(vec3) * num_points);
        vec3 *normals = malloc(sizeof(vec3) * num_points);
        vec2 *texture_coords = malloc(sizeof(vec2) * num_points);

        int k = 0;
        for (int i = 0; i < res; i++) {
            float theta0 = 2.0f * MF_PI * ((float) i) / res;
            float theta1 = 2.0f * MF_PI * ((float) (i + 1)) / res;

            vec3 p0 = V3(cosf(theta0), 0.0f, sinf(theta0));
            vec3 p1 = V3(cosf(theta1), 0.0f, sinf(theta1));
            vec3 p2 = V3(0.0f, 2.0f, 0.0f);
            vec3 p3 = V3(0.0f, 0.0f, 0.0f);

            positions[k + 0] = p1;
            positions[k + 1] = p0;
            positions[k + 2] = p2;

            vec3 n0 = vec3_normalize(vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0)));
            normals[k + 0] = n0;
            normals[k + 1] = n0;
            normals[k + 2] = n0;

            positions[k + 3] = p0;
            positions[k + 4] = p1;
            positions[k + 5] = p3;

            vec3 n1 = vec3_normalize(vec3_cross(vec3_sub(p1, p0), vec3_sub(p3, p0)));
            normals[k + 3] = n1;
            normals[k + 4] = n1;
            normals[k + 5] = n1;

            texture_coords[k + 0] = V2(0.0f, 0.0f);
            texture_coords[k + 1] = V2(0.0f, 0.0f);
            texture_coords[k + 2] = V2(0.0f, 0.0f);
            texture_coords[k + 3] = V2(0.0f, 0.0f);
            texture_coords[k + 4] = V2(0.0f, 0.0f);
            texture_coords[k + 5] = V2(0.0f, 0.0f);

            k += 6;
        }

        const char *name = "cone";
        struct model model;
        strcpy(model.name, name);
        model.num_points = num_points;
        model.positions = positions;
        model.normals = normals;
        model.texture_coords = texture_coords;
        model_create_buffers(&model);
        map_set(&_store->models, name, model);
    }

    {
        int num_points = 6;
        vec3 *positions = malloc(sizeof(vec3) * num_points);
        vec3 *normals = malloc(sizeof(vec3) * num_points);
        vec2 *texture_coords = malloc(sizeof(vec2) * num_points);

        positions[0] = V3(-1.0f, -1.0f, 0.0f);
        positions[1] = V3(1.0f, -1.0f, 0.0f);
        positions[2] = V3(1.0f, 1.0f, 0.0f);
        positions[3] = V3(-1.0f, -1.0f, 0.0f);
        positions[4] = V3(1.0f, 1.0f, 0.0f);
        positions[5] = V3(-1.0f, 1.0f, 0.0f);

        normals[0] = V3(0.0f, 0.0f, 1.0f);
        normals[1] = V3(0.0f, 0.0f, 1.0f);
        normals[2] = V3(0.0f, 0.0f, 1.0f);
        normals[3] = V3(0.0f, 0.0f, 1.0f);
        normals[4] = V3(0.0f, 0.0f, 1.0f);
        normals[5] = V3(0.0f, 0.0f, 1.0f);

        texture_coords[0] = V2(0.0f, 0.0f);
        texture_coords[1] = V2(1.0f, 0.0f);
        texture_coords[2] = V2(1.0f, 1.0f);
        texture_coords[3] = V2(0.0f, 0.0f);
        texture_coords[4] = V2(1.0f, 1.0f);
        texture_coords[5] = V2(0.0f, 1.0f);

        const char *name = "square";
        struct model model;
        strcpy(model.name, name);
        model.num_points = num_points;
        model.positions = positions;
        model.normals = normals;
        model.texture_coords = texture_coords;
        model_create_buffers(&model);
        map_set(&_store->models, name, model);
    }

	struct directory dir;
    directory_init(&dir, "assets/models");
    for (int i = 0; i < dir.num_files; i++) {
        struct file file = dir.files[i];
        if (strcmp(file.ext, ".model") != 0) {
            continue;
        }
        if (!file_load_data(&file)) {
            continue;
        }

        int line_buffer_len = 1024;
        char *line_buffer = malloc(line_buffer_len);

        file_copy_line(&file, &line_buffer, &line_buffer_len); 
        char model_name[FILES_MAX_FILENAME];
        strcpy(model_name, line_buffer);

        file_copy_line(&file, &line_buffer, &line_buffer_len);
        int num_materials = atoi(line_buffer);

        for (int j = 0; j < num_materials; j++) {
            file_copy_line(&file, &line_buffer, &line_buffer_len);
            float vals[9];
            line_parse_floats(line_buffer, vals, 9);

            file_copy_line(&file, &line_buffer, &line_buffer_len);
        }

        file_copy_line(&file, &line_buffer, &line_buffer_len);
        int num_shapes = atoi(line_buffer);
        assert(num_shapes == 1);

        file_copy_line(&file, &line_buffer, &line_buffer_len);
        int num_points = atoi(line_buffer);
        vec3 *positions = malloc(sizeof(vec3) * num_points);
        vec3 *normals = malloc(sizeof(vec3) * num_points);
        vec2 *texture_coords = malloc(sizeof(vec2) * num_points);
        for (int j = 0; j < num_points; j++) {
            file_copy_line(&file, &line_buffer, &line_buffer_len);

            float vals[11];
            line_parse_floats(line_buffer, vals, 11);
            positions[j] = V3(vals[0], vals[1], vals[2]);
            normals[j] = V3(vals[3], vals[4], vals[5]);
            texture_coords[j] = V2(vals[6], vals[7]);
        }

        struct model model;
        strcpy(model.name, model_name);
        model.num_points = num_points;
        model.positions = positions;
        model.normals = normals;
        model.texture_coords = texture_coords;
        model_create_buffers(&model);
        map_set(&_store->models, model_name, model);

        file_delete_data(&file);
    }
    for (int i = 0; i < dir.num_files; i++) {
        struct file file = dir.files[i];
        if (strcmp(file.ext, ".terrain_model") != 0) {
            continue;
        }
        if (!file_load_data(&file)) {
            continue;
        }

        struct array_vec3 positions, normals;
        struct array_vec2 texture_coords;
        array_init(&positions);
        array_init(&normals);
        array_init(&texture_coords);

        char *line_buf = NULL;
        int line_buf_len = 0;
        while (file_copy_line(&file, &line_buf, &line_buf_len)) {
            vec3 p, n;
            vec2 tc;
            if(sscanf(line_buf, "%f %f %f %f %f %f %f %f",
                        &p.x, &p.y, &p.z, &n.x, &n.y, &n.z, &tc.x, &tc.y) == 8) {
                array_push(&positions, p);
                array_push(&normals, n);
                array_push(&texture_coords, tc);
            }
        }
        free(line_buf);

        struct model model;
        strcpy(model.name, file.name);
        model.num_points = positions.length;
        model.positions = malloc(sizeof(vec3) * model.num_points);
        model.normals = malloc(sizeof(vec3) * model.num_points);
        model.texture_coords = malloc(sizeof(vec2) * model.num_points);
        memcpy(model.positions, positions.data, sizeof(vec3) * model.num_points);
        memcpy(model.normals, normals.data, sizeof(vec3) * model.num_points);
        memcpy(model.texture_coords, texture_coords.data, sizeof(vec2) * model.num_points);
        model_create_buffers(&model);
        map_set(&_store->models, file.name, model);

        array_deinit(&positions);
        array_deinit(&normals);
        array_deinit(&texture_coords);

        file_delete_data(&file);
    }
    directory_deinit(&dir);
}

void asset_store_init(void) {
    profiler_push_section("asset_store_init");
    _store = malloc(sizeof(struct asset_store));

    //
    // Init models
    //
    {
        map_init(&_store->models);
        init_models();
    }

    //
    // Init textures
    //
    {
        map_init(&_store->textures);

		struct directory dir;
		directory_init(&dir, "assets/textures");
        for (int i = 0; i < dir.num_files; i++) {
            struct file file = dir.files[i];

            struct texture texture;
            bool ret = texture_create(&file, &texture);
            assert(ret);

            map_set(&_store->textures, texture.name, texture);
        }
		directory_deinit(&dir);
    }

    profiler_pop_section();
}

struct model *asset_store_get_model(const char *name) {
    struct model *model = map_get(&_store->models, name);
    if (model) {
        return model;
    }
    else {
        model = map_get(&_store->models, "cube");
        assert(model);
        return model;
    }
}

struct texture *asset_store_get_texture(const char *name) {
    struct texture *texture = map_get(&_store->textures, name);
    assert(texture);
    return texture;
}
