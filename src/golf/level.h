#ifndef _GOLF_LEVEL_H
#define _GOLF_LEVEL_H

#include "3rd_party/vec/vec.h"
#include "golf/file.h"
#include "golf/maths.h"

typedef struct golf_terrain_entity {
	char model_path[GOLF_FILE_MAX_PATH];
	float bounds[6];
	mat4 model_mat;
} golf_terrain_entity_t;
typedef vec_t(golf_terrain_entity_t*) vec_golf_terrain_entity_ptr_t;

typedef enum golf_entity_type {
	TERRAIN_ENTITY,
} golf_entity_type_t;

typedef struct golf_entity {
	golf_entity_type_t type;
} golf_entity_t;
typedef vec_t(golf_entity_t) vec_golf_entity_t;

typedef struct golf_level {
	vec_golf_entity_t entities;
} golf_level_t;

bool golf_level_save(golf_level_t *level, const char *path);
bool golf_level_load(golf_level_t *level, const char *path, char *data, int data_len);
bool golf_level_unload(golf_level_t *level);

#endif
