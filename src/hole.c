#define _CRT_SECURE_NO_WARNINGS

#include "hole.h"

#include "data_stream.h"
#include "lightmapper.h"
#include "maths.h"
#include "profiler.h"
#include "rnd.h"
#include "stb_image_write.h"
#include "xatlas_wrapper.h"

void lightmap_init(struct lightmap *lightmap, int width, int height, int num_elements, int num_images) {
    assert(num_images >= 1);

    lightmap->width = width;
    lightmap->height = height;
    array_init(&lightmap->images);
    array_init(&lightmap->uvs);

    for (int i = 0; i < num_images; i++) {
        struct lightmap_image image;

        image.data = malloc(sizeof(unsigned char) * width * height);
        for (int i = 0; i < width * height; i++) {
            image.data[i] = 0;
        }

        sg_image_desc img_desc = {
            .width = lightmap->width,
            .height = lightmap->height,
            .pixel_format = SG_PIXELFORMAT_R8,
            .usage = SG_USAGE_DYNAMIC,
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        };
        image.sg_image = sg_make_image(&img_desc);

        array_push(&lightmap->images, image);
    }

    {
        if (num_elements == 0) {
            num_elements = 64;
        }

        lightmap->buf_len = num_elements;
        sg_buffer_desc desc = {
            .type = SG_BUFFERTYPE_VERTEXBUFFER,
            .usage = SG_USAGE_DYNAMIC,
        };

        desc.size = sizeof(vec2) * num_elements,
        lightmap->uvs_buf = sg_make_buffer(&desc);
    }
}

void lightmap_deinit(struct lightmap *lightmap) {
    for (int i = 0; i < lightmap->images.length; i++) {
        struct lightmap_image *image = &lightmap->images.data[i];
        free(image->data);
        sg_destroy_image(image->sg_image);
    }
    sg_destroy_buffer(lightmap->uvs_buf);
    array_deinit(&lightmap->images);
    array_deinit(&lightmap->uvs);
}

void lightmap_resize(struct lightmap *lightmap, int width, int height, int num_images) {
    if (width != lightmap->width || height != lightmap->height || num_images != lightmap->images.length) {
        lightmap->width = width;
        lightmap->height = height;
        for (int i = 0; i < lightmap->images.length; i++) {
            struct lightmap_image *image = &lightmap->images.data[i];
            free(image->data);
            sg_destroy_image(image->sg_image);
        }
        lightmap->images.length = 0;
        for (int i = 0; i < num_images; i++) {
            struct lightmap_image image;
            image.data = malloc(sizeof(unsigned char) * width * height);
            sg_image_desc img_desc = {
                .width = lightmap->width,
                .height = lightmap->height,
                .pixel_format = SG_PIXELFORMAT_R8,
                .usage = SG_USAGE_DYNAMIC,
                .min_filter = SG_FILTER_LINEAR,
                .mag_filter = SG_FILTER_LINEAR,
                .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
                .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
            };
            image.sg_image = sg_make_image(&img_desc);
            array_push(&lightmap->images, image);
        }
    }
}

void lightmap_update_image(struct lightmap *lightmap) {
    for (int i = 0; i < lightmap->images.length; i++) {
        struct lightmap_image *image = &lightmap->images.data[i];
        sg_image_content content = {
            .subimage[0][0] = {
                .ptr = image->data,
                .size = sizeof(unsigned char) * lightmap->width * lightmap->height,
            },
        };
        sg_update_image(image->sg_image, &content);
    }
}

void lightmap_update_uvs_buffer(struct lightmap *lightmap, int num_elements) {
    while (lightmap->uvs.length < num_elements) {
        array_push(&lightmap->uvs, V2(0.0f, 0.0f));
    }
    if (lightmap->uvs.length > num_elements) {
        lightmap->uvs.length = num_elements;
    }

    if (lightmap->uvs.length > lightmap->buf_len) {
        sg_destroy_buffer(lightmap->uvs_buf);

        lightmap->buf_len = 2 * lightmap->uvs.length;
        sg_buffer_desc desc = {
            .type = SG_BUFFERTYPE_VERTEXBUFFER,
            .usage = SG_USAGE_DYNAMIC,
        };

        desc.size = sizeof(vec2) * lightmap->buf_len;
        lightmap->uvs_buf = sg_make_buffer(&desc);
    }

    sg_update_buffer(lightmap->uvs_buf, lightmap->uvs.data, sizeof(vec2) * lightmap->uvs.length);
}

void lightmap_save_image(struct lightmap *lightmap, const char *filename) {
    stbi_write_png(filename, lightmap->width, lightmap->height, 1, lightmap->images.data[0].data, 0);
}

struct terrain_model_face create_terrain_model_face(int num_points, int mat_idx, int smooth_normal,
        int x, int y, int z, int w, vec2 tc0, vec2 tc1, vec2 tc2, vec2 tc3, float texture_coord_scale,
        float cor, float friction, float vel_scale, enum terrain_model_auto_texture auto_texture) {
    struct terrain_model_face face;
    face.auto_texture = auto_texture;
    face.mat_idx = mat_idx;
    face.smooth_normal = smooth_normal;
    face.texture_coords[0] = tc0;
    face.texture_coords[1] = tc1;
    face.texture_coords[2] = tc2;
    face.texture_coords[3] = tc3;
    face.texture_coord_scale = texture_coord_scale;
    face.num_points = num_points;
    face.x = x;
    face.y = y;
    face.z = z;
    face.w = w;
    face.cor = cor;
    face.friction = friction;
    face.vel_scale = vel_scale;
    return face;
}

void terrain_model_init(struct terrain_model *model, int num_elements) {
    model->materials[0].color0 = V3(0.0f, 0.0f, 0.0f);
    model->materials[0].color1 = V3(0.0f, 0.0f, 0.0f);
    model->materials[1].color0 = V3(0.0f, 0.0f, 0.0f);
    model->materials[1].color1 = V3(0.0f, 0.0f, 0.0f);
    model->materials[2].color0 = V3(0.0f, 0.0f, 0.0f);
    model->materials[2].color1 = V3(0.0f, 0.0f, 0.0f);
    model->materials[3].color0 = V3(0.0f, 0.0f, 0.0f);
    model->materials[3].color1 = V3(0.0f, 0.0f, 0.0f);
    model->materials[4].color0 = V3(0.0f, 0.0f, 0.0f);
    model->materials[4].color1 = V3(0.0f, 0.0f, 0.0f);
    array_init(&model->points);
    array_init(&model->faces);
    model->generator_name[0] = 0;
    map_init(&model->generator_params);
    model->num_elements = 0;

    {
        if (num_elements == 0) {
            num_elements = 64;
        }

        model->buf_len = num_elements;
        sg_buffer_desc desc = {
            .type = SG_BUFFERTYPE_VERTEXBUFFER,
            .usage = SG_USAGE_DYNAMIC,
        };

        desc.size = sizeof(vec3) * model->buf_len;
        model->positions_buf = sg_make_buffer(&desc);

        desc.size = sizeof(vec3) * model->buf_len;
        model->normals_buf = sg_make_buffer(&desc);

        desc.size = sizeof(vec2) * model->buf_len;
        model->texture_coords_buf = sg_make_buffer(&desc);

        desc.size = sizeof(float) * model->buf_len;
        model->material_idxs_buf = sg_make_buffer(&desc);
    }
}

void terrain_model_deinit(struct terrain_model *model) {
    array_deinit(&model->points);
    array_deinit(&model->faces);
    sg_destroy_buffer(model->positions_buf);
    sg_destroy_buffer(model->normals_buf);
    sg_destroy_buffer(model->texture_coords_buf);
    sg_destroy_buffer(model->material_idxs_buf);
}

void terrain_model_copy(struct terrain_model *model, struct terrain_model *model_to_copy) {
    memcpy(model->materials, model_to_copy->materials, 
            MAX_NUM_TERRAIN_MODEL_MATERIALS*sizeof(struct terrain_model_material));
    model->points.length = 0;
    array_extend(&model->points, &model_to_copy->points);
    model->faces.length = 0;
    array_extend(&model->faces, &model_to_copy->faces);

    const char *key;
    map_iter_t iter = map_iter(&model_to_copy->generator_params);
    while (key = map_next(&model_to_copy->generator_params, &iter)) {
        float *val = map_get(&model_to_copy->generator_params, key); 
        assert(val);
        map_set(&model->generator_params, key, *val);
    }
}

int terrain_model_add_point(struct terrain_model *model, vec3 point, int idx) {
    if (idx == -1) {
        array_push(&model->points, point);
        return model->points.length - 1;
    }
    else {
        assert(idx >= 0 && idx <= model->points.length);
        array_insert(&model->points, idx, point);

        for (int i = 0; i < model->faces.length; i++) {
            struct terrain_model_face *face = &model->faces.data[i];
            assert(face->num_points == 3 || face->num_points == 4);
            if (face->x >= idx) face->x++;
            if (face->y >= idx) face->y++;
            if (face->z >= idx) face->z++;
            if (face->num_points == 4) {
                if (face->w >= idx) face->w++;
            }
        }
        return idx;
    }
}

vec3 terrain_model_get_point(struct terrain_model *model, int point_idx) {
    assert(point_idx >= 0 && point_idx < model->points.length);
    return model->points.data[point_idx];
}

int terrain_model_get_point_idx(struct terrain_model *model, vec3 *point) {
    int point_idx = (int) (point - model->points.data);
    assert(point_idx >= 0 && point_idx < model->points.length);
    return point_idx;
}

void terrain_model_delete_point(struct terrain_model *model, int point_idx) {
    assert(point_idx >= 0 && point_idx < model->points.length);
    array_splice(&model->points, point_idx, 1);
    for (int i = 0; i < model->faces.length; i++) {
        struct terrain_model_face *face = &model->faces.data[i];
        if (face->num_points == 3) {
            if (face->x == point_idx || face->y == point_idx || face->z == point_idx) {
                assert(false);
            }
            else {
                if (face->x > point_idx) face->x--;
                if (face->y > point_idx) face->y--;
                if (face->z > point_idx) face->z--;
            }
        }
        else if (face->num_points == 4) {
            if (face->x == point_idx || face->y == point_idx || face->z == point_idx || face->w == point_idx) {
                assert(false);
            }
            else {
                if (face->x > point_idx) face->x--;
                if (face->y > point_idx) face->y--;
                if (face->z > point_idx) face->z--;
                if (face->w > point_idx) face->w--;
            }
        }
        else {
            assert(false);
        }
    }
}

