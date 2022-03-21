#include "common/level.h"

#include <assert.h>

#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#include "common/alloc.h"
#include "common/common.h"
#include "common/json.h"
#include "common/log.h"

static const char *_golf_geo_face_uv_gen_type_strings[] = {
    [GOLF_GEO_FACE_UV_GEN_MANUAL] = "manual", 
    [GOLF_GEO_FACE_UV_GEN_GROUND] = "ground", 
    [GOLF_GEO_FACE_UV_GEN_WALL_SIDE] = "wall-side", 
    [GOLF_GEO_FACE_UV_GEN_WALL_TOP] = "wall-top", 
};
static_assert((sizeof(_golf_geo_face_uv_gen_type_strings) / sizeof(_golf_geo_face_uv_gen_type_strings[0])) == GOLF_GEO_FACE_UV_GEN_COUNT, "Invalid number of uv gen type strings");

const char **golf_geo_uv_gen_type_strings(void) {
    return _golf_geo_face_uv_gen_type_strings;
}

golf_geo_generator_data_t golf_geo_generator_data(golf_script_t *script, vec_golf_geo_generator_data_arg_t args) {
    golf_geo_generator_data_t generator_data;
    generator_data.script = script;
    generator_data.args = args;
    return generator_data;
}

void golf_geo_generator_data_init(golf_geo_generator_data_t *generator_data) {
    generator_data->script = NULL;
    vec_init(&generator_data->args, "geo");
}

bool golf_geo_generator_data_get_arg(golf_geo_generator_data_t *data, const char *name, golf_geo_generator_data_arg_t **arg) {
    for (int i = 0; i < data->args.length; i++) {
        if (strcmp(data->args.data[i].name, name) == 0) {
            *arg = &data->args.data[i];
            return true;
        }
    }
    return false;
}

golf_geo_face_t golf_geo_face(const char *material_name, vec_int_t idx, golf_geo_face_uv_gen_type_t uv_gen_type, vec_vec2_t uvs, vec3 water_dir) {
    golf_geo_face_t face;
    face.active = true;
    snprintf(face.material_name, GOLF_MAX_NAME_LEN, "%s", material_name);
    face.idx = idx;
    face.uvs = uvs;
    face.uv_gen_type = uv_gen_type;
    face.start_vertex_in_model = 0;
    face.water_dir = water_dir;
    return face;
}

golf_geo_point_t golf_geo_point(vec3 position) {
    golf_geo_point_t point;
    point.active = true;
    point.position = position;
    return point;
}

typedef map_t(vec_golf_geo_face_ptr_t) _map_vec_golf_geo_face_ptr_t;

static void _golf_geo_face_guess_wall_dir(vec3 p0, vec3 p1, vec3 p2, vec3 *long_dir, vec3 *short_dir) {
    vec3 dir0 = vec3_sub(p1, p0);
    float dir0_l = vec3_length(dir0);
    dir0 = vec3_scale(dir0, 1.0f / dir0_l);

    vec3 dir1 = vec3_sub(p2, p0);
    float dir1_l = vec3_length(dir1);
    dir1 = vec3_scale(dir1, 1.0f / dir1_l);

    vec3 dir2 = vec3_sub(p2, p1);
    float dir2_l = vec3_length(dir2);
    dir2 = vec3_scale(dir2, 1.0f / dir2_l);
    
    float dot0 = vec3_dot(dir0, dir1);
    float dot1 = vec3_dot(dir0, dir2);
    float dot2 = vec3_dot(dir1, dir2);

    if (fabsf(dot0) <= fabsf(dot1) && fabsf(dot0) <= fabsf(dot2)) {
        if (dir0_l <= dir1_l) {
            *long_dir = dir1;
            *short_dir = dir0;
        }
        else {
            *long_dir = dir0;
            *short_dir = dir1;
        }
    }
    else if (fabsf(dot1) <= fabsf(dot0) && fabsf(dot1) <= fabsf(dot2)) {
        if (dir0_l <= dir2_l) {
            *long_dir = dir2;
            *short_dir = dir0;
        }
        else {
            *long_dir = dir0;
            *short_dir = dir2;
        }
    }
    else if (fabsf(dot2) <= fabsf(dot0) && fabsf(dot2) <= fabsf(dot1)) {
        if (dir1_l <= dir2_l) {
            *long_dir = dir2;
            *short_dir = dir1;
        }
        else {
            *long_dir = dir1;
            *short_dir = dir2;
        }
    }
    else {
        golf_log_warning("Could not find a minimum?");
        *long_dir = dir0;
        *short_dir = dir1;
    }

    // Attempt to get the directions to always be in the same direction
    vec3 d = vec3_normalize(V3(1, 2, 3));
    if (vec3_dot(*long_dir, d) < 0) {
        *long_dir = vec3_scale(*long_dir, -1);
    }
    if (vec3_dot(*short_dir, d) < 0) {
        *short_dir = vec3_scale(*short_dir, -1);
    }
}

