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

mat4 golf_model_entity_get_model_mat(golf_model_entity_t *entity) {
    return mat4_multiply_n(3,
            mat4_translation(entity->position),
            mat4_from_quat(entity->orientation),
            mat4_scale(entity->scale));
}
