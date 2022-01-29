#include "golf/ui.h"

#include <assert.h>
#include "parson/parson.h"
#include "golf/alloc.h"
#include "golf/config.h"
#include "golf/data.h"
#include "golf/inputs.h"
#include "golf/json.h"
#include "golf/log.h"
#include "golf/renderer.h"

static golf_ui_t ui;
static golf_inputs_t *inputs; 
static golf_renderer_t *renderer; 

static golf_ui_draw_entity_t _golf_ui_draw_entity(sg_image image, vec2 pos, vec2 size, vec2 uv0, vec2 uv1, float is_font, vec4 overlay_color) {
    golf_ui_draw_entity_t entity;
    entity.image = image;
    entity.pos = pos;
    entity.size = size;
    entity.uv0 = uv0;
    entity.uv1 = uv1;
    entity.is_font = is_font;
    entity.overlay_color = overlay_color;
    return entity;
}

static bool _golf_ui_layout_load_entity(JSON_Object *entity_obj, golf_ui_entity_t *entity) {
    const char *type = json_object_get_string(entity_obj, "type");
    if (!type) {
        golf_log_warning("No ui entity type %s", type);
        return false;
    }

    const char *name = json_object_get_string(entity_obj, "name");
    if (name) {
        snprintf(entity->name, GOLF_MAX_NAME_LEN, "%s", name);
    }
    else {
        entity->name[0] = 0;
    }

    const char *parent_name = json_object_get_string(entity_obj, "parent");
    if (parent_name) {
        snprintf(entity->parent_name, GOLF_MAX_NAME_LEN, "%s", parent_name);
    }
    else {
        entity->parent_name[0] = 0;
    }

    entity->pos = golf_json_object_get_vec2(entity_obj, "pos");
    entity->size = golf_json_object_get_vec2(entity_obj, "size");
    entity->anchor = golf_json_object_get_vec2(entity_obj, "anchor");

    if (strcmp(type, "pixel_pack_square") == 0) {
        const char *pixel_pack_path = json_object_get_string(entity_obj, "pixel_pack");
        golf_data_load(pixel_pack_path);
        const char *square_name = json_object_get_string(entity_obj, "square");

        entity->type = GOLF_UI_PIXEL_PACK_SQUARE;
        entity->pixel_pack_square.pixel_pack = golf_data_get_pixel_pack(pixel_pack_path);
        snprintf(entity->pixel_pack_square.square_name, GOLF_MAX_NAME_LEN, "%s", square_name);
        entity->pixel_pack_square.tile_size = (float)json_object_get_number(entity_obj, "tile_size");
        entity->pixel_pack_square.overlay_color = golf_json_object_get_vec4(entity_obj, "overlay_color");
    }
    else if (strcmp(type, "text") == 0) {
        const char *font_path = json_object_get_string(entity_obj, "font");
        golf_data_load(font_path);
        const char *text = json_object_get_string(entity_obj, "text");

        entity->type = GOLF_UI_TEXT;
        entity->text.font = golf_data_get_font(font_path);
        golf_string_init(&entity->text.text, "ui_layout", text);
        entity->text.font_size = (float)json_object_get_number(entity_obj, "font_size");
        entity->text.color = golf_json_object_get_vec4(entity_obj, "color");
        entity->text.horiz_align = (int)json_object_get_number(entity_obj, "horiz_align");
        entity->text.vert_align = (int)json_object_get_number(entity_obj, "vert_align");
    }
    else if (strcmp(type, "button") == 0) {
        vec_init(&entity->button.up_entities, "ui_layout");
        vec_init(&entity->button.down_entities, "ui_layout");

        entity->type = GOLF_UI_BUTTON;

        JSON_Array *up_entities_arr = json_object_get_array(entity_obj, "up_entities");
        for (int i = 0; i < (int)json_array_get_count(up_entities_arr); i++) {
            JSON_Object *up_entity_obj = json_array_get_object(up_entities_arr, i);
            golf_ui_entity_t up_entity;
            if (_golf_ui_layout_load_entity(up_entity_obj, &up_entity)) {
                vec_push(&entity->button.up_entities, up_entity);
            }
        }

        JSON_Array *down_entities_arr = json_object_get_array(entity_obj, "down_entities");
        for (int i = 0; i < (int)json_array_get_count(down_entities_arr); i++) {
            JSON_Object *down_entity_obj = json_array_get_object(down_entities_arr, i);
            golf_ui_entity_t down_entity;
            if (_golf_ui_layout_load_entity(down_entity_obj, &down_entity)) {
                vec_push(&entity->button.down_entities, down_entity);
            }
        }
    }
    else {
        golf_log_warning("Unknown ui entity type %s", type);
        return false;
    }

    return true;
}