static void _golf_geo_generate_model_data(golf_geo_t *geo, vec_golf_group_t *groups, vec_vec3_t *positions, vec_vec3_t *normals, vec_vec2_t *texcoords, bool is_water, vec_vec3_t *water_dir) {
    groups->length = 0;
    positions->length = 0;
    normals->length = 0;
    texcoords->length = 0;

    _map_vec_golf_geo_face_ptr_t material_faces;
    map_init(&material_faces, "geo");

    for (int i = 0; i < geo->faces.length; i++) {
        golf_geo_face_t *face = &geo->faces.data[i];
        if (!face->active) continue;

        vec_golf_geo_face_ptr_t *faces = map_get(&material_faces, face->material_name);
        if (faces) {
            vec_push(faces, face);
        }
        else {
            vec_golf_geo_face_ptr_t new_faces;
            vec_init(&new_faces, "geo");
            vec_push(&new_faces, face);
            map_set(&material_faces, face->material_name, new_faces);
        }
    }

    const char *key;
    map_iter_t iter = map_iter(&material_faces);
    while ((key = map_next(&material_faces, &iter))) {
        int group_start = positions->length;
        int group_count = 0;
        vec_golf_geo_face_ptr_t *faces = map_get(&material_faces, key);
        for (int i = 0; i < faces->length; i++) {
            golf_geo_face_t *face = faces->data[i];
            face->start_vertex_in_model = positions->length;

            int idx0 = face->idx.data[0];
            for (int i = 1; i < face->idx.length - 1; i++) {
                int idx1 = face->idx.data[i];
                int idx2 = face->idx.data[i + 1];

                golf_geo_point_t p0 = geo->points.data[idx0];
                golf_geo_point_t p1 = geo->points.data[idx1];
                golf_geo_point_t p2 = geo->points.data[idx2];
                vec2 tc0 = face->uvs.data[0];
                vec2 tc1 = face->uvs.data[i];
                vec2 tc2 = face->uvs.data[i + 1];
                switch (face->uv_gen_type) {
                    case GOLF_GEO_FACE_UV_GEN_COUNT:
                    case GOLF_GEO_FACE_UV_GEN_MANUAL: {
                        break;
                    }
                    case GOLF_GEO_FACE_UV_GEN_GROUND: {
                        tc0 = V2(0.5f * p0.position.x, 0.5f * p0.position.z);
                        tc1 = V2(0.5f * p1.position.x, 0.5f * p1.position.z);
                        tc2 = V2(0.5f * p2.position.x, 0.5f * p2.position.z);
                        break;
                    }
                    case GOLF_GEO_FACE_UV_GEN_WALL_SIDE: {
                        vec3 long_dir, short_dir;
                        _golf_geo_face_guess_wall_dir(p0.position, p1.position, p2.position, &long_dir, &short_dir);

                        tc0.x = vec3_dot(p0.position, short_dir);
                        tc1.x = vec3_dot(p1.position, short_dir);
                        tc2.x = vec3_dot(p2.position, short_dir);

                        tc0.y = 0.5f * vec3_dot(p0.position, long_dir);
                        tc1.y = 0.5f * vec3_dot(p1.position, long_dir);
                        tc2.y = 0.5f * vec3_dot(p2.position, long_dir);
                        break;
                    }
                    case GOLF_GEO_FACE_UV_GEN_WALL_TOP: {
                        vec3 long_dir, short_dir;
                        _golf_geo_face_guess_wall_dir(p0.position, p1.position, p2.position, &long_dir, &short_dir);

                        tc0.x = vec3_dot(p0.position, short_dir);
                        tc1.x = vec3_dot(p1.position, short_dir);
                        tc2.x = vec3_dot(p2.position, short_dir);

                        tc0.y = 0.5f * vec3_dot(p0.position, long_dir);
                        tc1.y = 0.5f * vec3_dot(p1.position, long_dir);
                        tc2.y = 0.5f * vec3_dot(p2.position, long_dir);
                        break;
                    }
                }
                vec3 n = vec3_normalize(vec3_cross(
                            vec3_sub(p1.position, p0.position), 
                            vec3_sub(p2.position, p0.position)));

                vec_push(positions, p0.position);
                vec_push(positions, p1.position);
                vec_push(positions, p2.position);
                vec_push(normals, n);
                vec_push(normals, n);
                vec_push(normals, n);
                vec_push(texcoords, tc0);
                vec_push(texcoords, tc1);
                vec_push(texcoords, tc2);
                if (is_water) {
                    vec_push(water_dir, face->water_dir);
                }
                group_count += 3;
            }
        }
        vec_deinit(faces);

        golf_model_group_t group = golf_model_group(key, group_start, group_count);
        vec_push(groups, group);
    }
}

golf_geo_t golf_geo(vec_golf_geo_point_t points, vec_golf_geo_face_t faces, golf_geo_generator_data_t generator_data, bool is_water) {
    golf_geo_t geo;
    geo.points = points;
    geo.faces = faces;
    geo.generator_data = generator_data;
    geo.model_updated_this_frame = false;
    geo.is_water = is_water;

    vec_golf_group_t groups;
    vec_init(&groups, "geo");
    vec_vec3_t positions;
    vec_init(&positions, "positions");
    vec_vec3_t normals;
    vec_init(&normals, "normals");
    vec_vec2_t texcoords;
    vec_init(&texcoords, "texcoords");
    vec_vec3_t water_dir;
    vec_init(&water_dir, "water_dir");
    _golf_geo_generate_model_data(&geo, &groups, &positions, &normals, &texcoords, is_water, &water_dir);

    if (is_water) {
        geo.model = golf_model_dynamic_water(groups, positions, normals, texcoords, water_dir);
    }
    else {
        geo.model = golf_model_dynamic(groups, positions, normals, texcoords);
    }
    return geo;
}

void golf_geo_finalize(golf_geo_t *geo) {
    geo->model_updated_this_frame = true;
    golf_model_dynamic_finalize(&geo->model);
}

void golf_geo_update_model(golf_geo_t *geo) {
    if (geo->model_updated_this_frame) {
        return;
    }
    geo->model_updated_this_frame = true;

    _golf_geo_generate_model_data(geo, &geo->model.groups, &geo->model.positions, &geo->model.normals, &geo->model.texcoords, geo->is_water, &geo->model.water_dir);
    golf_model_dynamic_update_sg_buf(&geo->model);
}

static void _golf_json_object_set_movement(JSON_Object *obj, const char *name, golf_movement_t *movement) {
    JSON_Value *movement_val = json_value_init_object();
    JSON_Object *movement_obj = json_value_get_object(movement_val);

    switch (movement->type) {
        case GOLF_MOVEMENT_NONE: {
            json_object_set_string(movement_obj, "type", "none");
            break;
        }
        case GOLF_MOVEMENT_LINEAR: {
            json_object_set_string(movement_obj, "type", "linear");
            json_object_set_number(movement_obj, "t0", movement->t0);
            json_object_set_number(movement_obj, "length", movement->length);
            golf_json_object_set_vec3(movement_obj, "p0", movement->linear.p0);
            golf_json_object_set_vec3(movement_obj, "p1", movement->linear.p1);
            break;
        }
        case GOLF_MOVEMENT_SPINNER: {
            json_object_set_string(movement_obj, "type", "spinner");
            json_object_set_number(movement_obj, "t0", movement->t0);
            json_object_set_number(movement_obj, "length", movement->length);
            break;
        }
        case GOLF_MOVEMENT_PENDULUM: {
            json_object_set_string(movement_obj, "type", "pendulum");
            json_object_set_number(movement_obj, "t0", movement->t0);
            json_object_set_number(movement_obj, "length", movement->length);
            json_object_set_number(movement_obj, "theta0", movement->pendulum.theta0);
            golf_json_object_set_vec3(movement_obj, "axis", movement->pendulum.axis);
            break;
        }
        case GOLF_MOVEMENT_RAMP: {
            json_object_set_string(movement_obj, "type", "ramp");
            json_object_set_number(movement_obj, "t0", movement->t0);
            json_object_set_number(movement_obj, "length", movement->length);
            json_object_set_number(movement_obj, "theta0", movement->ramp.theta0);
            json_object_set_number(movement_obj, "theta1", movement->ramp.theta1);
            json_object_set_number(movement_obj, "transition_length", movement->ramp.transition_length);
            golf_json_object_set_vec3(movement_obj, "axis", movement->ramp.axis);
            break;
        }
    }

    json_object_set_value(obj, name, movement_val);
}

