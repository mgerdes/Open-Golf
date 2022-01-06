#include "golf/level.h"

#include <assert.h>

#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#include "golf/alloc.h"
#include "golf/json.h"
#include "golf/log.h"

static const char *_golf_geo_face_uv_gen_type_strings[] = {
    [GOLF_GEO_FACE_UV_GEN_MANUAL] = "manual", 
    [GOLF_GEO_FACE_UV_GEN_GROUND] = "ground", 
    [GOLF_GEO_FACE_UV_GEN_WALL_SIDE] = "wall-side", 
};
static_assert((sizeof(_golf_geo_face_uv_gen_type_strings) / sizeof(_golf_geo_face_uv_gen_type_strings[0])) == GOLF_GEO_FACE_UV_GEN_COUNT, "Invalid number of uv gen type strings");

const char **golf_geo_uv_gen_type_strings(void) {
    return _golf_geo_face_uv_gen_type_strings;
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

golf_geo_face_t golf_geo_face(const char *material_name, int n, int *idx, golf_geo_face_uv_gen_type_t uv_gen_type, vec2 *uvs) {
    golf_geo_face_t face;
    face.active = true;
    snprintf(face.material_name, GOLF_MAX_NAME_LEN, "%s", material_name);
    vec_init(&face.idx, "geo");
    vec_init(&face.uvs, "geo");
    face.uv_gen_type = uv_gen_type;
    for (int i = 0; i < n; i++) {
        vec_push(&face.idx, idx[i]);
        vec_push(&face.uvs, uvs[i]);
    }
    face.start_vertex_in_model = 0;
    return face;
}

golf_geo_point_t golf_geo_point(vec3 position) {
    golf_geo_point_t point;
    point.active = true;
    point.position = position;
    return point;
}

void golf_geo_init(golf_geo_t *geo) {
    golf_geo_generator_data_init(&geo->generator_data);
    vec_init(&geo->points, "level");
    vec_init(&geo->faces, "level");
    geo->model_updated_this_frame = false;
    golf_model_init(&geo->model, 64);
}

void golf_geo_init_cube(golf_geo_t *geo) {
    golf_geo_init(geo);

    vec_push(&geo->points, golf_geo_point(V3(0, 0, 0)));
    vec_push(&geo->points, golf_geo_point(V3(1, 0, 0)));
    vec_push(&geo->points, golf_geo_point(V3(1, 0, 1)));
    vec_push(&geo->points, golf_geo_point(V3(0, 0, 1)));

    vec_push(&geo->points, golf_geo_point(V3(0, 1, 0)));
    vec_push(&geo->points, golf_geo_point(V3(1, 1, 0)));
    vec_push(&geo->points, golf_geo_point(V3(1, 1, 1)));
    vec_push(&geo->points, golf_geo_point(V3(0, 1, 1)));

    vec2 uvs[] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
    golf_geo_face_uv_gen_type_t uv_gen_type = GOLF_GEO_FACE_UV_GEN_MANUAL;
    vec_push(&geo->faces, golf_geo_face("default", 4, (int[]){0, 1, 2, 3}, uv_gen_type, uvs));
    vec_push(&geo->faces, golf_geo_face("default", 4, (int[]){7, 6, 5, 4}, uv_gen_type, uvs));
    vec_push(&geo->faces, golf_geo_face("default", 4, (int[]){7, 4, 0, 3}, uv_gen_type, uvs));
    vec_push(&geo->faces, golf_geo_face("default", 4, (int[]){5, 6, 2, 1}, uv_gen_type, uvs));
    vec_push(&geo->faces, golf_geo_face("default", 4, (int[]){4, 5, 1, 0}, uv_gen_type, uvs));
    vec_push(&geo->faces, golf_geo_face("default", 4, (int[]){6, 7, 3, 2}, uv_gen_type, uvs));
}

typedef map_t(vec_golf_geo_face_ptr_t) _map_vec_golf_geo_face_ptr_t;

void golf_geo_update_model(golf_geo_t *geo) {
    if (geo->model_updated_this_frame) {
        return;
    }
    geo->model_updated_this_frame = true;

    geo->model.groups.length = 0;
    geo->model.positions.length = 0;
    geo->model.normals.length = 0;
    geo->model.texcoords.length = 0;

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
        int group_start = geo->model.positions.length;
        int group_count = 0;
        vec_golf_geo_face_ptr_t *faces = map_get(&material_faces, key);
        for (int i = 0; i < faces->length; i++) {
            golf_geo_face_t *face = faces->data[i];
            face->start_vertex_in_model = geo->model.positions.length;

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
                        // Try to guess what direction this wall is going
                        vec3 dir, p_start;
                        {
                            vec3 dir0 = vec3_sub(p1.position, p0.position);
                            float dir0_l = vec3_length(dir0);
                            dir0 = vec3_scale(dir0, 1.0f / dir0_l);

                            vec3 dir1 = vec3_sub(p2.position, p0.position);
                            float dir1_l = vec3_length(dir1);
                            dir1 = vec3_scale(dir1, 1.0f / dir1_l);

                            vec3 dir2 = vec3_sub(p2.position, p1.position);
                            float dir2_l = vec3_length(dir2);
                            dir2 = vec3_scale(dir2, 1.0f / dir2_l);

                            if (fabsf(dir0.y) <= fabsf(dir1.y) && fabsf(dir0.y) <= fabsf(dir2.y)) {
                                dir = dir0;
                                p_start = p0.position;
                            }
                            else if (fabsf(dir1.y) <= fabsf(dir0.y) && fabsf(dir1.y) <= fabsf(dir2.y)) {
                                dir = dir1;
                                p_start = p0.position;
                            }
                            else if (fabsf(dir2.y) <= fabsf(dir0.y) && fabsf(dir2.y) <= fabsf(dir1.y)) {
                                dir = dir2;
                                p_start = p1.position;
                            }
                            else {
                                golf_log_warning("Couldn't find a direction?????");
                                dir = dir0;
                            }

                            float dot;
                            float dot0 = vec3_dot(dir, V3(1, 0, 0));
                            float dot1 = vec3_dot(dir, V3(0, 0, 1));
                            if (fabsf(dot0) > fabsf(dot1)) {
                                dot = dot0;
                            }
                            else {
                                dot = dot1;
                            }
                            if (dot < 0) {
                                dir = vec3_scale(dir, -1);
                            }
                        }

                        float tc0x = vec3_length(vec3_perpindicular_component(p0.position, dir));
                        float tc1x = vec3_length(vec3_perpindicular_component(p1.position, dir));
                        float tc2x = vec3_length(vec3_perpindicular_component(p2.position, dir));
                        float min_tc = fminf(tc0x, fminf(tc1x, tc2x));

                        tc0.x = tc0x - min_tc;
                        tc0.y = vec3_dot(p0.position, dir);
                        tc1.x = tc1x - min_tc;
                        tc1.y = vec3_dot(p1.position, dir);
                        tc2.x = tc2x - min_tc;
                        tc2.y = vec3_dot(p2.position, dir);
                        tc0 = vec2_scale(tc0, 0.5f);
                        tc1 = vec2_scale(tc1, 0.5f);
                        tc2 = vec2_scale(tc2, 0.5f);
                        break;
                    }
                }
                vec3 n = vec3_normalize(vec3_cross(
                            vec3_sub(p1.position, p0.position), 
                            vec3_sub(p2.position, p0.position)));

                vec_push(&geo->model.positions, p0.position);
                vec_push(&geo->model.positions, p1.position);
                vec_push(&geo->model.positions, p2.position);
                vec_push(&geo->model.normals, n);
                vec_push(&geo->model.normals, n);
                vec_push(&geo->model.normals, n);
                vec_push(&geo->model.texcoords, tc0);
                vec_push(&geo->model.texcoords, tc1);
                vec_push(&geo->model.texcoords, tc2);
                group_count += 3;
            }
        }
        vec_deinit(faces);

        golf_model_group_t group = golf_model_group(key, group_start, group_count);
        vec_push(&geo->model.groups, group);
    }
    map_deinit(&material_faces);

    golf_model_update_buf(&geo->model);
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
            json_object_set_number(movement_obj, "length", movement->length);
            golf_json_object_set_vec3(movement_obj, "p0", movement->linear.p0);
            golf_json_object_set_vec3(movement_obj, "p1", movement->linear.p1);
            break;
        }
        case GOLF_MOVEMENT_SPINNER: {
            json_object_set_string(movement_obj, "type", "spinner");
            json_object_set_number(movement_obj, "length", movement->length);
            break;
        }
    }

    json_object_set_value(obj, name, movement_val);
}

