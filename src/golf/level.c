#include "golf/level.h"

#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#include "golf/log.h"
#include "golf/parson_helper.h"

void golf_lightmap_init(golf_lightmap_t *lightmap, int resolution, int image_width, int image_height, unsigned char *image_data, vec_vec2_t uvs) {
    lightmap->resolution = resolution;
    lightmap->image_width = image_width;
    lightmap->image_height = image_height;
    lightmap->image_data = malloc(image_width * image_height);
    memcpy(lightmap->image_data, image_data, image_width * image_height);

    char *sg_image_data = malloc(4 * image_width * image_height);
    for (int i = 0; i < 4 * image_width * image_height; i += 4) {
        sg_image_data[i + 0] = image_data[i / 4];
        sg_image_data[i + 1] = image_data[i / 4];
        sg_image_data[i + 2] = image_data[i / 4];
        sg_image_data[i + 3] = 0xFF;
    }
    sg_image_desc img_desc = {
        .width = image_width,
        .height = image_height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .data.subimage[0][0] = {
            .ptr = sg_image_data,
            .size = 4 * image_width * image_height,
        },
    };
    lightmap->sg_image = sg_make_image(&img_desc);
    free(sg_image_data);

    vec_init(&lightmap->uvs);
    vec_pusharr(&lightmap->uvs, uvs.data, uvs.length);
    sg_buffer_desc desc = {
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .data = {
            .size = sizeof(vec2) * lightmap->uvs.length,
            .ptr = lightmap->uvs.data,
        },
    };
    lightmap->sg_uvs_buf = sg_make_buffer(&desc);
}

static void _golf_json_object_get_lightmap(JSON_Object *obj, const char *name, golf_lightmap_t *lightmap) {
    JSON_Object *lightmap_obj = json_object_get_object(obj, name);

    int resolution = (int)json_object_get_number(lightmap_obj, "resolution");
    unsigned char *data;
    int data_len;
    golf_json_object_get_data(lightmap_obj, "data", &data, &data_len);
    int image_width, image_height, c;
    unsigned char *image_data = stbi_load_from_memory(data, data_len, &image_width, &image_height, &c, 1);
    free(data);

    JSON_Array *uvs_arr = json_object_get_array(lightmap_obj, "uvs");
    vec_vec2_t uvs;
    vec_init(&uvs);
    for (int i = 0; i < (int)json_array_get_count(uvs_arr); i += 2) {
        float x = (float)json_array_get_number(uvs_arr, i + 0);
        float y = (float)json_array_get_number(uvs_arr, i + 1);
        vec_push(&uvs, V2(x, y));
    }

    golf_lightmap_init(lightmap, resolution, image_width, image_height, image_data, uvs);

    free(image_data);
    vec_deinit(&uvs);
}

static void _stbi_write_func(void *context, void *data, int size) {
    vec_char_t *v = (vec_char_t*)context;
    vec_pusharr(v, (char*)data, size);
}

static void _golf_json_object_set_lightmap(JSON_Object *obj, const char *name, golf_lightmap_t *lightmap) {
    JSON_Value *lightmap_val = json_value_init_object();
    JSON_Object *lightmap_obj = json_value_get_object(lightmap_val);

    json_object_set_number(lightmap_obj, "resolution", lightmap->resolution);

    vec_char_t png_data;
    vec_init(&png_data);
    stbi_write_png_to_func(_stbi_write_func, &png_data, lightmap->image_width, lightmap->image_height, 1, lightmap->image_data, lightmap->image_width);
    golf_json_object_set_data(lightmap_obj, "data", (unsigned char*)png_data.data, sizeof(unsigned char) * png_data.length);
    vec_deinit(&png_data);

    JSON_Value *uvs_val = json_value_init_array();
    JSON_Array *uvs_arr = json_value_get_array(uvs_val);
    for (int i = 0; i < lightmap->uvs.length; i++) {
        json_array_append_number(uvs_arr, lightmap->uvs.data[i].x);
        json_array_append_number(uvs_arr, lightmap->uvs.data[i].y);
    }
    json_object_set_value(lightmap_obj, "uvs", uvs_val);

    json_object_set_value(obj, name, lightmap_val);
}