static void _golf_json_object_set_geo(JSON_Object *obj, const char *name, golf_geo_t *geo) {
    JSON_Value *geo_val = json_value_init_object();
    JSON_Object *geo_obj = json_value_get_object(geo_val);

    int points_not_active = 0;
    vec_int_t num_points_not_active;
    vec_init(&num_points_not_active, "geo");
    for (int i = 0; i < geo->points.length; i++) {
        vec_push(&num_points_not_active, points_not_active);
        golf_geo_point_t point = geo->points.data[i];
        if (!point.active) {
            points_not_active++;
        }
    }

    JSON_Value *p_val = json_value_init_array();
    JSON_Array *p_arr = json_value_get_array(p_val);
    for (int i = 0; i < geo->points.length; i++) {
        golf_geo_point_t point = geo->points.data[i];
        if (!point.active) continue;

        vec3 pos = point.position;
        json_array_append_number(p_arr, pos.x);
        json_array_append_number(p_arr, pos.y);
        json_array_append_number(p_arr, pos.z);
    }
    json_object_set_value(geo_obj, "p", p_val);

    JSON_Value *faces_val = json_value_init_array();
    JSON_Array *faces_arr = json_value_get_array(faces_val);
    for (int i = 0; i < geo->faces.length; i++) {
        golf_geo_face_t face = geo->faces.data[i];
        if (!face.active) continue;

        JSON_Value *idxs_val = json_value_init_array();
        JSON_Array *idxs_arr = json_value_get_array(idxs_val);
        JSON_Value *uvs_val = json_value_init_array();
        JSON_Array *uvs_arr = json_value_get_array(uvs_val);
        for (int j = 0; j < face.idx.length; j++) {
            int idx = face.idx.data[j];
            json_array_append_number(idxs_arr, (double)(idx - num_points_not_active.data[idx]));

            vec2 uv = face.uvs.data[j];
            json_array_append_number(uvs_arr, (double)uv.x);
            json_array_append_number(uvs_arr, (double)uv.y);
        }

        JSON_Value *face_val = json_value_init_object();
        JSON_Object *face_obj = json_value_get_object(face_val);
        json_object_set_string(face_obj, "material_name", face.material_name);
        json_object_set_value(face_obj, "idxs", idxs_val);
        json_object_set_value(face_obj, "uvs", uvs_val);
        const char *uv_gen_type_str = golf_geo_uv_gen_type_strings()[face.uv_gen_type];
        json_object_set_string(face_obj, "uv_gen_type", uv_gen_type_str);
        json_array_append_value(faces_arr, face_val);
        golf_json_object_set_vec3(face_obj, "water_dir", face.water_dir);
    }
    json_object_set_value(geo_obj, "faces", faces_val);

    if (geo->generator_data.script) {
        JSON_Value *generator_data_val = json_value_init_object();
        JSON_Object *generator_data_obj = json_value_get_object(generator_data_val);
        json_object_set_string(generator_data_obj, "script", geo->generator_data.script->path);

        JSON_Value *args_val = json_value_init_array();
        JSON_Array *args_arr = json_value_get_array(args_val);
        for (int i = 0; i < geo->generator_data.args.length; i++) {
            JSON_Value *arg_val = json_value_init_object();
            JSON_Object *arg_obj = json_value_get_object(arg_val);

            json_object_set_string(arg_obj, "name", geo->generator_data.args.data[i].name);
            gs_val_t val = geo->generator_data.args.data[i].val;
            switch (val.type) {
                case GS_VAL_BOOL: 
                    json_object_set_string(arg_obj, "type", "bool");
                    json_object_set_number(arg_obj, "val", val.bool_val);
                    break;
                case GS_VAL_INT: 
                    json_object_set_string(arg_obj, "type", "int");
                    json_object_set_number(arg_obj, "val", val.int_val);
                    break;
                case GS_VAL_FLOAT: 
                    json_object_set_string(arg_obj, "type", "float");
                    json_object_set_number(arg_obj, "val", val.float_val);
                    break;
                case GS_VAL_VEC2: 
                    json_object_set_string(arg_obj, "type", "vec2");
                    golf_json_object_set_vec2(arg_obj, "val", val.vec2_val);
                    break;
                case GS_VAL_VEC3: 
                    json_object_set_string(arg_obj, "type", "vec3");
                    golf_json_object_set_vec3(arg_obj, "val", val.vec3_val);
                    break;
                case GS_VAL_VOID: 
                case GS_VAL_LIST: 
                case GS_VAL_STRING: 
                case GS_VAL_FN: 
                case GS_VAL_C_FN: 
                case GS_VAL_ERROR: 
                case GS_VAL_NUM_TYPES: {
                    assert(false);
                    break;
                }
            }

            json_array_append_value(args_arr, arg_val);
        }

        json_object_set_value(generator_data_obj, "args", args_val);
        json_object_set_value(geo_obj, "generator_data", generator_data_val);
    }

    json_object_set_value(obj, name, geo_val);

    vec_deinit(&num_points_not_active);
}

static void _golf_json_object_set_transform(JSON_Object *obj, const char *name, golf_transform_t *transform) {
    JSON_Value *transform_val = json_value_init_object();
    JSON_Object *transform_obj = json_value_get_object(transform_val);

    golf_json_object_set_vec3(transform_obj, "position", transform->position);
    golf_json_object_set_vec3(transform_obj, "scale", transform->scale);
    golf_json_object_set_quat(transform_obj, "rotation", transform->rotation);

    json_object_set_value(obj, name, transform_val);
}