int terrain_model_add_face(struct terrain_model *model, struct terrain_model_face face, int idx) {
    if (face.num_points == 3) {
        if (face.x < 0 || face.x >= model->points.length ||
                face.y < 0 || face.y >= model->points.length ||
                face.z < 0 || face.z >= model->points.length) {
            return -1;
        }
    }
    else if (face.num_points == 4) {
        if (face.x < 0 || face.x >= model->points.length ||
                face.y < 0 || face.y >= model->points.length ||
                face.z < 0 || face.z >= model->points.length ||
                face.w < 0 || face.w >= model->points.length) {
            return -1;
        }
    }
    else {
        return -1;
    }

    if (idx == -1) {
        array_push(&model->faces, face);
        return model->faces.length - 1;
    }
    else {
        assert(idx >= 0 && idx <= model->faces.length);
        array_insert(&model->faces, idx, face);
        return idx;
    }
}

struct terrain_model_face terrain_model_get_face(struct terrain_model *model, int face_idx) {
    assert(face_idx >= 0 && face_idx < model->faces.length);
    return model->faces.data[face_idx];
}

int terrain_model_get_face_idx(struct terrain_model *model, struct terrain_model_face *face) {
    int face_idx = (int) (face - model->faces.data);
    assert(face_idx >= 0 && face_idx < model->faces.length);
    return face_idx;
}

void terrain_model_delete_face(struct terrain_model *model, int face_idx) {
    assert(face_idx >= 0 && face_idx < model->faces.length);
    array_splice(&model->faces, face_idx, 1);
}

static vec3 terrain_model_point_smooth_normal(struct terrain_model *model, int idx) {
    vec3 n = V3(0.0f, 0.0f, 0.0f);
    int count = 0;
    for (int i = 0; i < model->faces.length; i++) {
        struct terrain_model_face face = model->faces.data[i];
        if (!face.smooth_normal) {
            continue;
        }

        if (face.num_points == 3) {
            if (face.x == idx || face.y == idx || face.z == idx) {
                vec3 p0 = model->points.data[face.x];
                vec3 p1 = model->points.data[face.y];
                vec3 p2 = model->points.data[face.z];

                vec3 face_normal = vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0));
                n = vec3_add(n, face_normal);
                count++;
            }
        }
        else if (face.num_points == 4) {
            if (face.x == idx || face.y == idx || face.z == idx || face.w == idx) {
                vec3 p0 = model->points.data[face.x];
                vec3 p1 = model->points.data[face.y];
                vec3 p2 = model->points.data[face.z];
                vec3 p3 = model->points.data[face.w];

                vec3 face_normal0 = vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0));
                vec3 face_normal1 = vec3_cross(vec3_sub(p3, p2), vec3_sub(p0, p2));
                vec3 face_normal = vec3_scale(vec3_add(face_normal0, face_normal1), 0.5f);
                n = vec3_add(n, face_normal);
                count++;
            }
        }
        else {
            assert(false);
        }
    }
    assert(count > 0);
    n = vec3_normalize(vec3_scale(n, 1.0f / count));
    return n;
}

