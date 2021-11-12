#include "golf/level.h"

#include "golf/log.h"
#include "golf/parson_helper.h"

bool golf_level_save(golf_level_t *level, const char *path) {
    JSON_Value *json_materials_val = json_value_init_array();
    JSON_Array *json_materials_arr = json_value_get_array(json_materials_val);
    for (int i = 0; i < level->materials.length; i++) {
        golf_material_t *material = &level->materials.data[i];
        JSON_Value *json_material_val = json_value_init_object();
        JSON_Object *json_material_obj = json_value_get_object(json_material_val);
        json_object_set_string(json_material_obj, "name", material->name);
        json_object_set_string(json_material_obj, "texture", material->texture_path);
        json_object_set_number(json_material_obj, "friction", material->friction);
        json_object_set_number(json_material_obj, "restitution", material->restitution);
        json_array_append_value(json_materials_arr, json_material_val);
    }

    JSON_Value *json_entities_val = json_value_init_array();
    JSON_Array *json_entities_arr = json_value_get_array(json_entities_val);
    for (int i = 0; i < level->entities.length; i++) {
        golf_entity_t *entity = &level->entities.data[i];
        JSON_Value *json_entity_val = json_value_init_object();
        JSON_Object *json_entity_obj = json_value_get_object(json_entity_val);

        switch (entity->type) {
            case MODEL_ENTITY: {
                golf_model_entity_t *model = &entity->model;
                json_object_set_string(json_entity_obj, "type", "model");
                json_object_set_string(json_entity_obj, "name", "testing");
                json_object_set_string(json_entity_obj, "model", model->model_path);
                golf_json_object_set_vec3(json_entity_obj, "position", model->transform.position);
                golf_json_object_set_vec3(json_entity_obj, "scale", model->transform.scale);
                golf_json_object_set_quat(json_entity_obj, "rotation", model->transform.rotation);
                break;
            }
            case BALL_START_ENTITY: {
                golf_ball_start_entity_t *ball_start = &entity->ball_start;
                json_object_set_string(json_entity_obj, "type", "ball_start");
                json_object_set_string(json_entity_obj, "name", "testing");
                golf_json_object_set_vec3(json_entity_obj, "position", ball_start->transform.position);
                golf_json_object_set_vec3(json_entity_obj, "scale", ball_start->transform.scale);
                golf_json_object_set_quat(json_entity_obj, "rotation", ball_start->transform.rotation);
                break;
            }
        }

        json_array_append_value(json_entities_arr, json_entity_val);
    }

    JSON_Value *json_val = json_value_init_object();
    JSON_Object *json_obj = json_value_get_object(json_val);
    json_object_set_value(json_obj, "materials", json_materials_val);
    json_object_set_value(json_obj, "entities", json_entities_val);
    json_serialize_to_file_pretty(json_val, path);
    json_value_free(json_val);
    return true;
}

bool golf_level_load(golf_level_t *level, const char *path, char *data, int data_len) {
    vec_init(&level->materials);
    vec_init(&level->entities);

    JSON_Value *json_val = json_parse_string(data);
    JSON_Object *json_obj = json_value_get_object(json_val);
    JSON_Array *json_materials_arr = json_object_get_array(json_obj, "materials");
    for (int i = 0; i < (int)json_array_get_count(json_materials_arr); i++) {
        JSON_Object *obj = json_array_get_object(json_materials_arr, i);
        const char *name = json_object_get_string(obj, "name");
        const char *texture_path = json_object_get_string(obj, "texture");
        float friction = (float)json_object_get_number(obj, "friction");
        float restitution = (float)json_object_get_number(obj, "restitution");

        golf_material_t material;
        snprintf(material.name, GOLF_MATERIAL_NAME_MAX_LEN, "%s", name);
        snprintf(material.texture_path, GOLF_FILE_MAX_PATH, "%s", texture_path);
        material.texture = golf_data_get_texture(material.texture_path);
        material.friction = friction;
        material.restitution = restitution;
        vec_push(&level->materials, material);
    }

    JSON_Array *json_entities_arr = json_object_get_array(json_obj, "entities");
    for (int i = 0; i < (int)json_array_get_count(json_entities_arr); i++) {
        JSON_Object *obj = json_array_get_object(json_entities_arr, i);
        const char *type = json_object_get_string(obj, "type");
        const char *name = json_object_get_string(obj, "name");

        golf_entity_t entity;  
        bool valid_entity = false;
        if (strcmp(type, "model") == 0) {
            const char *model_path = json_object_get_string(obj, "model");
            vec3 position = golf_json_object_get_vec3(obj, "position");
            vec3 scale = golf_json_object_get_vec3(obj, "scale");
            quat rotation = golf_json_object_get_quat(obj, "rotation");

            golf_model_entity_t model;
            snprintf(model.model_path, GOLF_FILE_MAX_PATH, "%s", model_path);
            model.model = golf_data_get_model(model.model_path);
            model.transform.position = position;
            model.transform.scale = scale;
            model.transform.rotation = rotation;

            entity.active = true;
            entity.type = MODEL_ENTITY;
            entity.model = model;
            valid_entity = true;
        }
        else if (strcmp(type, "ball_start") == 0) {
            vec3 position = golf_json_object_get_vec3(obj, "position");
            vec3 scale = golf_json_object_get_vec3(obj, "scale");
            quat rotation = golf_json_object_get_quat(obj, "rotation");

            golf_ball_start_entity_t ball_start;
            ball_start.transform.position = position;
            ball_start.transform.scale = scale;
            ball_start.transform.rotation = rotation;

            entity.active = true;
            entity.type = BALL_START_ENTITY;
            entity.ball_start = ball_start;
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

bool golf_level_get_material(golf_level_t *level, const char *material_name, golf_material_t *material) {
    for (int i = 0; i < level->materials.length; i++) {
        if (strcmp(level->materials.data[i].name, material_name) == 0) {
            *material = level->materials.data[i];
            return true;
        }
    }
    return false;
}

golf_transform_t *golf_entity_get_transform(golf_entity_t *entity) {
    switch (entity->type) {
        case MODEL_ENTITY: {
            return &entity->model.transform;
        }
        case BALL_START_ENTITY: {
            return &entity->ball_start.transform;
        }
    }
    return NULL;
}