static void _golf_json_object_set_lightmap_section(JSON_Object *obj, const char *name, golf_lightmap_section_t *lightmap_section) {
    JSON_Value *lightmap_section_val = json_value_init_object();
    JSON_Object *lightmap_section_obj = json_value_get_object(lightmap_section_val);

    json_object_set_string(lightmap_section_obj, "lightmap_name", lightmap_section->lightmap_name);
    golf_json_object_set_float_array(lightmap_section_obj, "uvs", (float*)lightmap_section->uvs.data, 2 * lightmap_section->uvs.length, 0, 1); 
    json_object_set_value(obj, name, lightmap_section_val);
}

golf_movement_t golf_movement_none(void) {
    golf_movement_t movement;
    movement.type = GOLF_MOVEMENT_NONE;
    movement.repeats = false;
    movement.t0 = 0;
    movement.length = 0;
    return movement;
}

golf_movement_t golf_movement_linear(float t0, vec3 p0, vec3 p1, float length) {
    golf_movement_t movement;
    movement.type = GOLF_MOVEMENT_LINEAR;
    movement.repeats = true;
    movement.t0 = t0;
    movement.length = length;
    movement.linear.p0 = p0;
    movement.linear.p1 = p1;
    return movement;
}

golf_movement_t golf_movement_spinner(float t0, float length) {
    golf_movement_t movement;
    movement.type = GOLF_MOVEMENT_SPINNER;
    movement.repeats = false;
    movement.t0 = t0;
    movement.length = length;
    return movement;
}

golf_movement_t golf_movement_pendulum(float t0, float length, float theta0, vec3 axis) {
    golf_movement_t movement;
    movement.type = GOLF_MOVEMENT_PENDULUM;
    movement.repeats = true;
    movement.t0 = t0;
    movement.length = length;
    movement.pendulum.theta0 = theta0;
    movement.pendulum.axis = axis;
    return movement;
}

golf_movement_t golf_movement_ramp(float t0, float length, float theta0, float theta1, float transition_length, vec3 axis) {
    golf_movement_t movement;
    movement.type = GOLF_MOVEMENT_RAMP;
    movement.repeats = true;
    movement.t0 = t0;
    movement.length = length;
    movement.ramp.theta0 = theta0;
    movement.ramp.theta1 = theta1;
    movement.ramp.transition_length = transition_length;
    movement.ramp.axis = axis;
    return movement;
}

golf_lightmap_image_t golf_lightmap_image(const char *name, int resolution, int width, int height, float time_length, bool repeats, int num_samples, unsigned char **data, sg_image *sg_image) {
    golf_lightmap_image_t lightmap;
    lightmap.active = true;
    snprintf(lightmap.name, GOLF_MAX_NAME_LEN, "%s", name);
    lightmap.resolution = resolution;
    lightmap.width = width;
    lightmap.height = height;
    lightmap.time_length = time_length;
    lightmap.repeats = repeats;
    lightmap.edited_num_samples = num_samples;
    lightmap.num_samples = num_samples;
    lightmap.data = data;
    lightmap.sg_image = sg_image;
    return lightmap;
}

void golf_lightmap_image_finalize(golf_lightmap_image_t *lightmap) {
    for (int s = 0; s < lightmap->num_samples; s++) {
        unsigned char *sg_image_data = golf_alloc(4 * lightmap->width * lightmap->height);
        for (int i = 0; i < 4 * lightmap->width * lightmap->height; i += 4) {
            sg_image_data[i + 0] = lightmap->data[s][i / 4];
            sg_image_data[i + 1] = lightmap->data[s][i / 4];
            sg_image_data[i + 2] = lightmap->data[s][i / 4];
            sg_image_data[i + 3] = 0xFF;
        }
        sg_image_desc img_desc = {
            .width = lightmap->width,
            .height = lightmap->height,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
            .data.subimage[0][0] = {
                .ptr = sg_image_data,
                .size = 4 * lightmap->width * lightmap->height,
            },
        };
        lightmap->sg_image[s] = sg_make_image(&img_desc);
        golf_free(sg_image_data);
    }
}

golf_lightmap_section_t golf_lightmap_section(const char *lightmap_name, vec_vec2_t uvs) {
    golf_lightmap_section_t section;
    snprintf(section.lightmap_name, GOLF_MAX_NAME_LEN, "%s", lightmap_name);
    section.uvs = uvs;
    return section;
}

void golf_lightmap_section_finalize(golf_lightmap_section_t *section) {
    sg_buffer_desc desc = {
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .data = {
            .size = sizeof(vec2) * section->uvs.length,
            .ptr = section->uvs.data,
        },
    };
    section->sg_uvs_buf = sg_make_buffer(&desc);
}

static void _stbi_write_func(void *context, void *data, int size) {
    vec_char_t *v = (vec_char_t*)context;
    vec_pusharr(v, (char*)data, size);
}

golf_material_t golf_material_texture(const char *name, float friction, float restitution, float vel_scale, const char *texture_path) {
    golf_material_t material;
    material.active = true;
    snprintf(material.name, GOLF_MAX_NAME_LEN, "%s", name);
    material.friction = friction;
    material.restitution = restitution;
    material.vel_scale = vel_scale;
    material.type = GOLF_MATERIAL_TEXTURE;
    snprintf(material.texture_path, GOLF_FILE_MAX_PATH, "%s", texture_path);
    material.texture = golf_data_get_texture(texture_path);
    return material;
}

golf_material_t golf_material_diffuse_color(const char *name, float friction, float vel_scale, float restitution, vec4 color) {
    golf_material_t material;
    material.active = true;
    snprintf(material.name, GOLF_MAX_NAME_LEN, "%s", name);
    material.friction = friction;
    material.restitution = restitution;
    material.vel_scale = vel_scale;
    material.type = GOLF_MATERIAL_DIFFUSE_COLOR;
    material.color = color;
    return material;
}

golf_material_t golf_material_color(const char *name, float friction, float restitution, float vel_scale, vec4 color) {
    golf_material_t material;
    material.active = true;
    snprintf(material.name, GOLF_MAX_NAME_LEN, "%s", name);
    material.friction = friction;
    material.restitution = restitution;
    material.vel_scale = vel_scale;
    material.type = GOLF_MATERIAL_COLOR;
    material.color = color;
    return material;
}

golf_material_t golf_material_environment(const char *name, float friction, float restitution, float vel_scale, const char *texture_path) {
    golf_material_t material;
    material.active = true;
    snprintf(material.name, GOLF_MAX_NAME_LEN, "%s", name);
    material.friction = friction;
    material.restitution = restitution;
    material.vel_scale = vel_scale;
    material.type = GOLF_MATERIAL_ENVIRONMENT;
    snprintf(material.texture_path, GOLF_FILE_MAX_PATH, "%s", texture_path);
    material.texture = golf_data_get_texture(texture_path);
    return material;
}