void terrain_model_generate_triangle_data(struct terrain_model *model,
        struct array_vec3 *positions, struct array_vec3 *normals, struct array_vec2 *texture_coords,
        struct array_float *material_idx) {
    for (int i = 0; i < model->faces.length; i++) {
        struct terrain_model_face face = model->faces.data[i];
        if (face.num_points == 3) {
            vec3 p0 = model->points.data[face.x];
            vec3 p1 = model->points.data[face.y];
            vec3 p2 = model->points.data[face.z];
            array_push(positions, p0);
            array_push(positions, p1);
            array_push(positions, p2);
            if (face.smooth_normal) {
                vec3 n0 = terrain_model_point_smooth_normal(model, face.x);
                vec3 n1 = terrain_model_point_smooth_normal(model, face.y);
                vec3 n2 = terrain_model_point_smooth_normal(model, face.z);
                array_push(normals, n0);
                array_push(normals, n1);
                array_push(normals, n2);
            }
            else {
                vec3 n = vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0));
                array_push(normals, n);
                array_push(normals, n);
                array_push(normals, n);
            }

            array_push(texture_coords, vec2_scale(face.texture_coords[0], face.texture_coord_scale));
            array_push(texture_coords, vec2_scale(face.texture_coords[1], face.texture_coord_scale));
            array_push(texture_coords, vec2_scale(face.texture_coords[2], face.texture_coord_scale));
            array_push(material_idx, (float) face.mat_idx);
            array_push(material_idx, (float) face.mat_idx);
            array_push(material_idx, (float) face.mat_idx);
        }
        else if (face.num_points == 4) {
            vec3 p0 = model->points.data[face.x];
            vec3 p1 = model->points.data[face.y];
            vec3 p2 = model->points.data[face.z];
            vec3 p3 = model->points.data[face.w];
            array_push(positions, p0);
            array_push(positions, p1);
            array_push(positions, p2);
            array_push(positions, p2);
            array_push(positions, p3);
            array_push(positions, p0);
            if (face.smooth_normal) {
                vec3 n0 = terrain_model_point_smooth_normal(model, face.x);
                vec3 n1 = terrain_model_point_smooth_normal(model, face.y);
                vec3 n2 = terrain_model_point_smooth_normal(model, face.z);
                vec3 n3 = terrain_model_point_smooth_normal(model, face.w);
                array_push(normals, n0);
                array_push(normals, n1);
                array_push(normals, n2);
                array_push(normals, n2);
                array_push(normals, n3);
                array_push(normals, n0);
            }
            else {
                vec3 n0 = vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0));
                vec3 n1 = vec3_cross(vec3_sub(p3, p2), vec3_sub(p0, p2));
                array_push(normals, n0);
                array_push(normals, n0);
                array_push(normals, n0);
                array_push(normals, n1);
                array_push(normals, n1);
                array_push(normals, n1);
            }

            vec2 tc0 = face.texture_coords[0];
            vec2 tc1 = face.texture_coords[1];
            vec2 tc2 = face.texture_coords[2];
            vec2 tc3 = face.texture_coords[3];
            if (face.auto_texture == AUTO_TEXTURE_WOOD_OUT) {
                int top_left, top_right, bot_left, bot_right;
                if (p0.y >= p1.y && p0.y >= p2.y && p0.y >= p3.y) {
                    if (fabsf(vec3_dot(vec3_sub(p0, p1), V3(0.0f, 1.0f, 0.0f))) > 
                            fabsf(vec3_dot(vec3_sub(p0, p3), V3(0.0f, 1.0f, 0.0f)))) {
                        top_left = 0;
                        bot_left = 1;
                        bot_right = 2;
                        top_right = 3;
                    }
                    else {
                        top_right = 0;
                        top_left = 1;
                        bot_left = 2;
                        bot_right = 3;
                    }
                }
                else if (p1.y >= p0.y && p1.y >= p2.y && p1.y >= p3.y) {
                    if (fabsf(vec3_dot(vec3_sub(p1, p2), V3(0.0f, 1.0f, 0.0f))) > 
                            fabsf(vec3_dot(vec3_sub(p1, p0), V3(0.0f, 1.0f, 0.0f)))) {
                        top_left = 1;
                        bot_left = 2;
                        bot_right = 3;
                        top_right = 0;
                    }
                    else {
                        top_right = 1;
                        top_left = 2;
                        bot_left = 3;
                        bot_right = 0;
                    }
                }
                else if (p2.y >= p0.y && p2.y >= p1.y && p2.y >= p3.y) {
                    if (fabsf(vec3_dot(vec3_sub(p2, p3), V3(0.0f, 1.0f, 0.0f))) > 
                            fabsf(vec3_dot(vec3_sub(p3, p1), V3(0.0f, 1.0f, 0.0f)))) {
                        top_left = 2;
                        bot_left = 3;
                        bot_right = 0;
                        top_right = 1;
                    }
                    else {
                        top_right = 2;
                        top_left = 3;
                        bot_left = 0;
                        bot_right = 1;
                    }
                }
                else {
                    if (fabsf(vec3_dot(vec3_sub(p3, p0), V3(0.0f, 1.0f, 0.0f))) > 
                            fabsf(vec3_dot(vec3_sub(p3, p2), V3(0.0f, 1.0f, 0.0f)))) {
                        top_left = 3;
                        bot_left = 0;
                        bot_right = 1;
                        top_right = 2;
                    }
                    else {
                        top_right = 3;
                        top_left = 0;
                        bot_left = 1;
                        bot_right = 2;
                    }
                }

                vec3 ps[4] = { p0, p1, p2, p3 };
                vec2 *ts[4] = { &tc0, &tc1, &tc2, &tc3 };
                float height_left = vec3_distance(ps[top_left], ps[bot_left]);
                float height_right = vec3_distance(ps[top_right], ps[bot_right]);
                vec3 dir = vec3_normalize(vec3_sub(ps[top_right], ps[top_left]));
                if (vec3_dot(dir, V3(0.9f, 0.0f, 0.1f)) < 0.0f) dir = vec3_scale(dir, -1.0f);
                float l0 = vec3_dot(dir, ps[top_left]);
                float l1 = vec3_dot(dir, ps[top_right]);
                ts[top_left]->x = 0.0f;
                ts[top_left]->y = l0;
                ts[top_right]->x = 0.0f;
                ts[top_right]->y = l1;
                ts[bot_left]->x = height_left;
                ts[bot_left]->y = l0;
                ts[bot_right]->x = height_right;
                ts[bot_right]->y = l1;
                face.texture_coord_scale = 0.25f;
            }
            else if (face.auto_texture == AUTO_TEXTURE_WOOD_IN) {
                int top_left, top_right, bot_left, bot_right;
                if (p0.y >= p1.y && p0.y >= p2.y && p0.y >= p3.y) {
                    if (fabsf(vec3_dot(vec3_sub(p0, p1), V3(0.0f, 1.0f, 0.0f))) > 
                            fabsf(vec3_dot(vec3_sub(p0, p3), V3(0.0f, 1.0f, 0.0f)))) {
                        top_left = 0;
                        bot_left = 1;
                        bot_right = 2;
                        top_right = 3;
                    }
                    else {
                        top_right = 0;
                        top_left = 1;
                        bot_left = 2;
                        bot_right = 3;
                    }
                }
                else if (p1.y >= p0.y && p1.y >= p2.y && p1.y >= p3.y) {
                    if (fabsf(vec3_dot(vec3_sub(p1, p2), V3(0.0f, 1.0f, 0.0f))) > 
                            fabsf(vec3_dot(vec3_sub(p1, p0), V3(0.0f, 1.0f, 0.0f)))) {
                        top_left = 1;
                        bot_left = 2;
                        bot_right = 3;
                        top_right = 0;
                    }
                    else {
                        top_right = 1;
                        top_left = 2;
                        bot_left = 3;
                        bot_right = 0;
                    }
                }
                else if (p2.y >= p0.y && p2.y >= p1.y && p2.y >= p3.y) {
                    if (fabsf(vec3_dot(vec3_sub(p2, p3), V3(0.0f, 1.0f, 0.0f))) > 
                            fabsf(vec3_dot(vec3_sub(p3, p1), V3(0.0f, 1.0f, 0.0f)))) {
                        top_left = 2;
                        bot_left = 3;
                        bot_right = 0;
                        top_right = 1;
                    }
                    else {
                        top_right = 2;
                        top_left = 3;
                        bot_left = 0;
                        bot_right = 1;
                    }
                }
                else {
                    if (fabsf(vec3_dot(vec3_sub(p3, p0), V3(0.0f, 1.0f, 0.0f))) > 
                            fabsf(vec3_dot(vec3_sub(p3, p2), V3(0.0f, 1.0f, 0.0f)))) {
                        top_left = 3;
                        bot_left = 0;
                        bot_right = 1;
                        top_right = 2;
                    }
                    else {
                        top_right = 3;
                        top_left = 0;
                        bot_left = 1;
                        bot_right = 2;
                    }
                }

                vec3 ps[4] = { p0, p1, p2, p3 };
                vec2 *ts[4] = { &tc0, &tc1, &tc2, &tc3 };
                vec3 dir = vec3_normalize(vec3_sub(ps[top_left], ps[top_right]));
                if (vec3_dot(dir, V3(0.9f, 0.0f, 0.1f)) < 0.0f) dir = vec3_scale(dir, -1.0f);
                float l0 = vec3_dot(dir, ps[top_left]);
                float l1 = vec3_dot(dir, ps[top_right]);
                ts[top_left]->x = 0.0f;
                ts[top_left]->y = l0;
                ts[top_right]->x = 0.0f;
                ts[top_right]->y = l1;
                ts[bot_left]->x = 1.0f;
                ts[bot_left]->y = l0;
                ts[bot_right]->x = 1.0f;
                ts[bot_right]->y = l1;
                face.texture_coord_scale = 0.25f;
            }
            else if (face.auto_texture == AUTO_TEXTURE_WOOD_TOP) {
                int top_left, top_right, bot_left, bot_right;
                if (p0.x >= p1.x && p0.x >= p2.x && p0.x >= p3.x) {
                    if (fabsf(vec3_dot(vec3_sub(p0, p1), V3(1.0f, 0.0f, 0.0f))) > 
                            fabsf(vec3_dot(vec3_sub(p0, p3), V3(1.0f, 0.0f, 0.0f)))) {
                        top_left = 0;
                        bot_left = 1;
                        bot_right = 2;
                        top_right = 3;
                    }
                    else {
                        top_right = 0;
                        top_left = 1;
                        bot_left = 2;
                        bot_right = 3;
                    }
                }
                else if (p1.x >= p0.x && p1.x >= p2.x && p1.x >= p3.x) {
                    if (fabsf(vec3_dot(vec3_sub(p1, p2), V3(1.0f, 0.0f, 0.0f))) > 
                            fabsf(vec3_dot(vec3_sub(p1, p0), V3(1.0f, 0.0f, 0.0f)))) {
                        top_left = 1;
                        bot_left = 2;
                        bot_right = 3;
                        top_right = 0;
                    }
                    else {
                        top_right = 1;
                        top_left = 2;
                        bot_left = 3;
                        bot_right = 0;
                    }
                }
                else if (p2.x >= p0.x && p2.x >= p1.x && p2.x >= p3.x) {
                    if (fabsf(vec3_dot(vec3_sub(p2, p3), V3(1.0f, 0.0f, 0.0f))) > 
                            fabsf(vec3_dot(vec3_sub(p3, p1), V3(1.0f, 0.0f, 0.0f)))) {
                        top_left = 2;
                        bot_left = 3;
                        bot_right = 0;
                        top_right = 1;
                    }
                    else {
                        top_right = 2;
                        top_left = 3;
                        bot_left = 0;
                        bot_right = 1;
                    }
                }
                else {
                    if (fabsf(vec3_dot(vec3_sub(p3, p0), V3(1.0f, 0.0f, 0.0f))) > 
                            fabsf(vec3_dot(vec3_sub(p3, p2), V3(1.0f, 0.0f, 0.0f)))) {
                        top_left = 3;
                        bot_left = 0;
                        bot_right = 1;
                        top_right = 2;
                    }
                    else {
                        top_right = 3;
                        top_left = 0;
                        bot_left = 1;
                        bot_right = 2;
                    }
                }

                vec3 ps[4] = { p0, p1, p2, p3 };
                vec2 *ts[4] = { &tc0, &tc1, &tc2, &tc3 };
                float dz = ps[top_left].z - ps[top_right].z;
                float dx = ps[top_left].x - ps[bot_left].x;

                if (fabsf(dz - 0.5f) < fabsf(dx - 0.5f)) {
                    vec3 dir_left = vec3_normalize(vec3_sub(ps[top_left], ps[bot_left]));
                    if (vec3_dot(dir_left, V3(0.9f, 0.0f, 0.1f)) < 0.0f) 
                        dir_left = vec3_scale(dir_left, -1.0f);
                    vec3 dir_right = vec3_normalize(vec3_sub(ps[top_right], ps[bot_right]));
                    if (vec3_dot(dir_right, V3(0.9f, 0.0f, 0.1f)) < 0.0f) 
                        dir_right = vec3_scale(dir_right, -1.0f);
                    ts[top_left]->x = 0.0f;
                    ts[top_left]->y = vec3_dot(dir_left, ps[top_left]);
                    ts[top_right]->x = 1.0f;
                    ts[top_right]->y = vec3_dot(dir_right, ps[top_right]);
                    ts[bot_left]->x = 0.0f;
                    ts[bot_left]->y = vec3_dot(dir_left, ps[bot_left]);
                    ts[bot_right]->x = 1.0f;
                    ts[bot_right]->y = vec3_dot(dir_right, ps[bot_right]);
                }
                else {
                    vec3 dir_top = vec3_normalize(vec3_sub(ps[top_left], ps[top_right]));
                    if (vec3_dot(dir_top, V3(0.9f, 0.0f, 0.1f)) < 0.0f) 
                        dir_top = vec3_scale(dir_top, -1.0f);
                    vec3 dir_bot = vec3_normalize(vec3_sub(ps[bot_left], ps[bot_right]));
                    if (vec3_dot(dir_bot, V3(0.9f, 0.0f, 0.1f)) < 0.0f) 
                        dir_bot = vec3_scale(dir_bot, -1.0f);
                    ts[top_left]->x = 0.0f;
                    ts[top_left]->y = vec3_dot(dir_top, ps[top_left]);
                    ts[top_right]->x = 0.0f;
                    ts[top_right]->y = vec3_dot(dir_top, ps[top_right]);
                    ts[bot_left]->x = 1.0f;
                    ts[bot_left]->y = vec3_dot(dir_bot, ps[bot_left]);
                    ts[bot_right]->x = 1.0f;
                    ts[bot_right]->y = vec3_dot(dir_bot, ps[bot_right]);
                }
                face.texture_coord_scale = 0.25f;
            }
            else if (face.auto_texture == AUTO_TEXTURE_GRASS) {
                tc0.x = p0.x;
                tc0.y = p0.z;
                tc1.x = p1.x;
                tc1.y = p1.z;
                tc2.x = p2.x;
                tc2.y = p2.z;
                tc3.x = p3.x;
                tc3.y = p3.z;
                face.texture_coord_scale = 0.5f;
            }

            array_push(texture_coords, vec2_scale(tc0, face.texture_coord_scale));
            array_push(texture_coords, vec2_scale(tc1, face.texture_coord_scale));
            array_push(texture_coords, vec2_scale(tc2, face.texture_coord_scale));
            array_push(texture_coords, vec2_scale(tc2, face.texture_coord_scale));
            array_push(texture_coords, vec2_scale(tc3, face.texture_coord_scale));
            array_push(texture_coords, vec2_scale(tc0, face.texture_coord_scale));
            array_push(material_idx, (float) face.mat_idx);
            array_push(material_idx, (float) face.mat_idx);
            array_push(material_idx, (float) face.mat_idx);
            array_push(material_idx, (float) face.mat_idx);
            array_push(material_idx, (float) face.mat_idx);
            array_push(material_idx, (float) face.mat_idx);
        }
        else {
            assert(false);
        }
    }
}

void terrain_model_update_buffers(struct terrain_model *model) {
    struct array_vec3 positions, normals;
    struct array_vec2 texture_coords;
    struct array_float material_idx;
    array_init(&positions);
    array_init(&normals);
    array_init(&texture_coords);
    array_init(&material_idx);
    terrain_model_generate_triangle_data(model,
            &positions, &normals, &texture_coords, &material_idx);

    if (positions.length > model->buf_len) {
        sg_destroy_buffer(model->positions_buf);
        sg_destroy_buffer(model->normals_buf);
        sg_destroy_buffer(model->texture_coords_buf);
        sg_destroy_buffer(model->material_idxs_buf);

        model->buf_len = 2 * positions.length;
        sg_buffer_desc desc = {
            .type = SG_BUFFERTYPE_VERTEXBUFFER,
            .usage = SG_USAGE_DYNAMIC,
        };

        desc.size = sizeof(vec3) * model->buf_len;
        model->positions_buf = sg_make_buffer(&desc);

        desc.size = sizeof(vec3) * model->buf_len;
        model->normals_buf = sg_make_buffer(&desc);

        desc.size = sizeof(vec2) * model->buf_len;
        model->texture_coords_buf = sg_make_buffer(&desc);

        desc.size = sizeof(float) * model->buf_len;
        model->material_idxs_buf = sg_make_buffer(&desc);
    }

    model->num_elements = positions.length;
    sg_update_buffer(model->positions_buf, positions.data, sizeof(vec3) * positions.length);
    sg_update_buffer(model->normals_buf, normals.data, sizeof(vec3) * normals.length);
    sg_update_buffer(model->texture_coords_buf, texture_coords.data, sizeof(vec2) * texture_coords.length);
    sg_update_buffer(model->material_idxs_buf, material_idx.data, sizeof(float) * material_idx.length);

    array_deinit(&positions);
    array_deinit(&normals);
    array_deinit(&texture_coords);
    array_deinit(&material_idx);
}