golf_material_t golf_material_color(vec3 color) {
    golf_material_t material;
    material.type = GOLF_MATERIAL_COLOR;
    material.color = color;
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
        if (!material->active) {
            continue;
        }

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
                golf_json_object_set_vec3(json_material_obj, "color", material->color);
                break;
            }
            case GOLF_MATERIAL_DIFFUSE_COLOR: {
                json_object_set_string(json_material_obj, "type", "diffuse_color");
                golf_json_object_set_vec3(json_material_obj, "color", material->color);
                break;
            }
        }

        json_array_append_value(json_materials_arr, json_material_val);
    }

    JSON_Value *json_entities_val = json_value_init_array();
    JSON_Array *json_entities_arr = json_value_get_array(json_entities_val);
    for (int i = 0; i < level->entities.length; i++) {
        golf_entity_t *entity = &level->entities.data[i];
        if (!entity->active) {
            continue;
        }

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
            case HOLE_ENTITY: {
                golf_hole_entity_t *hole = &entity->hole;
                json_object_set_string(json_entity_obj, "type", "hole");
                json_object_set_string(json_entity_obj, "name", "testing");
                golf_json_object_set_vec3(json_entity_obj, "position", hole->transform.position);
                golf_json_object_set_vec3(json_entity_obj, "scale", hole->transform.scale);
                golf_json_object_set_quat(json_entity_obj, "rotation", hole->transform.rotation);
                break;
            }
        }

        golf_transform_t *transform = golf_entity_get_transform(entity);
        if (transform) {
            golf_json_object_set_transform(json_entity_obj, "transform", transform);
        }

        golf_lightmap_t *lightmap = golf_entity_get_lightmap(entity);
        if (lightmap) {
            _golf_json_object_set_lightmap(json_entity_obj, "lightmap", lightmap);
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
        const char *type = json_object_get_string(obj, "type");
        const char *name = json_object_get_string(obj, "name");
        float friction = (float)json_object_get_number(obj, "friction");
        float restitution = (float)json_object_get_number(obj, "restitution");

        bool valid_material = false;
        golf_material_t material;
        memset(&material, 0, sizeof(golf_material_t));
        material.active = true;
        snprintf(material.name, GOLF_MATERIAL_NAME_MAX_LEN, "%s", name);
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
            material.color = golf_json_object_get_vec3(obj, "color");
            valid_material = true;
        }
        else if (type && strcmp(type, "diffuse_color") == 0) {
            material.type = GOLF_MATERIAL_DIFFUSE_COLOR;
            material.color = golf_json_object_get_vec3(obj, "color");
            valid_material = true;
        }

        if (valid_material) {
            vec_push(&level->materials, material);
        }
        else {
            golf_log_warning("Invalid material. type: %s, name: %s", type, name);
        }
    }

    JSON_Array *json_entities_arr = json_object_get_array(json_obj, "entities");
    for (int i = 0; i < (int)json_array_get_count(json_entities_arr); i++) {
        JSON_Object *obj = json_array_get_object(json_entities_arr, i);
        const char *type = json_object_get_string(obj, "type");
        const char *name = json_object_get_string(obj, "name");

        bool valid_entity = false;
        golf_entity_t entity;  
        if (type && strcmp(type, "model") == 0) {
            vec3 position = golf_json_object_get_vec3(obj, "position");
            vec3 scale = golf_json_object_get_vec3(obj, "scale");
            quat rotation = golf_json_object_get_quat(obj, "rotation");
            golf_transform_t transform = golf_transform(position, scale, rotation);

            const char *model_path = json_object_get_string(obj, "model");

            golf_lightmap_t lightmap;
            _golf_json_object_get_lightmap(obj, "lightmap", &lightmap);

            entity = golf_entity_model(transform, model_path, lightmap);
            valid_entity = true;
        }
        else if (type && strcmp(type, "ball_start") == 0) {
            vec3 position = golf_json_object_get_vec3(obj, "position");
            vec3 scale = golf_json_object_get_vec3(obj, "scale");
            quat rotation = golf_json_object_get_quat(obj, "rotation");
            golf_transform_t transform = golf_transform(position, scale, rotation);

            entity = golf_entity_ball_start(transform);
            valid_entity = true;
        }
        else if (type && strcmp(type, "hole") == 0) {
            vec3 position = golf_json_object_get_vec3(obj, "position");
            vec3 scale = golf_json_object_get_vec3(obj, "scale");
            quat rotation = golf_json_object_get_quat(obj, "rotation");
            golf_transform_t transform = golf_transform(position, scale, rotation);

            golf_lightmap_t lightmap;
            _golf_json_object_get_lightmap(obj, "lightmap", &lightmap);

            entity = golf_entity_hole(transform, lightmap);
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
        if (!level->materials.data[i].active) {
            continue;
        }
        if (strcmp(level->materials.data[i].name, material_name) == 0) {
            *material = level->materials.data[i];
            return true;
        }
    }
    return false;
}

golf_entity_t golf_entity_model(golf_transform_t transform, const char *model_path, golf_lightmap_t lightmap) {
    golf_entity_t entity;
    entity.active = true;
    entity.type = MODEL_ENTITY;
    entity.model.transform = transform;
    snprintf(entity.model.model_path, GOLF_FILE_MAX_PATH, "%s", model_path);
    entity.model.model = golf_data_get_model(model_path);
    entity.model.lightmap = lightmap;
    return entity;
}

golf_entity_t golf_entity_hole(golf_transform_t transform, golf_lightmap_t lightmap) {
    golf_entity_t entity;
    entity.active = true;
    entity.type = HOLE_ENTITY;
    entity.hole.transform = transform;
    entity.hole.lightmap = lightmap;
    return entity;
}

golf_entity_t golf_entity_ball_start(golf_transform_t transform) {
    golf_entity_t entity;
    entity.active = true;
    entity.type = BALL_START_ENTITY;
    entity.ball_start.transform = transform;
    return entity;
}

golf_entity_t golf_entity_make_copy(golf_entity_t *entity) {
    golf_entity_t entity_copy = *entity;

    golf_lightmap_t *lightmap = golf_entity_get_lightmap(entity);
    golf_lightmap_t *lightmap_copy = golf_entity_get_lightmap(&entity_copy);
    if (lightmap) {
        golf_lightmap_init(lightmap_copy, 
                lightmap->resolution,
                lightmap->image_width,
                lightmap->image_height,
                lightmap->image_data,
                lightmap->uvs);
    }

    return entity_copy;
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
    }
    return NULL;
}

golf_lightmap_t *golf_entity_get_lightmap(golf_entity_t *entity) {
    switch (entity->type) {
        case MODEL_ENTITY: {
            return &entity->model.lightmap;
        }
        case HOLE_ENTITY: {
            return &entity->hole.lightmap;
        }
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
    }
    return NULL;
}