golf_transform_t golf_transform(vec3 position, vec3 scale, quat rotation) {
    golf_transform_t transform;
    transform.position = position;
    transform.scale = scale;
    transform.rotation = rotation;
    return transform;
}

static void _add_dependency(vec_str_t *data_dependencies, char *path) {
    for (int i = 0; i < data_dependencies->length; i++) {
        if (strcmp(data_dependencies->data[i], path) == 0) {
            return;
        }
    }
    vec_push(data_dependencies, path);
}

bool golf_level_save(golf_level_t *level, const char *path) {
    vec_str_t data_dependencies;
    vec_init(&data_dependencies, "level");
    _add_dependency(&data_dependencies, "data/textures/hole_lightmap.png");

    JSON_Value *json_materials_val = json_value_init_array();
    JSON_Array *json_materials_arr = json_value_get_array(json_materials_val);
    for (int i = 0; i < level->materials.length; i++) {
        golf_material_t *material = &level->materials.data[i];
        if (!material->active) continue;

        JSON_Value *json_material_val = json_value_init_object();
        JSON_Object *json_material_obj = json_value_get_object(json_material_val);
        json_object_set_string(json_material_obj, "name", material->name);
        json_object_set_number(json_material_obj, "friction", material->friction);
        json_object_set_number(json_material_obj, "restitution", material->restitution);
        json_object_set_number(json_material_obj, "vel_scale", material->vel_scale);

        switch (material->type) {
            case GOLF_MATERIAL_TEXTURE: {
                json_object_set_string(json_material_obj, "type", "texture");
                json_object_set_string(json_material_obj, "texture", material->texture_path);
                _add_dependency(&data_dependencies, material->texture_path);
                break;
            }
            case GOLF_MATERIAL_COLOR: {
                json_object_set_string(json_material_obj, "type", "color");
                golf_json_object_set_vec4(json_material_obj, "color", material->color);
                break;
            }
            case GOLF_MATERIAL_DIFFUSE_COLOR: {
                json_object_set_string(json_material_obj, "type", "diffuse_color");
                golf_json_object_set_vec4(json_material_obj, "color", material->color);
                break;
            }
            case GOLF_MATERIAL_ENVIRONMENT: {
                json_object_set_string(json_material_obj, "type", "environment");
                json_object_set_string(json_material_obj, "texture", material->texture_path);
                _add_dependency(&data_dependencies, material->texture_path);
                break;
            }
        }

        json_array_append_value(json_materials_arr, json_material_val);
    }

    JSON_Value *json_lightmap_images_val = json_value_init_array();
    JSON_Array *json_lightmap_images_arr = json_value_get_array(json_lightmap_images_val);
    for (int i = 0; i < level->lightmap_images.length; i++) {
        golf_lightmap_image_t *lightmap_image = &level->lightmap_images.data[i];
        if (!lightmap_image->active) continue;

        JSON_Value *json_lightmap_image_val = json_value_init_object();
        JSON_Object *json_lightmap_image_obj = json_value_get_object(json_lightmap_image_val);
        json_object_set_string(json_lightmap_image_obj, "name", lightmap_image->name);
        json_object_set_number(json_lightmap_image_obj, "resolution", lightmap_image->resolution);
        json_object_set_number(json_lightmap_image_obj, "time_length", lightmap_image->time_length);
        json_object_set_number(json_lightmap_image_obj, "num_samples", lightmap_image->num_samples);
        json_object_set_boolean(json_lightmap_image_obj, "repeats", lightmap_image->repeats);

        JSON_Value *datas_val = json_value_init_array();
        JSON_Array *datas_arr = json_value_get_array(datas_val);
        for (int s = 0; s < lightmap_image->num_samples; s++) {
            vec_char_t png_data;
            vec_init(&png_data, "level");
            stbi_write_png_to_func(_stbi_write_func, &png_data, lightmap_image->width, lightmap_image->height, 1, lightmap_image->data[s], lightmap_image->width);
            golf_json_array_append_data(datas_arr, (unsigned char*)png_data.data, sizeof(unsigned char) * png_data.length);
            vec_deinit(&png_data);
        }
        json_object_set_value(json_lightmap_image_obj, "datas", datas_val);

        json_array_append_value(json_lightmap_images_arr, json_lightmap_image_val);
    }

    vec_int_t entity_saved_idx;
    vec_init(&entity_saved_idx, "level");
    {
        int saved_idx = 0;
        for (int i = 0; i < level->entities.length; i++) {
            vec_push(&entity_saved_idx, saved_idx);
            if (level->entities.data[i].active) saved_idx++;
        }
    }

    JSON_Value *json_entities_val = json_value_init_array();
    JSON_Array *json_entities_arr = json_value_get_array(json_entities_val);
    for (int i = 0; i < level->entities.length; i++) {
        golf_entity_t *entity = &level->entities.data[i];
        if (!entity->active) continue;

        JSON_Value *json_entity_val = json_value_init_object();
        JSON_Object *json_entity_obj = json_value_get_object(json_entity_val);

        int parent_idx = entity->parent_idx;
        if (parent_idx >= 0) {
            parent_idx = entity_saved_idx.data[parent_idx];
        }
        json_object_set_number(json_entity_obj, "parent_idx", parent_idx);
        json_object_set_string(json_entity_obj, "name", entity->name);
        switch (entity->type) {
            case MODEL_ENTITY: {
                golf_model_entity_t *model = &entity->model;
                json_object_set_string(json_entity_obj, "type", "model");
                json_object_set_string(json_entity_obj, "model", model->model_path);
                json_object_set_number(json_entity_obj, "uv_scale", model->uv_scale);
                json_object_set_boolean(json_entity_obj, "ignore_physics", model->ignore_physics);
                _add_dependency(&data_dependencies, model->model_path);
                break;
            }
            case BALL_START_ENTITY: {
                json_object_set_string(json_entity_obj, "type", "ball-start");
                break;
            }
            case HOLE_ENTITY: {
                json_object_set_string(json_entity_obj, "type", "hole");
                break;
            }
            case GEO_ENTITY: {
                json_object_set_string(json_entity_obj, "type", "geo");
                break;
            }
            case GROUP_ENTITY: {
                json_object_set_string(json_entity_obj, "type", "group");
                break;
            }
            case WATER_ENTITY: {
                json_object_set_string(json_entity_obj, "type", "water");
                break;
            }
            case BEGIN_ANIMATION_ENTITY: {
                json_object_set_string(json_entity_obj, "type", "begin_animation");
                break;
            }
            case CAMERA_ZONE_ENTITY: {
                json_object_set_string(json_entity_obj, "type", "camera_zone");
                json_object_set_boolean(json_entity_obj, "towards_hole", entity->camera_zone.towards_hole);
                break;
            }
        }

        golf_geo_t *geo = golf_entity_get_geo(entity);
        if (geo) {
            _golf_json_object_set_geo(json_entity_obj, "geo", geo);
        }

        golf_transform_t *transform = golf_entity_get_transform(entity);
        if (transform) {
            _golf_json_object_set_transform(json_entity_obj, "transform", transform);
        }

        golf_lightmap_section_t *lightmap_section = golf_entity_get_lightmap_section(entity);
        if (lightmap_section) {
            _golf_json_object_set_lightmap_section(json_entity_obj, "lightmap_section", lightmap_section);
        }

        golf_movement_t *movement = golf_entity_get_movement(entity);
        if (movement) {
            _golf_json_object_set_movement(json_entity_obj, "movement", movement);
        }

        json_array_append_value(json_entities_arr, json_entity_val);
    }

    vec_deinit(&entity_saved_idx);

    JSON_Value *json_data_dependencies_val = json_value_init_array();
    JSON_Array *json_data_dependencies_arr = json_value_get_array(json_data_dependencies_val);
    for (int i = 0; i < data_dependencies.length; i++) {
        json_array_append_string(json_data_dependencies_arr, data_dependencies.data[i]);
    }
    vec_deinit(&data_dependencies);

    JSON_Value *json_val = json_value_init_object();
    JSON_Object *json_obj = json_value_get_object(json_val);
    json_object_set_value(json_obj, "data_dependencies", json_data_dependencies_val);
    json_object_set_value(json_obj, "materials", json_materials_val);
    json_object_set_value(json_obj, "lightmap_images", json_lightmap_images_val);
    json_object_set_value(json_obj, "entities", json_entities_val);
    json_serialize_to_file_pretty(json_val, path);
    json_value_free(json_val);
    return true;
}