void terrain_model_export(struct terrain_model *model, struct file *file) {
    char sprintf_buffer[1024];
    struct array_char buffer;
    array_init(&buffer);

    array_pusharr(&buffer, "MODEL\n", 6);
    array_pusharr(&buffer, "1\n", 2);
    for (int i = 0; i < MAX_NUM_TERRAIN_MODEL_MATERIALS; i++) {
        vec3 c = model->materials[i].color0;
        sprintf(sprintf_buffer, "%f %f %f\n", c.x, c.y, c.z);
        array_pusharr(&buffer, sprintf_buffer, (int) strlen(sprintf_buffer));
    }

    sprintf(sprintf_buffer, "%d\n", model->points.length);
    array_pusharr(&buffer, sprintf_buffer, (int) strlen(sprintf_buffer));
    for (int i = 0; i < model->points.length; i++) {
        vec3 p = model->points.data[i];
        sprintf(sprintf_buffer, "%f %f %f\n", p.x, p.y, p.z);
        array_pusharr(&buffer, sprintf_buffer, (int) strlen(sprintf_buffer));
    }

    sprintf(sprintf_buffer, "%d\n", model->faces.length);
    array_pusharr(&buffer, sprintf_buffer, (int) strlen(sprintf_buffer));
    for (int i = 0; i < model->faces.length; i++) {
        struct terrain_model_face face = model->faces.data[i];
        sprintf(sprintf_buffer, "%d %d %d %d %d %d\n", face.num_points, face.x, face.y, face.z,
                face.w, face.mat_idx);
        array_pusharr(&buffer, sprintf_buffer, (int) strlen(sprintf_buffer));
    }

    file_set_data(file, buffer.data, buffer.length);
    array_deinit(&buffer);
}

bool terrain_model_import(struct terrain_model *model, struct file *file) {
    if (!file_load_data(file)) {
        return false;
    }

    bool is_error = false;
    struct array_vec3 points;
    struct array_terrain_model_face faces; 
    int n, line_num = 0;
    char *line_buffer = NULL;
    int line_buffer_len = 0;
    vec3 color0[MAX_NUM_TERRAIN_MODEL_MATERIALS];
    vec3 color1[MAX_NUM_TERRAIN_MODEL_MATERIALS];

    array_init(&points);
    array_init(&faces);

    {
        file_copy_line(file, &line_buffer, &line_buffer_len);
        line_num++;
        if (line_buffer[0] != 'M' || line_buffer[1] != 'O' || line_buffer[2] != 'D' || 
                line_buffer[3] != 'E' || line_buffer[4] != 'L') {
            printf("File must begin with MODEL\n");
            is_error = true;
            goto clean_up;
        }
    }

    {
        file_copy_line(file, &line_buffer, &line_buffer_len);
        line_num++;
    }

    {
        for (int i = 0; i < MAX_NUM_TERRAIN_MODEL_MATERIALS; i++) {
            vec3 c;

            file_copy_line(file, &line_buffer, &line_buffer_len);
            line_num++;
            n = sscanf(line_buffer, "%f %f %f", &c.x, &c.y, &c.z);
            if (n != 3) {
                printf("Invalid material on line: %d\n", line_num);
                is_error = true;
                goto clean_up;
            }
            color0[i] = c;

            file_copy_line(file, &line_buffer, &line_buffer_len);
            line_num++;
            n = sscanf(line_buffer, "%f %f %f", &c.x, &c.y, &c.z);
            if (n != 3) {
                printf("Invalid material on line: %d\n", line_num);
                is_error = true;
                goto clean_up;
            }
            color1[i] = c;
        }
    }

    {
        file_copy_line(file, &line_buffer, &line_buffer_len);
        line_num++;

        int num_points;
        n = sscanf(line_buffer, "%d", &num_points);
        if (n != 1) {
            printf("Invalid number of points on line: %d\n", line_num);
            is_error = true;
            goto clean_up;
        }

        for (int i = 0; i < num_points; i++) {
            file_copy_line(file, &line_buffer, &line_buffer_len);
            line_num++;
            vec3 point;
            n = sscanf(line_buffer, "%f %f %f", &point.x, &point.y, &point.z);
            if (n != 3) {
                printf("Invalid point on line: %d\n", line_num);
                is_error = true;
                goto clean_up;
            }

            array_push(&points, point);
        }
    }

    {
        file_copy_line(file, &line_buffer, &line_buffer_len);
        line_num++;

        int num_faces;
        n = sscanf(line_buffer, "%d", &num_faces);
        if (n != 1) {
            printf("Invalid number of faces on line: %d\n", line_num);
            is_error = true;
            goto clean_up;
        }

        for (int i = 0; i < num_faces; i++) {
            struct terrain_model_face face;

            file_copy_line(file, &line_buffer, &line_buffer_len);
            line_num++;
            n = sscanf(line_buffer, "%d", &face.num_points);
            if (n != 1) {
                printf("Expected face num points on line: %d\n", line_num); 
                is_error = true;
                goto clean_up;
            }

            file_copy_line(file, &line_buffer, &line_buffer_len);
            line_num++;
            n = sscanf(line_buffer, "%d %d", &face.mat_idx, &face.smooth_normal);
            if (n != 2) {
                printf("Expected face mat idx and smooth normal on line: %d\n", line_num); 
                is_error = true;
                goto clean_up;
            }

            file_copy_line(file, &line_buffer, &line_buffer_len);
            line_num++;
            n = sscanf(line_buffer, "%d %d %d %d", &face.x, &face.y, &face.z, &face.w);
            if (n != 4) {
                printf("Expected face point indexes on line: %d\n", line_num); 
                is_error = true;
                goto clean_up;
            }

            file_copy_line(file, &line_buffer, &line_buffer_len);
            line_num++;
            n = sscanf(line_buffer, "%f %f %f %f %f %f %f %f", 
                    &face.texture_coords[0].x, &face.texture_coords[0].y,
                    &face.texture_coords[1].x, &face.texture_coords[1].y,
                    &face.texture_coords[2].x, &face.texture_coords[2].y,
                    &face.texture_coords[3].x, &face.texture_coords[3].y);
            if (n != 8) {
                printf("Expected face point texture coords on line: %d\n", line_num); 
                is_error = true;
                goto clean_up;
            }

            array_push(&faces, face);
        }
    }

    for (int i = 0; i < MAX_NUM_TERRAIN_MODEL_MATERIALS; i++) {
        model->materials[i].color0 = color0[i];
        model->materials[i].color1 = color1[i];
    }
    model->points.length = 0;
    model->faces.length = 0;
    for (int i = 0; i < points.length; i++) {
        terrain_model_add_point(model, points.data[i], -1);
    }
    for (int i = 0; i < faces.length; i++) {
        terrain_model_add_face(model, faces.data[i], -1);
    }

clean_up:
    array_deinit(&points);
    array_deinit(&faces);
    file_delete_data(file);
    return is_error;
}

void terrain_model_make_square(struct terrain_model *model) {
    model->points.length = 0;
    model->faces.length = 0;

    terrain_model_add_point(model, V3(-1.0f, -1.0f, -1.0f), -1);
    terrain_model_add_point(model, V3(1.0f, -1.0f, -1.0f), -1);
    terrain_model_add_point(model, V3(1.0f, 1.0f, -1.0f), -1);
    terrain_model_add_point(model, V3(-1.0f, 1.0f, -1.0f), -1);
    terrain_model_add_point(model, V3(-1.0f, -1.0f, 1.0f), -1);
    terrain_model_add_point(model, V3(1.0f, -1.0f, 1.0f), -1);
    terrain_model_add_point(model, V3(1.0f, 1.0f, 1.0f), -1);
    terrain_model_add_point(model, V3(-1.0f, 1.0f, 1.0f), -1);

    struct terrain_model_face face = create_terrain_model_face(4, 0, false, 
            0, 0, 0, 0, V2_ZERO, V2_ZERO, V2_ZERO, V2_ZERO, 1.0f, 1.0f, 1.0f, 1.0f, AUTO_TEXTURE_NONE);
    face.x = 3; face.y = 2; face.z = 1; face.w = 0;
    terrain_model_add_face(model, face, -1);
    face.x = 5; face.y = 6; face.z = 7; face.w = 4;
    terrain_model_add_face(model, face, -1);
    face.x = 4; face.y = 7; face.z = 3; face.w = 0;
    terrain_model_add_face(model, face, -1);
    face.x = 2; face.y = 6; face.z = 5; face.w = 1;
    terrain_model_add_face(model, face, -1);
    face.x = 5; face.y = 4; face.z = 0; face.w = 1;
    terrain_model_add_face(model, face, -1);
    face.x = 7; face.y = 6; face.z = 2; face.w = 3;
    terrain_model_add_face(model, face, -1);
}

void terrain_entity_init(struct terrain_entity *entity, int num_elements, 
        int lightmap_width, int lightmap_height) {
    entity->position = V3(0.0f, 0.0f, 0.0f);
    entity->scale = V3(1.0f, 1.0f, 1.0f);
    entity->orientation = QUAT(0.0f, 0.0f, 0.0f, 1.0f);

    terrain_model_init(&entity->terrain_model, num_elements);
    lightmap_init(&entity->lightmap, lightmap_width, lightmap_height, num_elements, 1);
}

void terrain_entity_deinit(struct terrain_entity *entity) {
    terrain_model_deinit(&entity->terrain_model);
    lightmap_deinit(&entity->lightmap);
}

void terrain_entity_copy(struct terrain_entity *entity, struct terrain_entity *entity_to_copy) {
    entity->position = entity_to_copy->position;
    entity->scale = entity_to_copy->scale;
    entity->orientation = entity_to_copy->orientation;
    terrain_model_copy(&entity->terrain_model, &entity_to_copy->terrain_model);
}

mat4 terrain_entity_get_transform(struct terrain_entity *entity) {
    return mat4_multiply_n(3,
            mat4_translation(entity->position),
            mat4_scale(entity->scale),
            mat4_from_quat(entity->orientation));
}

void multi_terrain_entity_init(struct multi_terrain_entity *entity, int num_static_elements,
        int num_moving_elements, int lightmap_width, int lightmap_height) {
    entity->movement_data.type = MOVEMENT_TYPE_TO_AND_FROM;
    entity->movement_data.pendulum.theta0 = 0.0f;
    entity->movement_data.to_and_from.p0 = V3(0.0f, 0.0f, 0.0f);
    entity->movement_data.to_and_from.p1 = V3(0.0f, 0.0f, 0.0f);
    entity->movement_data.ramp.theta0 = 0.0f;
    entity->movement_data.ramp.theta1 = 1.0f;
    entity->movement_data.ramp.rotation_axis = V3(1.0f, 0.0f, 0.0f);
    entity->movement_data.ramp.transition_length = 1.0f;
    entity->movement_data.rotation.theta0 = 0.0f;
    entity->movement_data.rotation.axis = V3(1.0f, 0.0f, 0.0f);
    entity->movement_data.length = 1.0f;
    entity->moving_position = V3(0.0f, 0.0f, 0.0f);
    entity->moving_scale = V3(1.0f, 1.0f, 1.0f);
    entity->moving_orientation = QUAT(0.0f, 0.0f, 0.0f, 1.0f);
    terrain_model_init(&entity->moving_terrain_model, num_moving_elements);
    lightmap_init(&entity->moving_lightmap, lightmap_width, lightmap_height, num_moving_elements, 10);

    entity->static_position = V3(0.0f, 0.0f, 0.0f);
    entity->static_scale = V3(1.0f, 1.0f, 1.0f);
    entity->static_orientation = QUAT(0.0f, 0.0f, 0.0f, 1.0f);
    terrain_model_init(&entity->static_terrain_model, num_static_elements);
    lightmap_init(&entity->static_lightmap, lightmap_width, lightmap_height, num_static_elements, 10);
}