bool golf_ui_layout_load(golf_ui_layout_t *layout, const char *path, char *data, int data_len) {
    vec_init(&layout->entities, "ui_layout");

    JSON_Value *json_val = json_parse_string(data);
    JSON_Object *json_obj = json_value_get_object(json_val);

    JSON_Array *entities_arr = json_object_get_array(json_obj, "entities");
    for (int i = 0; i < (int)json_array_get_count(entities_arr); i++) {
        JSON_Object *entity_obj = json_array_get_object(entities_arr, i);

        golf_ui_entity_t entity;
        if (_golf_ui_layout_load_entity(entity_obj, &entity)) {
            vec_push(&layout->entities, entity);
        }
    }

    return true;
}

static void _golf_ui_layout_unload_entity(golf_ui_entity_t *entity) {
    switch (entity->type) {
        case GOLF_UI_PIXEL_PACK_SQUARE:
            break;
        case GOLF_UI_TEXT:
            golf_string_deinit(&entity->text.text);
            break;
        case GOLF_UI_BUTTON:
            for (int i = 0; i < entity->button.up_entities.length; i++) {
                _golf_ui_layout_unload_entity(&entity->button.up_entities.data[i]);
            }
            vec_deinit(&entity->button.up_entities);

            for (int i = 0; i < entity->button.down_entities.length; i++) {
                _golf_ui_layout_unload_entity(&entity->button.down_entities.data[i]);
            }
            vec_deinit(&entity->button.down_entities);
            break;
    }
}

bool golf_ui_layout_unload(golf_ui_layout_t *layout) {
    for (int i = 0; i < layout->entities.length; i++) {
        _golf_ui_layout_unload_entity(&layout->entities.data[i]);
    }
    vec_deinit(&layout->entities);
    return true;
}

golf_ui_t *golf_ui_get(void) {
    return &ui;
}

void golf_ui_init(void) {
    memset(&ui, 0, sizeof(ui));
    vec_init(&ui.entities, "ui");
    vec_init(&ui.draw_entities, "ui");
    ui.state = GOLF_UI_MAIN_MENU;

    inputs = golf_inputs_get();
    renderer = golf_renderer_get();
}

static bool _golf_ui_layout_get_entity(golf_ui_layout_t *layout, const char *name, golf_ui_entity_t *entity) {
    if (!name[0]) {
        return false;
    }

    for (int i = 0; i < layout->entities.length; i++) {
        if (strcmp(layout->entities.data[i].name, name) == 0) {
            *entity = layout->entities.data[i];
            return true;
        }
    }
    return false;
}

static bool _golf_ui_layout_get_entity_of_type(golf_ui_layout_t *layout, const char *name, golf_ui_entity_type_t type, golf_ui_entity_t *entity) {
    if (!name[0]) {
        return false;
    }

    for (int i = 0; i < layout->entities.length; i++) {
        if (strcmp(layout->entities.data[i].name, name) == 0 && layout->entities.data[i].type == type) {
            *entity = layout->entities.data[i];
            return true;
        }
    }
    return false;
}