bool golf_level_unload(golf_level_t *level) {
    GOLF_UNUSED(level);
    return false;
}

mat4 golf_transform_get_model_mat(golf_transform_t transform) {
    return mat4_multiply_n(3,
            mat4_translation(transform.position),
            mat4_from_quat(transform.rotation),
            mat4_scale(transform.scale));
}

golf_transform_t golf_transform_apply_movement(golf_transform_t transform, golf_movement_t movement, float t) {
    float l = movement.length;
    t = fmodf(movement.t0 + t, l);
    golf_transform_t new_transform = transform;
    switch (movement.type) {
        case GOLF_MOVEMENT_NONE:
            break;
        case GOLF_MOVEMENT_LINEAR: {
            vec3 p0 = movement.linear.p0;
            vec3 p1 = movement.linear.p1;
            vec3 p = transform.position;
            if (t < 0.5f * l) {
                p = vec3_add(p, vec3_interpolate(p0, p1, t / (0.5f * l)));
            }
            else {
                p = vec3_add(p, vec3_interpolate(p0, p1, (l - t) / (0.5f * l)));
            }
            new_transform.position = p;
            break;
        }
        case GOLF_MOVEMENT_SPINNER: {
            float a = 2 * MF_PI * (t / l);
            quat r = quat_create_from_axis_angle(V3(0, 1, 0), a);
            new_transform.rotation = quat_multiply(r, transform.rotation);
            break;
        }
        case GOLF_MOVEMENT_PENDULUM: {
            float a = 2 * (t / l);
            if (a >= 1) a = 2 - a;

            float theta = movement.pendulum.theta0 * cosf(MF_PI * a);
            quat r = quat_create_from_axis_angle(movement.pendulum.axis, theta);
            new_transform.rotation = quat_multiply(r, transform.rotation);
            break;
        }
        case GOLF_MOVEMENT_RAMP: {
            float theta0 = 2.0f * MF_PI * (movement.ramp.theta0 / 360.0f);
            float theta1 = 2.0f * MF_PI * (movement.ramp.theta1 / 360.0f);
            float transition_length = movement.ramp.transition_length;
            vec3 axis = movement.ramp.axis;
            float theta = 0;

            float s1 = transition_length;
            float s2 = 0.5f * l - transition_length;
            float s3 = 0.5f * l + transition_length;
            float s4 = l - transition_length;

            // stuck at bottom
            if (t < s1) {
                theta = theta0;
            }
            // going up
            else if (t < s2) {
                float a = (t - s1) / (s2 - s1);
                theta = theta0 + a * (theta1 - theta0);
            }
            // stuck at top
            else if (t < s3) {
                theta = theta1;
            }
            // going down
            else if (t < s4) {
                float a = (t - s3) / (s4 - s3);
                theta = theta1 + a * (theta0 - theta1);
            }
            // stuck at bottom
            else {
                theta = theta0;
            }

            quat r = quat_create_from_axis_angle(axis, theta);
            new_transform.rotation = quat_multiply(r, transform.rotation);

            break;
        }
    }
    return new_transform;
}

bool golf_level_get_material(golf_level_t *level, const char *material_name, golf_material_t *out_material) {
    for (int i = 0; i < level->materials.length; i++) {
        golf_material_t *material = &level->materials.data[i];
        if (!material->active) continue;
        if (strcmp(material->name, material_name) == 0) {
            *out_material = *material;
            return true;
        }
    }
    return false;
}

bool golf_level_get_lightmap_image(golf_level_t *level, const char *lightmap_name, golf_lightmap_image_t *out_lightmap_image) {
    for (int i = 0; i < level->lightmap_images.length; i++) {
        golf_lightmap_image_t *lightmap_image = &level->lightmap_images.data[i];
        if (!lightmap_image->active) continue;
        if (strcmp(lightmap_image->name, lightmap_name) == 0) {
            *out_lightmap_image = *lightmap_image;
            return true;
        }
    }
    return false;
}