void multi_terrain_entity_deinit(struct multi_terrain_entity *entity) {
    terrain_model_deinit(&entity->static_terrain_model);
    lightmap_deinit(&entity->static_lightmap);
    terrain_model_deinit(&entity->moving_terrain_model);
    lightmap_deinit(&entity->moving_lightmap);
}

void multi_terrain_entity_copy(struct multi_terrain_entity *entity, struct multi_terrain_entity *entity_to_copy) {
    entity->static_position = entity_to_copy->static_position;
    entity->static_scale = entity_to_copy->static_scale;
    entity->static_orientation = entity_to_copy->static_orientation;
    terrain_model_copy(&entity->static_terrain_model, &entity_to_copy->static_terrain_model);

    entity->movement_data = entity_to_copy->movement_data;
    entity->moving_position = entity_to_copy->moving_position;
    entity->moving_scale = entity_to_copy->moving_scale;
    entity->moving_orientation = entity_to_copy->moving_orientation;
    terrain_model_copy(&entity->moving_terrain_model, &entity_to_copy->moving_terrain_model);
}

mat4 multi_terrain_entity_get_static_transform(struct multi_terrain_entity *entity) {
    return mat4_multiply_n(3,
            mat4_translation(entity->static_position),
            mat4_scale(entity->static_scale),
            mat4_from_quat(entity->static_orientation));
}

mat4 multi_terrain_entity_get_moving_transform(struct multi_terrain_entity *entity, float t) {
    struct movement_data movement_data = entity->movement_data;

    if (movement_data.type == MOVEMENT_TYPE_PENDULUM) {
        float a = 2.0f*fmodf(t, movement_data.length)/movement_data.length;
        if (a >= 1.0f) a = 2.0f - a;

        float theta = movement_data.pendulum.theta0*cosf(MF_PI*a);
        return mat4_multiply_n(4, 
                mat4_translation(entity->moving_position),
                mat4_scale(entity->moving_scale),
                mat4_rotation_x(theta),
                mat4_from_quat(entity->moving_orientation));
    }
    else if (movement_data.type == MOVEMENT_TYPE_TO_AND_FROM) {
        float a = 2.0f*fmodf(t, movement_data.length)/movement_data.length;
        if (a >= 1.0f) a = 2.0f - a;

        vec3 p0 = movement_data.to_and_from.p0;
        vec3 p1 = movement_data.to_and_from.p1;
        vec3 p = vec3_add(p0, vec3_scale(vec3_sub(p1, p0), a));
        return mat4_multiply_n(4,
                mat4_translation(p),
                mat4_translation(entity->moving_position),
                mat4_scale(entity->moving_scale),
                mat4_from_quat(entity->moving_orientation));
    }
    else if (movement_data.type == MOVEMENT_TYPE_RAMP) {
        float a = 2.0f*fmodf(t, movement_data.length)/movement_data.length;
        if (a >= 1.0f) a = 2.0f - a;

        float transition_dt = movement_data.ramp.transition_length/movement_data.length;
        float theta_dt = 0.5f*(1.0f - transition_dt);
        float theta0 = movement_data.ramp.theta0;
        float theta1 = movement_data.ramp.theta1;
        vec3 axis = movement_data.ramp.rotation_axis;
        float theta = 0.0f;
        if (a < theta_dt) {
            theta = theta0;
        }
        else if (a < theta_dt + transition_dt) {
            float b = (a - theta_dt)/transition_dt;
            theta = theta0 + (theta1 - theta0)*b;
        }
        else {
            theta = theta1;
        }
        return mat4_multiply_n(4,
                mat4_translation(entity->moving_position),
                mat4_from_quat(quat_create_from_axis_angle(axis, theta)),
                mat4_scale(entity->moving_scale),
                mat4_from_quat(entity->moving_orientation));
    }
    else if (movement_data.type == MOVEMENT_TYPE_ROTATION) {
        float a = fmodf(t, movement_data.length)/movement_data.length;
        vec3 axis = movement_data.rotation.axis;
        float theta = 2.0f*MF_PI*a + movement_data.rotation.theta0;
        return mat4_multiply_n(4,
                mat4_translation(entity->moving_position),
                mat4_from_quat(quat_create_from_axis_angle(axis, theta)),
                mat4_scale(entity->moving_scale),
                mat4_from_quat(entity->moving_orientation));
    }
    else {
        assert(false);
    }
    return mat4_identity();
}

vec3 multi_terrain_entity_get_moving_velocity(struct multi_terrain_entity *entity, float t, vec3 world_point) {
    float dt = 0.001f;
    float t0 = fmodf(t, entity->movement_data.length);
    float t1 = t0 + dt;
    if (t1 >= entity->movement_data.length) t1 = t1 - entity->movement_data.length;

    mat4 transform0 = multi_terrain_entity_get_moving_transform(entity, t0);
    mat4 transform1 = multi_terrain_entity_get_moving_transform(entity, t1);

    vec3 local_point = vec3_apply_mat4(world_point, 1.0f, mat4_inverse(transform0));
    vec3 world_point1 = vec3_apply_mat4(local_point, 1.0f, transform1);

    vec3 velocity = vec3_scale(vec3_sub(world_point1, world_point), 1.0f / dt);
    return velocity;
}

mat4 ball_start_entity_get_transform(struct ball_start_entity *entity) {
    return mat4_multiply_n(3, 
            mat4_translation(entity->position),
            mat4_scale(V3(0.2f, 0.2f, 0.2f)),
            mat4_translation(V3(0.0f, 0.5f, 0.0f)));
}

mat4 cup_entity_get_transform(struct cup_entity *entity) {
    return mat4_multiply_n(2,
            mat4_translation(vec3_add(entity->position, V3(0.0f, 0.0001f, 0.0f))),
            mat4_scale(V3(0.28f, 0.28f, 0.28f)));
}

mat4 camera_zone_entity_get_transform(struct camera_zone_entity *entity) {
    return mat4_multiply_n(2,
            mat4_translation(entity->position),
            mat4_scale(V3(entity->size.x, 0.001f, entity->size.y)));
}

mat4 beginning_camera_animation_entity_get_transform(struct beginning_camera_animation_entity *entity) {
    return mat4_multiply_n(2,
            mat4_translation(entity->start_position),
            mat4_scale(V3(0.3f, 0.3f, 0.3f)));
}

void ground_entity_init(struct ground_entity *entity) {
    terrain_model_init(&entity->model, 256);
    lightmap_init(&entity->lightmap, 256, 256, 256, 1);
}

mat4 ground_entity_get_transform(struct ground_entity *entity) {
    return mat4_translation(entity->position);
}

void environment_entity_init(struct environment_entity *entity, const char *model_name) {
    entity->is_tiled = false;
    entity->position = V3(0.0f, 0.0f, 0.0f);
    entity->scale = V3(1.0f, 1.0f, 1.0f);
    entity->orientation = QUAT(0.0f, 0.0f, 0.0f, 1.0f);
    entity->model = asset_store_get_model(model_name);
}

void environment_entity_deinit(struct environment_entity *entity) {
    entity->model = NULL;
}

mat4 environment_entity_get_transform(struct environment_entity *entity) {
    if (entity->is_tiled) {
        float width = 10.0f;
        float height = (2.0f*width/sqrtf(3));

        float x = ((int)(entity->position.x/((1.0f/2.0f)*width)))*((1.0f/2.0f)*width);
        float y = entity->position.y;
        float z = ((int)(entity->position.z/((3.0f/4.0f)*height)))*((3.0f/4.0f)*height);
        if (fmodf(x, width) < 0.01f) {
            z += (1.0f/2.0f)*height;
        }
        else {
            z -= (1.0f/4.0f)*height;
        }

        return mat4_multiply_n(2, 
                mat4_translation(V3(x, y, z)),
                mat4_scale(V3(10.0f, 10.0f, 10.0f))
                );
    }
    else {
        return mat4_multiply_n(3, 
                mat4_translation(entity->position),
                mat4_from_quat(entity->orientation),
                mat4_scale(entity->scale)
                );
    }
}

void water_entity_init(struct water_entity *entity, int num_elements, int lightmap_width, int lightmap_height) {
    entity->position = V3(0.0f, 0.0f, 0.0f);
    entity->scale = V3(1.0f, 1.0f, 1.0f);
    entity->orientation = QUAT(0.0f, 0.0f, 0.0f, 1.0f);
    terrain_model_init(&entity->model, num_elements);
    lightmap_init(&entity->lightmap, lightmap_width, lightmap_height, num_elements, 1);
}

void water_entity_deinit(struct water_entity *entity) {
    terrain_model_deinit(&entity->model);
    lightmap_deinit(&entity->lightmap);
}

mat4 water_entity_get_transform(struct water_entity *entity) {
    return mat4_multiply_n(3, 
            mat4_translation(entity->position),
            mat4_from_quat(entity->orientation),
            mat4_scale(entity->scale)
            );
}

static void hole_serialize_lightmap(struct data_stream *stream, struct lightmap *lightmap, 
        bool include_lightmaps) {
    serialize_int(stream, lightmap->width);
    serialize_int(stream, lightmap->height);
    if (include_lightmaps) {
        serialize_int(stream, lightmap->images.length);
        for (int i = 0; i < lightmap->images.length; i++) {
            serialize_char_array(stream, (char*) lightmap->images.data[i].data, lightmap->width * lightmap->height);
        }
    }
    serialize_int(stream, lightmap->uvs.length);
    serialize_vec2_array(stream, lightmap->uvs.data, lightmap->uvs.length);
}

struct lightmap_info {
    int num_images;
    int width, height;    
    struct array_char_ptr datas;

    int num_uvs;
    vec2 *uvs;
};