static void _golf_json_object_get_movement(JSON_Object *obj, const char *name, golf_movement_t *movement) {
    JSON_Object *movement_obj = json_object_get_object(obj, name);

    const char *type = json_object_get_string(movement_obj, "type");
    if (type && strcmp(type, "none") == 0) {
        *movement = golf_movement_none();
    }
    else if (type && strcmp(type, "linear") == 0) {
        float length = (float)json_object_get_number(movement_obj, "length");
        vec3 p0 = golf_json_object_get_vec3(movement_obj, "p0");
        vec3 p1 = golf_json_object_get_vec3(movement_obj, "p1");
        *movement = golf_movement_linear(p0, p1, length);
    }
    else if (type && strcmp(type, "spinner") == 0) {
        float length = (float)json_object_get_number(movement_obj, "length");
        *movement = golf_movement_spinner(length);
    }
    else {
        golf_log_warning("Invalid type for movement");
        *movement = golf_movement_none();
    }
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

static void _golf_json_object_get_geo(JSON_Object *obj, const char *name, golf_geo_t *geo) {
    golf_geo_init(geo);
    JSON_Object *geo_obj = json_object_get_object(obj, name);

    JSON_Array *p_arr = json_object_get_array(geo_obj, "p");
    for (int i = 0; i < (int)json_array_get_count(p_arr); i += 3) {
        float x = (float)json_array_get_number(p_arr, i);
        float y = (float)json_array_get_number(p_arr, i + 1);
        float z = (float)json_array_get_number(p_arr, i + 2);
        vec_push(&geo->points, golf_geo_point(V3(x, y, z)));
    }

    JSON_Array *faces_arr = json_object_get_array(geo_obj, "faces");
    for (int i = 0; i < (int)json_array_get_count(faces_arr); i++) {
        JSON_Object *face = json_array_get_object(faces_arr, i);
        const char *material_name = json_object_get_string(face, "material_name");
        JSON_Array *idxs_arr = json_object_get_array(face, "idxs");
        JSON_Array *uvs_arr = json_object_get_array(face, "uvs");
        int idxs_count = (int)json_array_get_count(idxs_arr);
        int *idxs = golf_alloc(sizeof(int) * idxs_count);
        vec2 *uvs = golf_alloc(sizeof(vec2) * idxs_count);
        for (int i = 0; i < idxs_count; i++) {
            idxs[i] = (int)json_array_get_number(idxs_arr, i);
            uvs[i].x = (float)json_array_get_number(uvs_arr, 2*i);
            uvs[i].y = (float)json_array_get_number(uvs_arr, 2*i + 1);
        }
        const char *uv_gen_type_str = json_object_get_string(face, "uv_gen_type");
        golf_geo_face_uv_gen_type_t uv_gen_type = GOLF_GEO_FACE_UV_GEN_MANUAL;
        for (int i = 0; i < GOLF_GEO_FACE_UV_GEN_COUNT; i++) {
            if (uv_gen_type_str && strcmp(uv_gen_type_str, golf_geo_uv_gen_type_strings()[i]) == 0) {
                uv_gen_type = i;
            }
        }
        vec_push(&geo->faces, golf_geo_face(material_name, idxs_count, idxs, uv_gen_type, uvs));
        golf_free(idxs);
        golf_free(uvs);
    }

    JSON_Object *generator_data_obj = json_object_get_object(geo_obj, "generator_data");
    if (generator_data_obj) {
        const char *script_path = json_object_get_string(generator_data_obj, "script");
        geo->generator_data.script = golf_data_get_script(script_path);

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
                vec_push(&geo->generator_data.args, arg);
            }
        }
    }

    golf_geo_update_model(geo);
}