golf_entity_t golf_entity_model(const char *name, golf_transform_t transform, const char *model_path, float uv_scale, golf_lightmap_section_t lightmap_section, golf_movement_t movement, bool ignore_physics) {
    golf_entity_t entity;
    entity.active = true;
    entity.parent_idx = -1;
    entity.type = MODEL_ENTITY;
    snprintf(entity.name, GOLF_MAX_NAME_LEN, "%s", name);
    entity.model.transform = transform;
    snprintf(entity.model.model_path, GOLF_FILE_MAX_PATH, "%s", model_path);
    entity.model.model = golf_data_get_model(model_path);
    entity.model.uv_scale = uv_scale;
    entity.model.lightmap_section = lightmap_section;
    entity.model.movement = movement;
    entity.model.ignore_physics = ignore_physics;
    return entity;
}

golf_entity_t golf_entity_hole(const char *name, golf_transform_t transform) {
    golf_entity_t entity;
    entity.active = true;
    entity.parent_idx = -1;
    entity.type = HOLE_ENTITY;
    snprintf(entity.name, GOLF_MAX_NAME_LEN, "%s", name);
    entity.hole.transform = transform;
    return entity;
}

golf_entity_t golf_entity_ball_start(const char *name, golf_transform_t transform) {
    golf_entity_t entity;
    entity.active = true;
    entity.parent_idx = -1;
    entity.type = BALL_START_ENTITY;
    snprintf(entity.name, GOLF_MAX_NAME_LEN, "%s", name);
    entity.ball_start.transform = transform;
    return entity;
}

golf_entity_t golf_entity_geo(const char *name, golf_transform_t transform, golf_movement_t movement, golf_geo_t geo, golf_lightmap_section_t lightmap_section) {
    golf_entity_t entity;
    entity.active = true;
    entity.parent_idx = -1;
    entity.type = GEO_ENTITY;
    snprintf(entity.name, GOLF_MAX_NAME_LEN, "%s", name);
    entity.geo.transform = transform;
    entity.geo.movement = movement;
    entity.geo.geo = geo;
    entity.geo.lightmap_section = lightmap_section;
    return entity;
}

golf_entity_t golf_entity_water(const char *name, golf_transform_t transform, golf_geo_t geo, golf_lightmap_section_t lightmap_section) {
    golf_entity_t entity;
    entity.active = true;
    entity.parent_idx = -1;
    entity.type = WATER_ENTITY;
    snprintf(entity.name, GOLF_MAX_NAME_LEN, "%s", name);
    entity.water.transform = transform;
    entity.water.geo = geo;
    entity.water.lightmap_section = lightmap_section;
    return entity;
}

golf_entity_t golf_entity_group(const char *name, golf_transform_t transform) {
    golf_entity_t entity;
    entity.active = true;
    entity.parent_idx = -1;
    entity.type = GROUP_ENTITY;
    snprintf(entity.name, GOLF_MAX_NAME_LEN, "%s", name);
    entity.group.transform = transform;
    return entity;
}

golf_entity_t golf_entity_begin_animation(const char *name, golf_transform_t transform) {
    golf_entity_t entity;
    entity.active = true;
    entity.parent_idx = -1;
    entity.type = BEGIN_ANIMATION_ENTITY;
    snprintf(entity.name, GOLF_MAX_NAME_LEN, "%s", name);
    entity.begin_animation.transform = transform;
    return entity;
}

golf_entity_t golf_entity_camera_zone(const char *name, bool towards_hole, golf_transform_t transform) {
    golf_entity_t entity;
    entity.active = true;
    entity.parent_idx = -1;
    entity.type = CAMERA_ZONE_ENTITY;
    snprintf(entity.name, GOLF_MAX_NAME_LEN, "%s", name);
    entity.camera_zone.towards_hole = towards_hole;
    entity.camera_zone.transform = transform;
    return entity;
}

golf_entity_t golf_entity_make_copy(golf_entity_t *entity) {
    golf_entity_t entity_copy = *entity;

    golf_lightmap_section_t *lightmap_section = golf_entity_get_lightmap_section(entity);
    golf_lightmap_section_t *lightmap_section_copy = golf_entity_get_lightmap_section(&entity_copy);
    if (lightmap_section) {
        vec_vec2_t uvs_copy;
        vec_init(&uvs_copy, "level");
        vec_pusharr(&uvs_copy, lightmap_section->uvs.data, lightmap_section->uvs.length);
        *lightmap_section_copy = golf_lightmap_section(lightmap_section->lightmap_name, uvs_copy);
        golf_lightmap_section_finalize(lightmap_section_copy);
    }

    golf_geo_t *geo = golf_entity_get_geo(entity);
    golf_geo_t *geo_copy = golf_entity_get_geo(&entity_copy);
    if (geo) {
        vec_golf_geo_point_t points_copy;
        vec_init(&points_copy, "geo");
        for (int i = 0; i < geo->points.length; i++) {
            golf_geo_point_t point = geo->points.data[i];
            golf_geo_point_t point_copy = golf_geo_point(point.position);
            vec_push(&points_copy, point_copy);
        }

        vec_golf_geo_face_t faces_copy;
        vec_init(&faces_copy, "geo");
        for (int i = 0; i < geo->faces.length; i++) {
            golf_geo_face_t face = geo->faces.data[i];

            vec_int_t idx_copy;
            vec_init(&idx_copy, "face");
            vec_pusharr(&idx_copy, face.idx.data, face.idx.length);

            vec_vec2_t uvs_copy;
            vec_init(&uvs_copy, "face");
            vec_pusharr(&uvs_copy, face.uvs.data, face.uvs.length);

            golf_geo_face_t face_copy = golf_geo_face(face.material_name, idx_copy, face.uv_gen_type, uvs_copy, face.water_dir);
            vec_push(&faces_copy, face_copy);
        }

        golf_geo_generator_data_t generator_data_copy;
        {
            golf_script_t *script = geo->generator_data.script;
            vec_golf_geo_generator_data_arg_t args;
            vec_init(&args, "geo");
            for (int i = 0; i < geo->generator_data.args.length; i++) {
                vec_push(&args, geo->generator_data.args.data[i]);
            }
            generator_data_copy = golf_geo_generator_data(script, args);
        }

        *geo_copy = golf_geo(points_copy, faces_copy, generator_data_copy, geo->is_water);
        golf_geo_finalize(geo_copy);
    }

    return entity_copy;
}