static void hole_deserialize_24_lightmap(struct data_stream *stream, struct lightmap_info *info,
        bool include_lightmaps) {
    info->width = deserialize_int(stream);
    info->height = deserialize_int(stream);
    array_init(&info->datas);
    if (include_lightmaps) {
        info->num_images = deserialize_int(stream);
        for (int i = 0; i < info->num_images; i++) {
            array_push(&info->datas, malloc(sizeof(unsigned char) * info->width * info->height));
            deserialize_char_array(stream, info->datas.data[i], info->width * info->height);
        }
    }
    else {
        info->num_images = 0;
    }
    info->num_uvs = deserialize_int(stream);
    info->uvs = malloc(sizeof(vec2) * info->num_uvs);
    deserialize_vec2_array(stream, info->uvs, info->num_uvs);
}

static void fill_lightmap(struct lightmap *lightmap, struct lightmap_info *info) {
    if (info->num_images > 0) {
        lightmap_resize(lightmap, info->width, info->height, info->num_images);
        for (int i = 0; i < info->num_images; i++) {
            memcpy(lightmap->images.data[i].data, info->datas.data[i], 
                    sizeof(unsigned char) * info->width * info->height);
        }
    }

    lightmap->uvs.length = 0;
    for (int i = 0; i < info->num_uvs; i++) {
        array_push(&lightmap->uvs, info->uvs[i]);
    }
}

static void free_lightmap_info(struct lightmap_info *info) {
    for (int i = 0; i < info->num_images; i++) {
        if (info->datas.data[i]) {
            free(info->datas.data[i]);
        }
    }
    array_deinit(&info->datas);
    free(info->uvs);
}

static void hole_serialize_terrain_model(struct data_stream *stream, struct terrain_model *model) {
    serialize_int(stream, model->points.length);
    for (int j = 0; j < model->points.length; j++) {
        serialize_vec3(stream, model->points.data[j]);
    }

    serialize_int(stream, model->faces.length);
    for (int j = 0; j < model->faces.length; j++) {
        struct terrain_model_face *face = &model->faces.data[j];
        serialize_int(stream, face->smooth_normal);
        serialize_int(stream, face->num_points);
        serialize_int(stream, face->x);
        serialize_int(stream, face->y);
        serialize_int(stream, face->z);
        serialize_int(stream, face->w);
        serialize_int(stream, face->mat_idx);
        serialize_float(stream, face->texture_coord_scale);
        serialize_vec2_array(stream, face->texture_coords, 4);
        serialize_float(stream, face->cor);
        serialize_float(stream, face->friction);
        serialize_float(stream, face->vel_scale);
        if (face->auto_texture == AUTO_TEXTURE_NONE) {
            serialize_char(stream, 'N');
        }
        else if (face->auto_texture == AUTO_TEXTURE_WOOD_OUT) {
            serialize_char(stream, 'O');
        }
        else if (face->auto_texture == AUTO_TEXTURE_WOOD_IN) {
            serialize_char(stream, 'I');
        }
        else if (face->auto_texture == AUTO_TEXTURE_WOOD_TOP) {
            serialize_char(stream, 'T');
        }
        else if (face->auto_texture == AUTO_TEXTURE_GRASS) {
            serialize_char(stream, 'G');
        }
        else {
            assert(false);
        }
    }

    for (int j = 0; j < MAX_NUM_TERRAIN_MODEL_MATERIALS; j++) {
        serialize_vec3(stream, model->materials[j].color0);
        serialize_vec3(stream, model->materials[j].color1);
    }

    serialize_string(stream, model->generator_name);

    map_iter_t iter = map_iter(&model->generator_params);
    const char *key = NULL;
    int num_items = 0;
    while ((key = map_next(&model->generator_params, &iter))) {
        num_items++;
    }
    serialize_int(stream, num_items);

    iter = map_iter(&model->generator_params);
    key = NULL;
    while ((key = map_next(&model->generator_params, &iter))) {
        float *value = map_get(&model->generator_params, key);
        serialize_string(stream, key);
        serialize_float(stream, *value);
    }
}

struct terrain_model_info {
    int num_elements;

    int num_points;
    vec3 *points;

    int num_faces;
    struct terrain_model_face *faces;

    vec3 color0[MAX_NUM_TERRAIN_MODEL_MATERIALS], color1[MAX_NUM_TERRAIN_MODEL_MATERIALS];

    char generator_name[FILES_MAX_FILENAME + 1];
    map_float_t generator_params;
};

static void hole_deserialize_27_terrain_model(struct data_stream *stream, struct terrain_model_info *info) {
    info->num_points = deserialize_int(stream); 
    info->points = malloc(sizeof(vec3) * info->num_points);
    deserialize_vec3_array(stream, info->points, info->num_points);

    info->num_elements = 0;
    info->num_faces = deserialize_int(stream);
    info->faces = malloc(sizeof(struct terrain_model_face) * info->num_faces);
    for (int i = 0; i < info->num_faces; i++) {
        int smooth_normal = deserialize_int(stream);
        int num_points = deserialize_int(stream);
        int x = deserialize_int(stream);
        int y = deserialize_int(stream);
        int z = deserialize_int(stream);
        int w = deserialize_int(stream);
        int mat_idx = deserialize_int(stream);
        float texture_coord_scale = deserialize_float(stream);
        vec2 tc[4];
        deserialize_vec2_array(stream, tc, 4);
        float cor = deserialize_float(stream);
        float friction = deserialize_float(stream);
        float vel_scale = deserialize_float(stream);
        char auto_texture_char = deserialize_char(stream);
        enum terrain_model_auto_texture auto_texture; 
        if (auto_texture_char == 'N') {
            auto_texture = AUTO_TEXTURE_NONE;
        }
        else if (auto_texture_char == 'O') {
            auto_texture = AUTO_TEXTURE_WOOD_OUT;
        }
        else if (auto_texture_char == 'I') {
            auto_texture = AUTO_TEXTURE_WOOD_IN;
        }
        else if (auto_texture_char == 'T') {
            auto_texture = AUTO_TEXTURE_WOOD_TOP;
        }
        else if (auto_texture_char == 'G') {
            auto_texture = AUTO_TEXTURE_GRASS;
        }
        else {
            assert(false);
        }

        struct terrain_model_face *face = &info->faces[i];
        *face = create_terrain_model_face(num_points, mat_idx, smooth_normal, x, y, z, w,
                tc[0], tc[1], tc[2], tc[3], texture_coord_scale, cor, friction, vel_scale, auto_texture);

        if (face->num_points == 3) {
            info->num_elements += 3;
        }
        else if (face->num_points == 4) {
            info->num_elements += 6;
        }
        else {
            assert(false);
        }
    }

    for (int i = 0; i < MAX_NUM_TERRAIN_MODEL_MATERIALS; i++) {
        info->color0[i] = deserialize_vec3(stream);
        info->color1[i] = deserialize_vec3(stream);
    }

    char *generator_name = deserialize_string(stream);
    strncpy(info->generator_name, generator_name, FILES_MAX_FILENAME + 1);
    info->generator_name[FILES_MAX_FILENAME] = 0;
    free(generator_name);

    map_init(&info->generator_params);
    int num_params = deserialize_int(stream);
    for (int i = 0; i < num_params; i++) {
        char *param_name = deserialize_string(stream);
        float value = deserialize_float(stream);
        map_set(&info->generator_params, param_name, value);
        free(param_name);
    }
}

static void fill_terrain_model(struct terrain_model *model, struct terrain_model_info *info) {
    model->points.length = 0;
    for (int i = 0; i < info->num_points; i++) {
        terrain_model_add_point(model, info->points[i], -1);
    }

    model->faces.length = 0;
    for (int i = 0; i < info->num_faces; i++) {
        terrain_model_add_face(model, info->faces[i], -1);
    }

    for (int i = 0; i < MAX_NUM_TERRAIN_MODEL_MATERIALS; i++) {
        model->materials[i].color0 = info->color0[i];
        model->materials[i].color1 = info->color1[i];
    }

    strncpy(model->generator_name, info->generator_name, FILES_MAX_FILENAME + 1);
    model->generator_name[FILES_MAX_FILENAME] = 0;

    map_deinit(&model->generator_params);
    map_init(&model->generator_params);

    map_iter_t iter = map_iter(&info->generator_params);
    const char *key = NULL;
    while ((key = map_next(&info->generator_params, &iter))) {
        float *value = map_get(&info->generator_params, key);
        map_set(&model->generator_params, key, *value);
    }
}

static void free_terrain_model_info(struct terrain_model_info *info) {
    free(info->points);
    free(info->faces);
    map_deinit(&info->generator_params);
}