static vec2 _golf_ui_layout_get_entity_pos(golf_ui_layout_t *layout, golf_ui_entity_t entity) {
    float ui_scale = renderer->viewport_size.x / 1280.0f;

    golf_ui_entity_t parent_entity;
    if (_golf_ui_layout_get_entity(layout, entity.parent_name, &parent_entity)) {
        vec2 parent_pos = _golf_ui_layout_get_entity_pos(layout, parent_entity);

        float sx = parent_entity.size.x;
        float sy = parent_entity.size.y;

        vec2 p;
        p.x = parent_pos.x - ui_scale * 0.5f * sx; 
        p.y = parent_pos.y - ui_scale * 0.5f * sy;

        p.x = p.x + ui_scale * entity.anchor.x * sx;
        p.y = p.y + ui_scale * entity.anchor.y * sy;

        p.x = p.x + ui_scale * entity.pos.x;
        p.y = p.y + ui_scale * entity.pos.y;

        return p;
    }
    else {
        float sx = renderer->viewport_size.x;
        float sy = renderer->viewport_size.y;

        vec2 p = V2(sx * entity.anchor.x, sy * entity.anchor.y);
        p = vec2_add(p, vec2_scale(entity.pos, ui_scale));
        return p;
    }
}

static void _golf_ui_pixel_pack_square_section(vec2 pos, vec2 size, float tile_size, vec4 overlay_color, golf_pixel_pack_t *pixel_pack, golf_pixel_pack_square_t *pixel_pack_square, int x, int y) {
    float px = pos.x + x * (0.5f * size.x - 0.5f * tile_size);
    float py = pos.y - y * (0.5f * size.y - 0.5f * tile_size);

    float sx = tile_size;
    float sy = tile_size;
    if (x == 0) {
        sx = size.x - 2.0f;
    }
    if (y == 0) {
        sy = size.y - 2.0f;
    }

    vec2 uv0, uv1;
    if (x == -1 && y == -1) {
        uv0 = pixel_pack_square->bl_uv0;
        uv1 = pixel_pack_square->bl_uv1;
    }
    else if (x == -1 && y == 0) {
        uv0 = pixel_pack_square->ml_uv0;
        uv1 = pixel_pack_square->ml_uv1;
    }
    else if (x == -1 && y == 1) {
        uv0 = pixel_pack_square->tl_uv0;
        uv1 = pixel_pack_square->tl_uv1;
    }
    else if (x == 0 && y == -1) {
        uv0 = pixel_pack_square->bm_uv0;
        uv1 = pixel_pack_square->bm_uv1;
    }
    else if (x == 0 && y == 0) {
        uv0 = pixel_pack_square->mm_uv0;
        uv1 = pixel_pack_square->mm_uv1;
    }
    else if (x == 0 && y == 1) {
        uv0 = pixel_pack_square->tm_uv0;
        uv1 = pixel_pack_square->tm_uv1;
    }
    else if (x == 1 && y == -1) {
        uv0 = pixel_pack_square->br_uv0;
        uv1 = pixel_pack_square->br_uv1;
    }
    else if (x == 1 && y == 0) {
        uv0 = pixel_pack_square->mr_uv0;
        uv1 = pixel_pack_square->mr_uv1;
    }
    else if (x == 1 && y == 1) {
        uv0 = pixel_pack_square->tr_uv0;
        uv1 = pixel_pack_square->tr_uv1;
    }
    else {
        golf_log_warning("Invalid x and y for pixel pack square section");
        return;
    }

    vec_push(&ui.draw_entities, _golf_ui_draw_entity(pixel_pack->texture->sg_image, V2(px, py), V2(sx, sy), uv0, uv1, 0, overlay_color));
}