golf_movement_t *golf_entity_get_movement(golf_entity_t *entity) {
    switch (entity->type) {
        case MODEL_ENTITY: {
            return &entity->model.movement;
        }
        case GEO_ENTITY: {
            return &entity->geo.movement;
        }
        case BALL_START_ENTITY: 
        case WATER_ENTITY: 
        case HOLE_ENTITY: 
        case BEGIN_ANIMATION_ENTITY:
        case CAMERA_ZONE_ENTITY:
        case GROUP_ENTITY: {
            return NULL;
        }
    }
    return NULL;
}

golf_transform_t *golf_entity_get_transform(golf_entity_t *entity) {
    switch (entity->type) {
        case MODEL_ENTITY: {
            return &entity->model.transform;
        }
        case BALL_START_ENTITY: {
            return &entity->ball_start.transform;
        }
        case HOLE_ENTITY: {
            return &entity->hole.transform;
        }
        case GEO_ENTITY: {
            return &entity->geo.transform;
        }
        case WATER_ENTITY: {
            return &entity->water.transform;
        }
        case GROUP_ENTITY: {
            return &entity->group.transform;
        }
        case BEGIN_ANIMATION_ENTITY: {
            return &entity->begin_animation.transform;
        }
        case CAMERA_ZONE_ENTITY: {
            return &entity->camera_zone.transform;
        }
    }
    return NULL;
}

golf_transform_t golf_entity_get_world_transform(golf_level_t *level, golf_entity_t *entity) {
    golf_transform_t parent_transform = golf_transform(V3(0, 0, 0), V3(1, 1, 1), QUAT(0, 0, 0, 1));
    if (entity->parent_idx >= 0) {
        golf_entity_t *parent_entity = &level->entities.data[entity->parent_idx];
        golf_transform_t *transform = golf_entity_get_transform(parent_entity);
        if (transform) {
            parent_transform = *transform;
        }
    }

    golf_transform_t *transform = golf_entity_get_transform(entity);
    if (!transform) {
        golf_log_warning("Attempting to get world transform on entity with no transform");
        return parent_transform;
    }

    quat rotation = quat_multiply(parent_transform.rotation, transform->rotation);

    vec3 scale = parent_transform.scale;
    scale.x *= transform->scale.x;
    scale.y *= transform->scale.y;
    scale.z *= transform->scale.z;

    vec3 position = vec3_apply_mat4(transform->position, 1, mat4_from_quat(parent_transform.rotation));
    position.x *= parent_transform.scale.x;
    position.y *= parent_transform.scale.y;
    position.z *= parent_transform.scale.z;
    position = vec3_add(position, parent_transform.position);

    return golf_transform(position, scale, rotation);
}

golf_lightmap_section_t *golf_entity_get_lightmap_section(golf_entity_t *entity) {
    switch (entity->type) {
        case MODEL_ENTITY: {
            return &entity->model.lightmap_section;
        }
        case GEO_ENTITY: {
            return &entity->geo.lightmap_section;
        }
        case WATER_ENTITY: {
            return &entity->water.lightmap_section;
        }
        case HOLE_ENTITY:
        case BALL_START_ENTITY:
        case BEGIN_ANIMATION_ENTITY:
        case CAMERA_ZONE_ENTITY:
        case GROUP_ENTITY: {
            return NULL;
        }
    }
    return NULL;
}

golf_model_t *golf_entity_get_model(golf_entity_t *entity) {
    switch (entity->type) {
        case MODEL_ENTITY: {
            return entity->model.model;
        }
        case HOLE_ENTITY: {
            return golf_data_get_model("data/models/hole.obj");
        }
        case BALL_START_ENTITY: {
            return golf_data_get_model("data/models/editor/ball_start.obj");
        }
        case GEO_ENTITY: {
            return &entity->geo.geo.model;
        }
        case WATER_ENTITY: {
            return &entity->water.geo.model;
        }
        case CAMERA_ZONE_ENTITY:
        case BEGIN_ANIMATION_ENTITY:
        case GROUP_ENTITY: {
            return NULL;
        }
    }
    return NULL;
}

golf_geo_t *golf_entity_get_geo(golf_entity_t *entity) {
    switch (entity->type) {
        case MODEL_ENTITY: 
        case HOLE_ENTITY: 
        case BALL_START_ENTITY: 
        case BEGIN_ANIMATION_ENTITY:
        case CAMERA_ZONE_ENTITY:
        case GROUP_ENTITY: {
            return NULL;
        }
        case GEO_ENTITY: {
            return &entity->geo.geo;
        }
        case WATER_ENTITY: {
            return &entity->water.geo;
        }
    }
    return NULL;
}

vec3 golf_entity_get_velocity(golf_level_t *level, golf_entity_t *entity, float t, vec3 world_point) {
    float dt = 0.001f;
    golf_movement_t *movement = golf_entity_get_movement(entity);
    if (!movement) {
        return V3(0, 0, 0);
    }

    float t0 = t;
    float t1 = t + dt;

    golf_transform_t world_transform = golf_entity_get_world_transform(level, entity);
    golf_transform_t transform0 = golf_transform_apply_movement(world_transform, *movement, t0);
    golf_transform_t transform1 = golf_transform_apply_movement(world_transform, *movement, t1);
    mat4 model_mat0 = golf_transform_get_model_mat(transform0);
    mat4 model_mat1 = golf_transform_get_model_mat(transform1);
    vec3 local_point = vec3_apply_mat4(world_point, 1, mat4_inverse(model_mat0));
    vec3 world_point1 = vec3_apply_mat4(local_point, 1, model_mat1);

    vec3 velocity = vec3_scale(vec3_sub(world_point1, world_point), 1 / dt);
    return velocity;
}

bool golf_level_get_camera_zone(golf_level_t *level, vec3 pos, golf_camera_zone_entity_t *camera_zone) {
    for (int i = 0; i < level->entities.length; i++) {
        golf_entity_t *entity = &level->entities.data[i];
        if (entity->type != CAMERA_ZONE_ENTITY) {
            continue;
        }

        vec3 cz_pos = entity->camera_zone.transform.position;
        vec3 cz_scale = entity->camera_zone.transform.scale;
        if (pos.x < cz_pos.x + cz_scale.x && pos.x > cz_pos.x - cz_scale.x &&
                pos.z < cz_pos.z + cz_scale.z && pos.z > cz_pos.z - cz_scale.z) {
            *camera_zone = entity->camera_zone;
            return true;
        }
    }
    return false;
}