void hole_serialize(struct hole *hole, struct data_stream *stream, bool include_lightmaps) {
    serialize_int(stream, 39);

    serialize_int(stream, hole->terrain_entities.length);
    for (int i = 0; i < hole->terrain_entities.length; i++) {
        struct terrain_entity *entity = &hole->terrain_entities.data[i];
        struct terrain_model *model = &entity->terrain_model;
        struct lightmap *lightmap = &entity->lightmap;

        serialize_vec3(stream, entity->position);
        serialize_vec3(stream, entity->scale);
        serialize_quat(stream, entity->orientation);
        hole_serialize_terrain_model(stream, model);
        hole_serialize_lightmap(stream, lightmap, include_lightmaps);
    }

    serialize_int(stream, hole->multi_terrain_entities.length);
    for (int i = 0; i < hole->multi_terrain_entities.length; i++) {
        struct multi_terrain_entity *entity = &hole->multi_terrain_entities.data[i];
        struct terrain_model *moving_model = &entity->moving_terrain_model;
        struct terrain_model *static_model = &entity->static_terrain_model;
        struct lightmap *moving_lightmap = &entity->moving_lightmap;
        struct lightmap *static_lightmap = &entity->static_lightmap;

        if (entity->movement_data.type == MOVEMENT_TYPE_PENDULUM) {
            serialize_char(stream, 'p');
        }
        else if (entity->movement_data.type == MOVEMENT_TYPE_TO_AND_FROM) {
            serialize_char(stream, 't');
        }
        else if (entity->movement_data.type == MOVEMENT_TYPE_RAMP) {
            serialize_char(stream, 'r');
        }
        else if (entity->movement_data.type == MOVEMENT_TYPE_ROTATION) {
            serialize_char(stream, 'o');
        }
        else {
            assert(false);
        }
        serialize_float(stream, entity->movement_data.pendulum.theta0);
        serialize_vec3(stream, entity->movement_data.to_and_from.p0);
        serialize_vec3(stream, entity->movement_data.to_and_from.p1);
        serialize_vec3(stream, entity->movement_data.ramp.rotation_axis);
        serialize_float(stream, entity->movement_data.ramp.theta0);
        serialize_float(stream, entity->movement_data.ramp.theta1);
        serialize_float(stream, entity->movement_data.ramp.transition_length);
        serialize_float(stream, entity->movement_data.rotation.theta0);
        serialize_vec3(stream, entity->movement_data.rotation.axis);
        serialize_float(stream, entity->movement_data.length);
        serialize_vec3(stream, entity->moving_position);
        serialize_vec3(stream, entity->moving_scale);
        serialize_quat(stream, entity->moving_orientation);
        hole_serialize_terrain_model(stream, moving_model);
        hole_serialize_lightmap(stream, moving_lightmap, include_lightmaps);

        serialize_vec3(stream, entity->static_position);
        serialize_vec3(stream, entity->static_scale);
        serialize_quat(stream, entity->static_orientation);
        hole_serialize_terrain_model(stream, static_model);
        hole_serialize_lightmap(stream, static_lightmap, include_lightmaps);
    }

    {
        struct cup_entity *cup = &hole->cup_entity;
        serialize_vec3(stream, cup->position);

        if (include_lightmaps) {
            struct lightmap *lightmap = &cup->lightmap;
            serialize_int(stream, lightmap->width);
            serialize_int(stream, lightmap->height);
            serialize_char_array(stream, (char*) lightmap->images.data[0].data, lightmap->width * lightmap->height);

            serialize_int(stream, lightmap->uvs.length);
            serialize_vec2_array(stream, lightmap->uvs.data, lightmap->uvs.length);
        }
    }

    {
        struct ball_start_entity *ball_start = &hole->ball_start_entity;
        serialize_vec3(stream, ball_start->position);
    }

    {
        struct beginning_camera_animation_entity *entity = &hole->beginning_camera_animation_entity;
        serialize_vec3(stream, entity->start_position);
    }

    serialize_int(stream, hole->environment_entities.length);
    for (int i = 0; i < hole->environment_entities.length; i++) {
        struct environment_entity *environment = &hole->environment_entities.data[i];
        serialize_int(stream, (int)environment->is_tiled);
        serialize_vec3(stream, environment->position);
        serialize_vec3(stream, environment->scale);
        serialize_quat(stream, environment->orientation);
        serialize_string(stream, environment->model->name);
    }
    hole_serialize_lightmap(stream, &hole->environment_lightmap, include_lightmaps);

    serialize_int(stream, hole->water_entities.length);
    for (int i = 0; i < hole->water_entities.length; i++) {
        struct water_entity *water = &hole->water_entities.data[i];
        serialize_vec3(stream, water->position);
        serialize_vec3(stream, water->scale);
        serialize_quat(stream, water->orientation);
        hole_serialize_terrain_model(stream, &water->model);
        hole_serialize_lightmap(stream, &water->lightmap, include_lightmaps);
    }

    serialize_int(stream, hole->camera_zone_entities.length);
    for (int i = 0; i < hole->camera_zone_entities.length; i++) {
        struct camera_zone_entity *camera_zone = &hole->camera_zone_entities.data[i];
        serialize_vec3(stream, camera_zone->position);
        serialize_vec2(stream, camera_zone->size);
        serialize_quat(stream, camera_zone->orientation);
        serialize_int(stream, (int) camera_zone->look_towards_cup);
    }
}

static void hole_deserialize_39(struct hole *hole, struct data_stream *stream, bool include_lightmaps) {
    int num_terrain_entities = deserialize_int(stream);
    if (hole->terrain_entities.length > num_terrain_entities) {
        for (int i = num_terrain_entities; i < hole->terrain_entities.length; i++) {
            struct terrain_entity *terrain = &hole->terrain_entities.data[i];
            terrain_entity_deinit(terrain);
        }
        hole->terrain_entities.length = num_terrain_entities;
    }

    for (int i = 0; i < num_terrain_entities; i++) {
        vec3 position = deserialize_vec3(stream);
        vec3 scale = deserialize_vec3(stream);
        quat orientation = deserialize_quat(stream);

        struct terrain_model_info terrain_model_info;
        hole_deserialize_27_terrain_model(stream, &terrain_model_info);

        struct lightmap_info lightmap_info;
        hole_deserialize_24_lightmap(stream, &lightmap_info, include_lightmaps);

        struct terrain_entity *terrain;
        if (i < hole->terrain_entities.length) {
            terrain = &hole->terrain_entities.data[i];
        }
        else {
            struct terrain_entity blank_entity;
            memset(&blank_entity, 0, sizeof(blank_entity));
            array_push(&hole->terrain_entities, blank_entity);
            terrain = &hole->terrain_entities.data[i];
            terrain_entity_init(terrain, terrain_model_info.num_elements, 
                    lightmap_info.width, lightmap_info.height);
        }

        terrain->position = position;
        terrain->scale = scale;
        terrain->orientation = orientation;
        fill_terrain_model(&terrain->terrain_model, &terrain_model_info);
        fill_lightmap(&terrain->lightmap, &lightmap_info);

        free_terrain_model_info(&terrain_model_info);
        free_lightmap_info(&lightmap_info);
    }

    int num_multi_terrain_entities = deserialize_int(stream);
    if (hole->multi_terrain_entities.length > num_multi_terrain_entities) {
        for (int i = num_multi_terrain_entities; i < hole->multi_terrain_entities.length; i++) {
            struct multi_terrain_entity *multi_terrain = &hole->multi_terrain_entities.data[i];
            multi_terrain_entity_deinit(multi_terrain);
        }
        hole->multi_terrain_entities.length = num_multi_terrain_entities;
    }

    for (int i = 0; i < num_multi_terrain_entities; i++) {
        char movement_type = deserialize_char(stream);
        float movement_pendulum_theta0 = deserialize_float(stream);
        vec3 movement_to_and_from_p0 = deserialize_vec3(stream);
        vec3 movement_to_and_from_p1 = deserialize_vec3(stream);
        vec3 movement_ramp_rot_axis = deserialize_vec3(stream);
        float movement_ramp_theta0 = deserialize_float(stream);
        float movement_ramp_theta1 = deserialize_float(stream);
        float movement_ramp_transition_length = deserialize_float(stream);
        float movement_ramp_rotation_theta0 = deserialize_float(stream);
        vec3 movement_ramp_rotation_axis = deserialize_vec3(stream);
        float moving_time_length = deserialize_float(stream);
        vec3 moving_position = deserialize_vec3(stream);
        vec3 moving_scale = deserialize_vec3(stream);
        quat moving_orientation = deserialize_quat(stream);
        struct terrain_model_info moving_terrain_model_info;
        hole_deserialize_27_terrain_model(stream, &moving_terrain_model_info);
        struct lightmap_info moving_lightmap_info;
        hole_deserialize_24_lightmap(stream, &moving_lightmap_info, include_lightmaps);

        vec3 static_position = deserialize_vec3(stream);
        vec3 static_scale = deserialize_vec3(stream);
        quat static_orientation = deserialize_quat(stream);
        struct terrain_model_info static_terrain_model_info;
        hole_deserialize_27_terrain_model(stream, &static_terrain_model_info);
        struct lightmap_info static_lightmap_info;
        hole_deserialize_24_lightmap(stream, &static_lightmap_info, include_lightmaps);

        struct multi_terrain_entity *multi_terrain;
        if (i < hole->multi_terrain_entities.length) {
            multi_terrain = &hole->multi_terrain_entities.data[i];
        }
        else {
            struct multi_terrain_entity blank_entity;
            memset(&blank_entity, 0, sizeof(blank_entity));
            array_push(&hole->multi_terrain_entities, blank_entity);
            multi_terrain = &hole->multi_terrain_entities.data[i];
            multi_terrain_entity_init(multi_terrain, 
                    static_terrain_model_info.num_elements, moving_terrain_model_info.num_elements, 
                    static_lightmap_info.width, static_lightmap_info.height);
        }

        if (movement_type == 'p') {
            multi_terrain->movement_data.type = MOVEMENT_TYPE_PENDULUM;
        }
        else if (movement_type == 't') {
            multi_terrain->movement_data.type = MOVEMENT_TYPE_TO_AND_FROM;
        }
        else if (movement_type == 'r') {
            multi_terrain->movement_data.type = MOVEMENT_TYPE_RAMP;
        }
        else if (movement_type == 'o') {
            multi_terrain->movement_data.type = MOVEMENT_TYPE_ROTATION;
        }
        else {
            assert(false);
        }
        multi_terrain->movement_data.pendulum.theta0 = movement_pendulum_theta0;
        multi_terrain->movement_data.to_and_from.p0 = movement_to_and_from_p0;
        multi_terrain->movement_data.to_and_from.p1 = movement_to_and_from_p1;
        multi_terrain->movement_data.ramp.rotation_axis = movement_ramp_rot_axis;
        multi_terrain->movement_data.ramp.theta0 = movement_ramp_theta0;
        multi_terrain->movement_data.ramp.theta1 = movement_ramp_theta1;
        multi_terrain->movement_data.ramp.transition_length = movement_ramp_transition_length;
        multi_terrain->movement_data.rotation.theta0 = movement_ramp_rotation_theta0;
        multi_terrain->movement_data.rotation.axis = movement_ramp_rotation_axis;
        multi_terrain->movement_data.length = moving_time_length;
        multi_terrain->moving_position = moving_position;
        multi_terrain->moving_scale = moving_scale;
        multi_terrain->moving_orientation = moving_orientation;
        fill_terrain_model(&multi_terrain->moving_terrain_model, &moving_terrain_model_info);
        fill_lightmap(&multi_terrain->moving_lightmap, &moving_lightmap_info);

        multi_terrain->static_position = static_position;
        multi_terrain->static_scale = static_scale;
        multi_terrain->static_orientation = static_orientation;
        fill_terrain_model(&multi_terrain->static_terrain_model, &static_terrain_model_info);
        fill_lightmap(&multi_terrain->static_lightmap, &static_lightmap_info);

        free_terrain_model_info(&moving_terrain_model_info);
        free_terrain_model_info(&static_terrain_model_info);
        free_lightmap_info(&moving_lightmap_info);
        free_lightmap_info(&static_lightmap_info);
    }

    {
        struct cup_entity *cup = &hole->cup_entity;
        cup->position = deserialize_vec3(stream);

        if (include_lightmaps) {
            int lightmap_width = deserialize_int(stream);
            int lightmap_height = deserialize_int(stream);
            char *lightmap_data = malloc(sizeof(char) * lightmap_width * lightmap_height);
            deserialize_char_array(stream, lightmap_data, lightmap_width * lightmap_height);

            int num_uvs = deserialize_int(stream);
            vec2 *uvs = malloc(sizeof(vec2) * num_uvs);
            deserialize_vec2_array(stream, uvs, num_uvs);

            struct lightmap *lightmap = &cup->lightmap;
            lightmap_resize(lightmap, lightmap_width, lightmap_height, 1);
            memcpy(lightmap->images.data[0].data, lightmap_data, 
                    sizeof(unsigned char) * lightmap->width * lightmap->height);

            lightmap->uvs.length = 0;
            for (int i = 0; i < num_uvs; i++) {
                array_push(&lightmap->uvs, uvs[i]);
            }

            free(lightmap_data);
            free(uvs);
        }
    }

    {
        struct ball_start_entity *ball_start = &hole->ball_start_entity;
        ball_start->position = deserialize_vec3(stream);
    }

    {
        struct beginning_camera_animation_entity *entity = &hole->beginning_camera_animation_entity;
        entity->start_position = deserialize_vec3(stream);
    }

    int num_environment_entities = deserialize_int(stream);
    if (hole->environment_entities.length > num_environment_entities) {
        for (int i = num_environment_entities; i < hole->environment_entities.length; i++) {
            struct environment_entity *environment = &hole->environment_entities.data[i];
            environment_entity_deinit(environment);
        }
        hole->environment_entities.length = num_environment_entities;
    }

    for (int i = 0; i < num_environment_entities; i++) {
        bool is_tiled = (bool) deserialize_int(stream);
        vec3 position = deserialize_vec3(stream);
        vec3 scale = deserialize_vec3(stream);
        quat orientation = deserialize_quat(stream);
        char *model_name = deserialize_string(stream);

        struct environment_entity *environment;
        if (i < hole->environment_entities.length) {
            environment = &hole->environment_entities.data[i];
        }
        else {
            struct environment_entity blank_entity;
            memset(&blank_entity, 0, sizeof(blank_entity));
            array_push(&hole->environment_entities, blank_entity);
            environment = &hole->environment_entities.data[i];
            environment_entity_init(environment, model_name);
        }

        environment->is_tiled = is_tiled;
        environment->position = position;
        environment->scale = scale;
        environment->orientation = orientation;
        environment->model = asset_store_get_model(model_name);

        free(model_name);
    }

    {
        struct lightmap_info lightmap_info;
        hole_deserialize_24_lightmap(stream, &lightmap_info, include_lightmaps);
        fill_lightmap(&hole->environment_lightmap, &lightmap_info);
        free_lightmap_info(&lightmap_info);
    }

    int num_water_entities = deserialize_int(stream);
    if (hole->water_entities.length > num_water_entities) {
        for (int i = num_water_entities; i < hole->water_entities.length; i++) {
            struct water_entity *water = &hole->water_entities.data[i];
            water_entity_deinit(water);
        }
        hole->water_entities.length = num_water_entities;
    }

    for (int i = 0; i < num_water_entities; i++) {
        vec3 position = deserialize_vec3(stream);
        vec3 scale = deserialize_vec3(stream);
        quat orientation = deserialize_quat(stream);
        struct terrain_model_info terrain_model_info;
        hole_deserialize_27_terrain_model(stream, &terrain_model_info);
        struct lightmap_info lightmap_info;
        hole_deserialize_24_lightmap(stream, &lightmap_info, include_lightmaps);

        struct water_entity *water;
        if (i < hole->water_entities.length) {
            water = &hole->water_entities.data[i];
        }
        else {
            struct water_entity blank_entity;
            memset(&blank_entity, 0, sizeof(blank_entity));
            array_push(&hole->water_entities, blank_entity);
            water = &hole->water_entities.data[i];
            water_entity_init(water, terrain_model_info.num_elements, 128, 128);
        }

        water->position = position;
        water->scale = scale;
        water->orientation = orientation;
        fill_terrain_model(&water->model, &terrain_model_info);
        fill_lightmap(&water->lightmap, &lightmap_info);

        free_terrain_model_info(&terrain_model_info);
        free_lightmap_info(&lightmap_info);
    }

    hole->camera_zone_entities.length = 0;
    int num_camera_zone_entities = deserialize_int(stream);
    for (int i = 0; i < num_camera_zone_entities; i++) {
        vec3 position = deserialize_vec3(stream);
        vec2 size = deserialize_vec2(stream);
        quat orientation = deserialize_quat(stream);
        int look_towards_cup = deserialize_int(stream);

        struct camera_zone_entity entity;
        entity.position = position;
        entity.size = size;
        entity.orientation = orientation;
        entity.look_towards_cup = (bool) look_towards_cup;
        array_push(&hole->camera_zone_entities, entity);
    }
}

