#include "golf/level.h"

bool golf_level_save(golf_level_t *level, const char *path) {
    return false;
}

bool golf_level_load(golf_level_t *level, const char *path, char *data, int data_len) {
    return false;
}

bool golf_level_unload(golf_level_t *level) {
    return false;
}

mat4 golf_entity_get_model_mat(golf_entity_t *entity) {
    switch (entity->type) {
        case MODEL_ENTITY:
            return entity->model_entity.model_mat;
            break;
        case TERRAIN_ENTITY:
            return entity->terrain_entity.model_mat;
            break;
    }
    return mat4_identity();
}

mat4 *golf_entity_get_model_mat_ptr(golf_entity_t *entity) {
    switch (entity->type) {
        case MODEL_ENTITY:
            return &entity->model_entity.model_mat;
            break;
        case TERRAIN_ENTITY:
            return &entity->terrain_entity.model_mat;
            break;
    }
    return NULL;
}

void golf_entity_set_model_mat(golf_entity_t *entity, mat4 model_mat) {
    switch (entity->type) {
        case MODEL_ENTITY:
            entity->model_entity.model_mat = model_mat;
            break;
        case TERRAIN_ENTITY:
            entity->terrain_entity.model_mat = model_mat;
            break;
    }
}