static void _golf_json_object_set_transform(JSON_Object *obj, const char *name, golf_transform_t *transform) {
    JSON_Value *transform_val = json_value_init_object();
    JSON_Object *transform_obj = json_value_get_object(transform_val);

    golf_json_object_set_vec3(transform_obj, "position", transform->position);
    golf_json_object_set_vec3(transform_obj, "scale", transform->scale);
    golf_json_object_set_quat(transform_obj, "rotation", transform->rotation);

    json_object_set_value(obj, name, transform_val);
}

static void _golf_json_object_get_transform(JSON_Object *obj, const char *name, golf_transform_t *transform) {
    JSON_Object *transform_obj = json_object_get_object(obj, name);

    transform->position = golf_json_object_get_vec3(transform_obj, "position");
    transform->scale = golf_json_object_get_vec3(transform_obj, "scale");
    transform->rotation = golf_json_object_get_quat(transform_obj, "rotation");
}

static void _golf_json_object_set_lightmap_section(JSON_Object *obj, const char *name, golf_lightmap_section_t *lightmap_section) {
    JSON_Value *lightmap_section_val = json_value_init_object();
    JSON_Object *lightmap_section_obj = json_value_get_object(lightmap_section_val);

    JSON_Value *uvs_val = json_value_init_array();
    JSON_Array *uvs_arr = json_value_get_array(uvs_val);
    for (int i = 0; i < lightmap_section->uvs.length; i++) {
        json_array_append_number(uvs_arr, lightmap_section->uvs.data[i].x);
        json_array_append_number(uvs_arr, lightmap_section->uvs.data[i].y);
    }

    json_object_set_string(lightmap_section_obj, "lightmap_name", lightmap_section->lightmap_name);
    json_object_set_value(lightmap_section_obj, "uvs", uvs_val);
    json_object_set_value(obj, "lightmap_section", lightmap_section_val);
}