void hole_deserialize(struct hole *hole, struct data_stream *stream, bool include_lightmaps) {
    data_stream_reset_pos(stream);
    int version = deserialize_int(stream);    
    if (version == 39) {
        hole_deserialize_39(hole, stream, include_lightmaps);
    }
    else {
        assert(false);
    }
}

void hole_init(struct hole *hole) {
    array_init(&hole->terrain_entities);
    array_init(&hole->multi_terrain_entities);
    array_init(&hole->camera_zone_entities);
    array_init(&hole->environment_entities);
    array_init(&hole->water_entities);

    hole->filepath[0] = 0;

    {
        hole->cup_entity.position = V3(0.0f, 0.0f, 0.0f);
        hole->cup_entity.radius = 0.36f;
        hole->cup_entity.in_hole_delta = V3(0.0f, -0.65f, 0.0f);
        hole->cup_entity.in_hole_radius = 0.30f;
        hole->cup_entity.model = asset_store_get_model("hole");

        struct model *model = hole->cup_entity.model;
        struct lightmap *lightmap = &hole->cup_entity.lightmap;
        lightmap_init(lightmap, 512, 512, model->num_points, 1);
    }

    {
        hole->ball_start_entity.position = V3(0.0f, 0.2f, 0.0f);
        hole->ball_start_entity.model = asset_store_get_model("cylinder");
    }

    {
        hole->beginning_camera_animation_entity.start_position = V3(0.0f, 0.0f, 0.0f);
    }

    {
        lightmap_init(&hole->environment_lightmap, 2048, 2048, 2048, 1);
    }
}

void hole_reset(struct hole *hole) {
    for (int i = 0; i < hole->terrain_entities.length; i++) {
        struct terrain_entity *entity = &hole->terrain_entities.data[i];
        terrain_entity_deinit(entity);
    }
    hole->terrain_entities.length = 0;

    for (int i = 0; i < hole->multi_terrain_entities.length; i++) {
        struct multi_terrain_entity *entity = &hole->multi_terrain_entities.data[i];
        multi_terrain_entity_deinit(entity);
    }
    hole->multi_terrain_entities.length = 0;

    for (int i = 0; i < hole->environment_entities.length; i++) {
        struct environment_entity *entity = &hole->environment_entities.data[i];
        environment_entity_deinit(entity);
    }
    hole->environment_entities.length = 0;

    for (int i = 0; i < hole->water_entities.length; i++) {
        struct water_entity *entity = &hole->water_entities.data[i];
        water_entity_deinit(entity);
    }
    hole->water_entities.length = 0;

    hole->cup_entity.position = V3(1.0f, 0.0f, 0.0f);
    hole->ball_start_entity.position = V3(-1.0f, 0.0f, 0.0f);
    hole->beginning_camera_animation_entity.start_position = V3(0.0f, 0.0f, 0.0f);
    hole->camera_zone_entities.length = 0;
}

void hole_load(struct hole *hole, struct file *file) {
    if (!file_load_data(file)) {
        return;
    }
    strncpy(hole->filepath, file->path, FILES_MAX_PATH);
    hole->filepath[FILES_MAX_PATH - 1] = 0;
    struct data_stream stream;
    data_stream_init(&stream);
    data_stream_push(&stream, file->data, file->data_len);
    data_stream_decompress(&stream);
    hole_deserialize(hole, &stream, true);
    file_delete_data(file);
    data_stream_deinit(&stream);
}

void hole_save(struct hole *hole, struct file *file) {
    struct data_stream stream;
    data_stream_init(&stream);
    hole_serialize(hole, &stream, true);
    data_stream_compress(&stream);
    file_set_data(file, stream.data, stream.len);
    data_stream_deinit(&stream);
}

int hole_add_camera_zone_entity(struct hole *hole) {
    struct camera_zone_entity entity;
    entity.position = V3(0.0f, 0.5f, 0.0f);
    entity.size = V2(1.0f, 1.0f);
    array_push(&hole->camera_zone_entities, entity);
    return hole->camera_zone_entities.length - 1;
}

void hole_update_buffers(struct hole *hole) {
    for (int i = 0; i < hole->terrain_entities.length; i++) {
        struct terrain_entity *terrain = &hole->terrain_entities.data[i];
        struct terrain_model *model = &terrain->terrain_model;
        struct lightmap *lightmap = &terrain->lightmap;
        terrain_model_update_buffers(model);
        lightmap_update_image(lightmap);
        lightmap_update_uvs_buffer(lightmap, model->num_elements);
    }
    
    for (int i = 0; i < hole->multi_terrain_entities.length; i++) {
        struct multi_terrain_entity *multi_terrain = &hole->multi_terrain_entities.data[i];
        struct terrain_model *static_model = &multi_terrain->static_terrain_model;
        struct terrain_model *moving_model = &multi_terrain->moving_terrain_model;
        struct lightmap *static_lightmap = &multi_terrain->static_lightmap;
        struct lightmap *moving_lightmap = &multi_terrain->moving_lightmap;
        terrain_model_update_buffers(static_model);
        terrain_model_update_buffers(moving_model);
        lightmap_update_image(static_lightmap);
        lightmap_update_uvs_buffer(static_lightmap, static_model->num_elements);
        lightmap_update_image(moving_lightmap);
        lightmap_update_uvs_buffer(moving_lightmap, moving_model->num_elements);
    }

    {
        struct cup_entity *cup = &hole->cup_entity;
        struct lightmap *lightmap = &cup->lightmap;
        lightmap_update_image(lightmap);
        lightmap_update_uvs_buffer(lightmap, cup->model->num_points);
    }

    {
        int num_elements = 0;

        for (int i = 0; i < hole->environment_entities.length; i++) {
            struct environment_entity *environment = &hole->environment_entities.data[i];
            struct model *model = environment->model;
            num_elements += model->num_points;
        }

        struct lightmap *lightmap = &hole->environment_lightmap;
        lightmap_update_image(lightmap);
        lightmap_update_uvs_buffer(lightmap, num_elements);
    }

    for (int i = 0; i < hole->water_entities.length; i++) {
        struct water_entity *water = &hole->water_entities.data[i];
        struct terrain_model *model = &water->model;
        struct lightmap *lightmap = &water->lightmap;
        terrain_model_update_buffers(model);
        lightmap_update_image(lightmap);
        lightmap_update_uvs_buffer(lightmap, model->num_elements);
    }
}