static void _golf_ui_pixel_pack_square(golf_ui_layout_t *layout, golf_ui_entity_t entity) {
    golf_pixel_pack_t *pixel_pack = entity.pixel_pack_square.pixel_pack;
    golf_pixel_pack_square_t *pixel_pack_square = map_get(&pixel_pack->squares, entity.pixel_pack_square.square_name);
    if (!pixel_pack_square) {
        golf_log_warning("Could not find pixel pack square %s", entity.pixel_pack_square.square_name);
        return;
    }

    vec2 vp_size = renderer->viewport_size;
    float ui_scale = vp_size.x / 1280.0f;
    vec2 pos = _golf_ui_layout_get_entity_pos(layout, entity);
    vec2 size = vec2_scale(entity.size, ui_scale);
    float tile_size = ui_scale * entity.pixel_pack_square.tile_size;
    vec4 overlay_color = entity.pixel_pack_square.overlay_color;

    _golf_ui_pixel_pack_square_section(pos, size, tile_size, overlay_color, pixel_pack, pixel_pack_square, 0, 0);
    _golf_ui_pixel_pack_square_section(pos, size, tile_size, overlay_color, pixel_pack, pixel_pack_square, 0, -1);
    _golf_ui_pixel_pack_square_section(pos, size, tile_size, overlay_color, pixel_pack, pixel_pack_square, 0, 1);
    _golf_ui_pixel_pack_square_section(pos, size, tile_size, overlay_color, pixel_pack, pixel_pack_square, -1, 0);
    _golf_ui_pixel_pack_square_section(pos, size, tile_size, overlay_color, pixel_pack, pixel_pack_square, 1, 0);
    _golf_ui_pixel_pack_square_section(pos, size, tile_size, overlay_color, pixel_pack, pixel_pack_square, -1, -1);
    _golf_ui_pixel_pack_square_section(pos, size, tile_size, overlay_color, pixel_pack, pixel_pack_square, 1, -1);
    _golf_ui_pixel_pack_square_section(pos, size, tile_size, overlay_color, pixel_pack, pixel_pack_square, -1, 1);
    _golf_ui_pixel_pack_square_section(pos, size, tile_size, overlay_color, pixel_pack, pixel_pack_square, 1, 1);
}

static void _golf_ui_pixel_pack_square_name(golf_ui_layout_t *layout, const char *name) {
    golf_ui_entity_t entity;
    if (!_golf_ui_layout_get_entity_of_type(layout, name, GOLF_UI_PIXEL_PACK_SQUARE, &entity)) {
        golf_log_warning("Could not find pixel pack square entity %s.", name);
        return;
    }

    _golf_ui_pixel_pack_square(layout, entity);
}

static void _golf_ui_text(golf_ui_layout_t *layout, golf_ui_entity_t entity) {
    golf_font_t *font = entity.text.font;
    vec2 vp_size = renderer->viewport_size;
    float ui_scale = vp_size.x / 1280.0f;
    vec2 pos = _golf_ui_layout_get_entity_pos(layout, entity);

    float cur_x = 0;
    float cur_y = 0;
    int sz_idx = 0;
    float font_size = ui_scale * entity.text.font_size;
    for (int idx = 1; idx < font->atlases.length; idx++) {
        if (fabsf(font->atlases.data[idx].font_size - font_size) <
                fabsf(font->atlases.data[sz_idx].font_size - font_size)) {
            sz_idx = idx;
        }
    }
    golf_font_atlas_t atlas = font->atlases.data[sz_idx];
    float sz_scale = font_size / atlas.font_size;

    float width = 0.0f;
    int i = 0;
    while (entity.text.text.cstr[i]) {
        char c = entity.text.text.cstr[i];
        width += sz_scale * atlas.char_data[(int)c].xadvance;
        i++;
    }

    if (entity.text.horiz_align == 0) {
        cur_x -= 0.5f * width;
    }
    else if (entity.text.horiz_align < 0) {
    }
    else if (entity.text.horiz_align > 0) {
        cur_x -= width;
    }
    else {
        golf_log_warning("Invalid text horizontal_alignment %s", entity.text.horiz_align);
    }

    if (entity.text.vert_align == 0) {
        cur_y += 0.5f * (atlas.ascent + atlas.descent);
    }
    else if (entity.text.vert_align > 0) {
    }
    else if (entity.text.vert_align < 0) {
        cur_y += (atlas.ascent + atlas.descent);
    }
    else {
        golf_log_warning("Invalid text vert_align %s", entity.text.vert_align);
    }

    i = 0;
    while (entity.text.text.cstr[i]) {
        char c = entity.text.text.cstr[i];

        int x0 = (int)atlas.char_data[(int)c].x0;
        int x1 = (int)atlas.char_data[(int)c].x1;
        int y0 = (int)atlas.char_data[(int)c].y0;
        int y1 = (int)atlas.char_data[(int)c].y1;

        float xoff = atlas.char_data[(int)c].xoff;
        float yoff = atlas.char_data[(int)c].yoff;
        float xadvance = atlas.char_data[(int)c].xadvance;

        int round_x = (int)floor((cur_x + xoff) + 0.5f);
        int round_y = (int)floor((cur_y + yoff) + 0.5f);

        float qx0 = (float)round_x; 
        float qy0 = (float)round_y;
        float qx1 = (float)(round_x + x1 - x0);
        float qy1 = (float)(round_y + (y1 - y0));

        float px = pos.x + qx0 + 0.5f * (qx1 - qx0);
        float py = pos.y + qy0 + 0.5f * (qy1 - qy0);

        float sx = sz_scale * (qx1 - qx0);
        float sy = sz_scale * (qy1 - qy0);

        vec2 uv0 = V2((float)x0 / atlas.size, (float)y0 / atlas.size);
        vec2 uv1 = V2((float)x1 / atlas.size, (float)y1 / atlas.size);

        vec4 overlay_color = entity.text.color;

        vec_push(&ui.draw_entities, _golf_ui_draw_entity(atlas.sg_image, V2(px, py), V2(sx, sy), uv0, uv1, 1, overlay_color));

        cur_x += sz_scale * xadvance;

        i++;
    }
}