static void _golf_json_object_get_lightmap_section(JSON_Object *obj, const char *name, golf_lightmap_section_t *lightmap_section) {
    JSON_Object *section_obj = json_object_get_object(obj, name);

    const char *lightmap_name = json_object_get_string(section_obj, "lightmap_name");
    JSON_Array *uvs_arr = json_object_get_array(section_obj, "uvs");
    vec_vec2_t uvs;
    vec_init(&uvs, "level");
    for (int i = 0; i < (int)json_array_get_count(uvs_arr); i += 2) {
        float x = (float)json_array_get_number(uvs_arr, i + 0);
        float y = (float)json_array_get_number(uvs_arr, i + 1);
        vec_push(&uvs, V2(x, y));
    }

    golf_lightmap_section_init(lightmap_section, lightmap_name, uvs, 0, uvs.length);
    vec_deinit(&uvs);
}

golf_movement_t golf_movement_none(void) {
    golf_movement_t movement;
    movement.type = GOLF_MOVEMENT_NONE;
    movement.repeats = false;
    movement.t = 0;
    movement.length = 0;
    return movement;
}

golf_movement_t golf_movement_linear(vec3 p0, vec3 p1, float length) {
    golf_movement_t movement;
    movement.type = GOLF_MOVEMENT_LINEAR;
    movement.repeats = true;
    movement.t = 0;
    movement.length = length;
    movement.linear.p0 = p0;
    movement.linear.p1 = p1;
    return movement;
}

golf_movement_t golf_movement_spinner(float length) {
    golf_movement_t movement;
    movement.type = GOLF_MOVEMENT_SPINNER;
    movement.repeats = false;
    movement.t = 0;
    movement.length = length;
    return movement;
}

