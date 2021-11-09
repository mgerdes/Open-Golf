#include "golf/level.h"

#include "golf/parson_helper.h"

bool golf_level_save(golf_level_t *level, const char *path) {
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
            case TERRAIN_ENTITY: {
                break;
            }
        }

        json_array_append_value(json_entities_arr, json_entity_val);
    }

    JSON_Value *json_val = json_value_init_object();
    JSON_Object *json_obj = json_value_get_object(json_val);
    json_object_set_value(json_obj, "entities", json_entities_val);
    json_serialize_to_file_pretty(json_val, path);
    json_value_free(json_val);
    return true;
}

bool golf_level_load(golf_level_t *level, const char *path, char *data, int data_len) {
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