static void _golf_ui_text_name(golf_ui_layout_t *layout, const char *name) {
    golf_ui_entity_t entity;
    if (!_golf_ui_layout_get_entity_of_type(layout, name, GOLF_UI_TEXT, &entity)) {
        golf_log_warning("Could not find text entity %s.", name);
        return;
    }
    _golf_ui_text(layout, entity);
}

static void _golf_ui_button_name(golf_ui_layout_t *layout, const char *name) {
    golf_ui_entity_t entity;
    if (!_golf_ui_layout_get_entity_of_type(layout, name, GOLF_UI_BUTTON, &entity)) {
        golf_log_warning("Could not find button entity %s.", name);
        return;
    }

    float ui_scale = renderer->viewport_size.x / 1280.0f;
    vec2 pos = _golf_ui_layout_get_entity_pos(layout, entity);
    vec2 size = vec2_scale(entity.size, ui_scale);
    vec2 mp = inputs->mouse_pos;
    vec_golf_ui_entity_t entities;
    if (pos.x - 0.5f * size.x <= mp.x && pos.x + 0.5f * size.x >= mp.x &&
            pos.y - 0.5f * size.y <= mp.y && pos.y + 0.5f * size.y >= mp.y) {
        entities = entity.button.down_entities;
    }
    else {
        entities = entity.button.up_entities;
    }

    for (int i = 0; i < entities.length; i++) {
        golf_ui_entity_t entity = entities.data[i]; 
        switch (entity.type) {
            case GOLF_UI_BUTTON:
                break;
            case GOLF_UI_TEXT:
                _golf_ui_text(layout, entity);
                break;
            case GOLF_UI_PIXEL_PACK_SQUARE:
                _golf_ui_pixel_pack_square(layout, entity);
                break;
        }
    }
}

void golf_ui_update(float dt) {
    ui.draw_entities.length = 0;

    golf_ui_layout_t *layout = golf_data_get_ui_layout("data/ui/main_menu.ui");
    _golf_ui_pixel_pack_square_name(layout, "background");
    _golf_ui_button_name(layout, "play_button");
    _golf_ui_button_name(layout, "courses_button");
    _golf_ui_text_name(layout, "main_text");
    _golf_ui_text_name(layout, "main2_text");
}