void golf_lightmap_image_init(golf_lightmap_image_t *lightmap, const char *name, int resolution, int width, int height, float time_length, int num_samples, unsigned char **data) {
    lightmap->active = true;
    snprintf(lightmap->name, GOLF_MAX_NAME_LEN, "%s", name);
    lightmap->resolution = resolution;
    lightmap->width = width;
    lightmap->height = height;
    lightmap->cur_time = 0;
    lightmap->time_length = time_length;
    lightmap->edited_num_samples = num_samples;
    lightmap->num_samples = num_samples;
    lightmap->data = golf_alloc(sizeof(unsigned char*) * num_samples);
    for (int i = 0; i < num_samples; i++) {
        lightmap->data[i] = golf_alloc(width * height);
        memcpy(lightmap->data[i], data[i], width * height);
    }

    lightmap->sg_image = golf_alloc(sizeof(sg_image*) * num_samples);
    for (int s = 0; s < num_samples; s++) {
        char *sg_image_data = golf_alloc(4 * width * height);
        for (int i = 0; i < 4 * width * height; i += 4) {
            sg_image_data[i + 0] = data[s][i / 4];
            sg_image_data[i + 1] = data[s][i / 4];
            sg_image_data[i + 2] = data[s][i / 4];
            sg_image_data[i + 3] = 0xFF;
        }
        sg_image_desc img_desc = {
            .width = width,
            .height = height,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
            .data.subimage[0][0] = {
                .ptr = sg_image_data,
                .size = 4 * width * height,
            },
        };
        lightmap->sg_image[s] = sg_make_image(&img_desc);
        golf_free(sg_image_data);
    }
}

void golf_lightmap_section_init(golf_lightmap_section_t *section, const char *lightmap_name, vec_vec2_t uvs, int start, int count) {
    snprintf(section->lightmap_name, GOLF_MAX_NAME_LEN, "%s", lightmap_name);
    vec_init(&section->uvs, "level");
    vec_pusharr(&section->uvs, uvs.data + start, count);
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

golf_material_t golf_material_color(vec4 color) {
    golf_material_t material;
    material.type = GOLF_MATERIAL_COLOR;
    material.color = color;
    return material;
}

golf_material_t golf_material_texture(const char *texture_path) {
    golf_material_t material;
    material.type = GOLF_MATERIAL_TEXTURE;
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

bool golf_level_save(golf_level_t *level, const char *path) {
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

        switch (material->type) {
            case GOLF_MATERIAL_TEXTURE: {
                json_object_set_string(json_material_obj, "type", "texture");
                json_object_set_string(json_material_obj, "texture", material->texture_path);
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

    JSON_Value *json_entities_val = json_value_init_array();
    JSON_Array *json_entities_arr = json_value_get_array(json_entities_val);
    for (int i = 0; i < level->entities.length; i++) {
        golf_entity_t *entity = &level->entities.data[i];
        if (!entity->active) continue;

        JSON_Value *json_entity_val = json_value_init_object();
        JSON_Object *json_entity_obj = json_value_get_object(json_entity_val);

        json_object_set_string(json_entity_obj, "name", entity->name);
        switch (entity->type) {
            case MODEL_ENTITY: {
                golf_model_entity_t *model = &entity->model;
                json_object_set_string(json_entity_obj, "type", "model");
                json_object_set_string(json_entity_obj, "model", model->model_path);
                break;
            }
            case BALL_START_ENTITY: {
                json_object_set_string(json_entity_obj, "type", "ball_start");
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

    JSON_Value *json_val = json_value_init_object();
    JSON_Object *json_obj = json_value_get_object(json_val);
    json_object_set_value(json_obj, "materials", json_materials_val);
    json_object_set_value(json_obj, "lightmap_images", json_lightmap_images_val);
    json_object_set_value(json_obj, "entities", json_entities_val);
    json_serialize_to_file_pretty(json_val, path);
    json_value_free(json_val);
    return true;
}

bool golf_level_load(golf_level_t *level, const char *path, char *data, int data_len) {
    vec_init(&level->lightmap_images, "level");
    vec_init(&level->materials, "level");
    vec_init(&level->entities, "level");

    JSON_Value *json_val = json_parse_string(data);
    JSON_Object *json_obj = json_value_get_object(json_val);
    JSON_Array *json_materials_arr = json_object_get_array(json_obj, "materials");
    for (int i = 0; i < (int)json_array_get_count(json_materials_arr); i++) {
        JSON_Object *obj = json_array_get_object(json_materials_arr, i);
        const char *type = json_object_get_string(obj, "type");
        const char *name = json_object_get_string(obj, "name");
        float friction = (float)json_object_get_number(obj, "friction");
        float restitution = (float)json_object_get_number(obj, "restitution");

        bool valid_material = false;
        golf_material_t material;
        memset(&material, 0, sizeof(golf_material_t));
        material.active = true;
        snprintf(material.name, GOLF_MAX_NAME_LEN, "%s", name);
        material.friction = friction;
        material.restitution = restitution;
        if (type && strcmp(type, "texture") == 0) {
            material.type = GOLF_MATERIAL_TEXTURE;
            const char *texture_path = json_object_get_string(obj, "texture");
            snprintf(material.texture_path, GOLF_FILE_MAX_PATH, "%s", texture_path);
            material.texture = golf_data_get_texture(material.texture_path);
            valid_material = true;
        }
        else if (type && strcmp(type, "color") == 0) {
            material.type = GOLF_MATERIAL_COLOR;
            material.color = golf_json_object_get_vec4(obj, "color");
            valid_material = true;
        }
        else if (type && strcmp(type, "diffuse_color") == 0) {
            material.type = GOLF_MATERIAL_DIFFUSE_COLOR;
            material.color = golf_json_object_get_vec4(obj, "color");
            valid_material = true;
        }
        else if (type && strcmp(type, "environment") == 0) {
            material.type = GOLF_MATERIAL_ENVIRONMENT;
            const char *texture_path = json_object_get_string(obj, "texture");
            snprintf(material.texture_path, GOLF_FILE_MAX_PATH, "%s", texture_path);
            material.texture = golf_data_get_texture(material.texture_path);
            valid_material = true;
        }

        if (valid_material) {
            vec_push(&level->materials, material);
        }
        else {
            golf_log_warning("Invalid material. type: %s, name: %s", type, name);
        }
    }

    JSON_Array *json_lightmap_images_arr = json_object_get_array(json_obj, "lightmap_images");
    for (int i = 0; i < (int)json_array_get_count(json_lightmap_images_arr); i++) {
        JSON_Object *obj = json_array_get_object(json_lightmap_images_arr, i);
        const char *name = json_object_get_string(obj, "name");
        int resolution = (int)json_object_get_number(obj, "resolution");
        float time_length = (float)json_object_get_number(obj, "time_length");

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

        golf_lightmap_image_t lightmap_image;
        golf_lightmap_image_init(&lightmap_image, name, resolution, width, height, time_length, num_samples, image_datas);
        vec_push(&level->lightmap_images, lightmap_image);

        for (int i = 0; i < num_samples; i++) {
            free(image_datas[i]);
        }
        golf_free(image_datas);
    }

    JSON_Array *json_entities_arr = json_object_get_array(json_obj, "entities");
    for (int i = 0; i < (int)json_array_get_count(json_entities_arr); i++) {
        JSON_Object *obj = json_array_get_object(json_entities_arr, i);
        const char *type = json_object_get_string(obj, "type");
        const char *name = json_object_get_string(obj, "name");

        bool valid_entity = false;
        golf_entity_t entity;  
        if (type && strcmp(type, "model") == 0) {
            golf_transform_t transform;
            _golf_json_object_get_transform(obj, "transform", &transform);

            const char *model_path = json_object_get_string(obj, "model");

            golf_lightmap_section_t lightmap_section;
            _golf_json_object_get_lightmap_section(obj, "lightmap_section", &lightmap_section);

            golf_movement_t movement;
            _golf_json_object_get_movement(obj, "movement", &movement);

            entity = golf_entity_model(name, transform, model_path, lightmap_section, movement);
            valid_entity = true;
        }
        else if (type && strcmp(type, "ball_start") == 0) {
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
            _golf_json_object_get_geo(obj, "geo", &geo);

            entity = golf_entity_geo(name, transform, movement, geo);
            valid_entity = true;
        }

        if (valid_entity) {
            vec_push(&level->entities, entity);
        }
        else {
            golf_log_warning("Invalid entity. type: %s, name: %s", type, name);
        }
    }

    json_value_free(json_val);
    return false;
}

bool golf_level_unload(golf_level_t *level) {
    return false;
}

mat4 golf_transform_get_model_mat(golf_transform_t transform) {
    return mat4_multiply_n(3,
            mat4_translation(transform.position),
            mat4_from_quat(transform.rotation),
            mat4_scale(transform.scale));
}

golf_transform_t golf_transform_apply_movement(golf_transform_t transform, golf_movement_t movement) {
    float t = movement.t;
    float l = movement.length;
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

golf_entity_t golf_entity_model(const char *name, golf_transform_t transform, const char *model_path, golf_lightmap_section_t lightmap_section, golf_movement_t movement) {
    golf_entity_t entity;
    entity.active = true;
    entity.type = MODEL_ENTITY;
    snprintf(entity.name, GOLF_MAX_NAME_LEN, "%s", name);
    entity.model.transform = transform;
    snprintf(entity.model.model_path, GOLF_FILE_MAX_PATH, "%s", model_path);
    entity.model.model = golf_data_get_model(model_path);
    entity.model.lightmap_section = lightmap_section;
    entity.model.movement = movement;
    return entity;
}

golf_entity_t golf_entity_hole(const char *name, golf_transform_t transform) {
    golf_entity_t entity;
    entity.active = true;
    entity.type = HOLE_ENTITY;
    snprintf(entity.name, GOLF_MAX_NAME_LEN, "%s", name);
    entity.hole.transform = transform;
    return entity;
}

golf_entity_t golf_entity_ball_start(const char *name, golf_transform_t transform) {
    golf_entity_t entity;
    entity.active = true;
    entity.type = BALL_START_ENTITY;
    snprintf(entity.name, GOLF_MAX_NAME_LEN, "%s", name);
    entity.ball_start.transform = transform;
    return entity;
}

golf_entity_t golf_entity_geo(const char *name, golf_transform_t transform, golf_movement_t movement, golf_geo_t geo) {
    golf_entity_t entity;
    entity.active = true;
    entity.type = GEO_ENTITY;
    snprintf(entity.name, GOLF_MAX_NAME_LEN, "%s", name);
    entity.geo.transform = transform;
    entity.geo.movement = movement;
    entity.geo.geo = geo;
    return entity;
}

golf_entity_t golf_entity_make_copy(golf_entity_t *entity) {
    golf_entity_t entity_copy = *entity;

    golf_lightmap_section_t *lightmap_section = golf_entity_get_lightmap_section(entity);
    golf_lightmap_section_t *lightmap_section_copy = golf_entity_get_lightmap_section(&entity_copy);
    if (lightmap_section) {
        golf_lightmap_section_init(lightmap_section_copy,
                lightmap_section->lightmap_name,
                lightmap_section->uvs,
                0,
                lightmap_section->uvs.length);
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
        case HOLE_ENTITY: {
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
    }
    return NULL;
}

golf_lightmap_section_t *golf_entity_get_lightmap_section(golf_entity_t *entity) {
    switch (entity->type) {
        case MODEL_ENTITY: {
            return &entity->model.lightmap_section;
        }
        case GEO_ENTITY:
        case HOLE_ENTITY:
        case BALL_START_ENTITY: {
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
            return golf_data_get_model("data/models/sphere.obj");
        }
        case GEO_ENTITY: {
            return &entity->geo.geo.model;
        }
    }
    return NULL;
}

golf_geo_t *golf_entity_get_geo(golf_entity_t *entity) {
    switch (entity->type) {
        case MODEL_ENTITY: 
        case HOLE_ENTITY: 
        case BALL_START_ENTITY: {
            return NULL;
        }
        case GEO_ENTITY: {
            return &entity->geo.geo;
        }
    }
    return NULL;
}
