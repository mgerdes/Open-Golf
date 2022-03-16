#include "golf/ui.h"

#include <assert.h>
#include "parson/parson.h"
#include "common/alloc.h"
#include "common/audio.h"
#include "common/common.h"
#include "common/data.h"
#include "common/graphics.h"
#include "common/inputs.h"
#include "common/json.h"
#include "common/log.h"
#include "common/storage.h"
#include "golf/game.h"
#include "golf/golf.h"

static golf_ui_t ui;
static golf_inputs_t *inputs; 
static golf_graphics_t *graphics; 
static golf_game_t *game; 
static golf_config_t *game_cfg; 
static golf_t *golf; 

static float reference_width = 720.0f;

static float _get_ui_scale(void) {
    float ui_scale = 1;
    if (graphics->viewport_size.x > graphics->viewport_size.y) {
        ui_scale = graphics->viewport_size.y / reference_width;
    }
    else {
        ui_scale = graphics->viewport_size.x / reference_width;
    }
    return ui_scale;
}

static golf_ui_draw_entity_t _golf_ui_draw_entity(sg_image image, vec2 pos, vec2 size, float angle, vec2 uv0, vec2 uv1, float is_font, vec4 overlay_color, float alpha) {
    golf_ui_draw_entity_t entity;
    entity.type = GOLF_UI_DRAW_TEXTURE;
    entity.image = image;
    entity.pos = pos;
    entity.size = size;
    entity.angle = angle;
    entity.uv0 = uv0;
    entity.uv1 = uv1;
    entity.is_font = is_font;
    entity.overlay_color = overlay_color;
    entity.alpha = alpha;
    return entity;
}

static golf_ui_draw_entity_t _golf_ui_draw_entity_apply_viewport(vec2 pos, vec2 size) {
    golf_ui_draw_entity_t entity;
    entity.type = GOLF_UI_DRAW_APPLY_VIEWPORT;
    entity.pos = pos;
    entity.size = size;
    return entity;
}

static golf_ui_draw_entity_t _golf_ui_draw_entity_undo_apply_viewport(void) {
    golf_ui_draw_entity_t entity;
    entity.type = GOLF_UI_DRAW_UNDO_APPLY_VIEWPORT;
    return entity;
}

static void _golf_ui_start_fade_out(bool to_main_menu, bool to_loading_level, int level, bool to_retry) {
    ui.fade_out.active = true;
    ui.fade_out.to_main_menu = to_main_menu;
    ui.fade_out.to_loading_level = to_loading_level;
    ui.fade_out.to_retry = to_retry;
    ui.fade_out.level = level;
    ui.fade_out.t = 0;
}

static void _golf_ui_draw_fade(float alpha) {
    golf_texture_t *texture = golf_data_get_texture("data/textures/colors/black.png");
    vec2 pos = vec2_scale(V2(graphics->viewport_size.x, graphics->viewport_size.y), 0.5f);
    vec2 size = graphics->viewport_size;
    float angle = 0;
    vec2 uv0 = V2(0, 0);
    vec2 uv1 = V2(1, 1);
    float is_font = 0;
    vec4 overlay_color = V4(0, 0, 0, 0);
    vec_push(&ui.draw_entities, _golf_ui_draw_entity(texture->sg_image, pos, size, angle, uv0, uv1, is_font, overlay_color, alpha));
}

golf_ui_t *golf_ui_get(void) {
    return &ui;
}

void golf_ui_init(void) {
    memset(&ui, 0, sizeof(ui));
    vec_init(&ui.draw_entities, "ui");
    ui.main_menu.is_level_select_open = false;
    ui.aim_circle.viewport_size_when_set = V2(-1, -1);
    ui.aim_circle.size = -1;
    ui.tutorial.just_saw_tutorial_0 = false;

    golf_data_load("data/ui/ui.ui", false);

    inputs = golf_inputs_get();
    graphics = golf_graphics_get();
    game = golf_game_get();
    game_cfg = golf_data_get_config("data/config/game.cfg");
    golf = golf_get();
}

static bool _golf_ui_layout_get_entity(golf_ui_layout_t *layout, const char *name, golf_ui_layout_entity_t *entity) {
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

static bool _golf_ui_layout_get_entity_of_type(golf_ui_layout_t *layout, const char *name, golf_ui_layout_entity_type type, golf_ui_layout_entity_t **entity) {
    if (!name[0]) {
        return false;
    }

    for (int i = 0; i < layout->entities.length; i++) {
        if (strcmp(layout->entities.data[i].name, name) == 0 && layout->entities.data[i].type == type) {
            *entity = &layout->entities.data[i];
            return true;
        }
    }
    return false;
}

static vec2 _golf_ui_layout_get_entity_pos(golf_ui_layout_t *layout, golf_ui_layout_entity_t entity) {
    float ui_scale = _get_ui_scale();

    golf_ui_layout_entity_t parent_entity;
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
        float sx = graphics->viewport_size.x;
        float sy = graphics->viewport_size.y;

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

    vec_push(&ui.draw_entities, _golf_ui_draw_entity(pixel_pack->texture->sg_image, V2(px, py), V2(sx, sy), 0, uv0, uv1, 0, overlay_color, 1));
}

static void _golf_ui_draw_pixel_pack_square(vec2 pos, vec2 size, float tile_size, vec4 overlay_color, golf_pixel_pack_t *pixel_pack, const char *square_name) {
    golf_pixel_pack_square_t *pixel_pack_square = map_get(&pixel_pack->squares, square_name);
    if (!pixel_pack_square) {
        golf_log_warning("Could not find pixel pack square %s", square_name);
        return;
    }

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

static void _golf_ui_pixel_pack_square(golf_ui_layout_t *layout, golf_ui_layout_entity_t entity) {
    golf_pixel_pack_t *pixel_pack = entity.pixel_pack_square.pixel_pack;

    float ui_scale = _get_ui_scale();
    vec2 pos = _golf_ui_layout_get_entity_pos(layout, entity);
    vec2 size = vec2_scale(entity.size, ui_scale);
    float tile_size = ui_scale * entity.pixel_pack_square.tile_size;
    vec4 overlay_color = entity.pixel_pack_square.overlay_color;

    _golf_ui_draw_pixel_pack_square(pos, size, tile_size, overlay_color, pixel_pack, entity.pixel_pack_square.square_name);
}

static void _golf_ui_pixel_pack_square_name(golf_ui_layout_t *layout, const char *name) {
    golf_ui_layout_entity_t *entity;
    if (!_golf_ui_layout_get_entity_of_type(layout, name, GOLF_UI_PIXEL_PACK_SQUARE, &entity)) {
        golf_log_warning("Could not find pixel pack square entity %s.", name);
        return;
    }

    _golf_ui_pixel_pack_square(layout, *entity);
}

static void _golf_ui_draw_pixel_pack_icon(vec2 pos, vec2 size, vec4 overlay_color, golf_pixel_pack_t *pixel_pack, const char *icon_name) {
    golf_pixel_pack_icon_t *pixel_pack_icon = map_get(&pixel_pack->icons, icon_name);
    vec2 uv0 = pixel_pack_icon->uv0;
    vec2 uv1 = pixel_pack_icon->uv1;
    vec_push(&ui.draw_entities, _golf_ui_draw_entity(pixel_pack->texture->sg_image, pos, size, 0, uv0, uv1, 0, overlay_color, 1));
}

static void _golf_ui_pixel_pack_icon(golf_ui_layout_t *layout, golf_ui_layout_entity_t entity) {
    float ui_scale = _get_ui_scale();
    vec2 pos = _golf_ui_layout_get_entity_pos(layout, entity);
    vec2 size = vec2_scale(entity.size, ui_scale);
    vec4 overlay_color = entity.pixel_pack_icon.overlay_color;
    golf_pixel_pack_t *pixel_pack = entity.pixel_pack_icon.pixel_pack;
    const char *icon_name = entity.pixel_pack_icon.icon_name;

    _golf_ui_draw_pixel_pack_icon(pos, size, overlay_color, pixel_pack, icon_name);
}

static void _golf_ui_pixel_pack_icon_name(golf_ui_layout_t *layout, const char *name) {
    golf_ui_layout_entity_t *entity;
    if (!_golf_ui_layout_get_entity_of_type(layout, name, GOLF_UI_PIXEL_PACK_ICON, &entity)) {
        golf_log_warning("Could not find pixel pack icon entity %s.", name);
        return;
    }

    _golf_ui_pixel_pack_icon(layout, *entity);
}

static void _golf_ui_draw_text(golf_font_t *font, vec2 pos, float font_size, vec4 overlay_color, int horiz_align, int vert_align, const char *text, float alpha) {
    float cur_x = 0;
    float cur_y = 0;
    int sz_idx = 0;
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
    while (text[i]) {
        char c = text[i];
        width += sz_scale * atlas.char_data[(int)c].xadvance;
        i++;
    }

    if (horiz_align == 0) {
        cur_x -= 0.5f * width;
    }
    else if (horiz_align < 0) {
    }
    else if (horiz_align > 0) {
        cur_x -= width;
    }
    else {
        golf_log_warning("Invalid text horizontal_alignment %s", horiz_align);
    }

    if (vert_align == 0) {
        cur_y += 0.5f * (atlas.ascent + atlas.descent);
    }
    else if (vert_align > 0) {
    }
    else if (vert_align < 0) {
        cur_y += (atlas.ascent + atlas.descent);
    }
    else {
        golf_log_warning("Invalid text vert_align %s", vert_align);
    }

    i = 0;
    while (text[i]) {
        char c = text[i];

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

        vec_push(&ui.draw_entities, _golf_ui_draw_entity(atlas.sg_image, V2(px, py), V2(sx, sy), 0,
                    uv0, uv1, 1, overlay_color, alpha));

        cur_x += sz_scale * xadvance;

        i++;
    }
}

static void _golf_ui_text(golf_ui_layout_t *layout, golf_ui_layout_entity_t entity, float alpha) {
    golf_font_t *font = entity.text.font;
    float ui_scale = _get_ui_scale();
    vec2 pos = _golf_ui_layout_get_entity_pos(layout, entity);
    float font_size = ui_scale * entity.text.font_size;

    _golf_ui_draw_text(font, pos, font_size, entity.text.color, entity.text.horiz_align, entity.text.vert_align, entity.text.text.cstr, alpha);
}

static void _golf_ui_text_name(golf_ui_layout_t *layout, const char *name, float alpha) {
    golf_ui_layout_entity_t *entity;
    if (!_golf_ui_layout_get_entity_of_type(layout, name, GOLF_UI_TEXT, &entity)) {
        golf_log_warning("Could not find text entity %s.", name);
        return;
    }
    _golf_ui_text(layout, *entity, alpha);
}

static void _golf_ui_texture_draw(golf_texture_t *texture, vec2 pos, vec2 size, vec4 overlay_color, float alpha) {
    vec_push(&ui.draw_entities, _golf_ui_draw_entity(texture->sg_image, pos, size, 0,
                V2(0, 0), V2(1, 1), 0, overlay_color, alpha));
}

static void _golf_ui_texture(golf_ui_layout_t *layout, golf_ui_layout_entity_t entity) {
    float ui_scale = _get_ui_scale();
    vec2 pos = _golf_ui_layout_get_entity_pos(layout, entity);
    vec2 size = vec2_scale(entity.size, ui_scale);
    vec4 overlay_color = entity.texture.overlay_color;
    golf_texture_t *texture = entity.texture.texture;
    _golf_ui_texture_draw(texture, pos, size, overlay_color, 1);
}

static void _golf_ui_texture_name(golf_ui_layout_t *layout, const char *name) {
    golf_ui_layout_entity_t *entity;
    if (!_golf_ui_layout_get_entity_of_type(layout, name, GOLF_UI_TEXTURE, &entity)) {
        golf_log_warning("Could not find texture entity %s.", name);
        return;
    }
    _golf_ui_texture(layout, *entity);
}

static void _golf_ui_gif_texture_name(golf_ui_layout_t *layout, const char *name, float dt) {
    golf_ui_layout_entity_t *entity;
    if (!_golf_ui_layout_get_entity_of_type(layout, name, GOLF_UI_GIF_TEXTURE, &entity)) {
        golf_log_warning("Could not find gif texture entity %s.", name);
        return;
    }

    float ui_scale = _get_ui_scale();
    vec2 pos = _golf_ui_layout_get_entity_pos(layout, *entity);
    vec2 size = vec2_scale(entity->size, ui_scale);
    golf_gif_texture_t *texture = entity->gif_texture.texture;

    entity->gif_texture.t += dt;
    float a = fmodf(entity->gif_texture.t, entity->gif_texture.total_time) / entity->gif_texture.total_time;
    int frame = (int) (a * texture->num_frames);
    if (frame < 0) frame = 0;
    if (frame >= texture->num_frames) frame = texture->num_frames - 1;

    vec_push(&ui.draw_entities, _golf_ui_draw_entity(texture->sg_images[frame], pos, size, 0,
                V2(0, 0), V2(1, 1), 0, V4(0, 0, 0, 0), 1));
}

static bool _golf_ui_pt_in_box(vec2 pt, vec2 pos, vec2 size) {
    return (pos.x - 0.5f * size.x <= pt.x) && 
        (pos.x + 0.5f * size.x >= pt.x) &&
        (pos.y - 0.5f * size.y <= pt.y) &&
        (pos.y + 0.5f * size.y >= pt.y);
}

static void _golf_ui_button(vec2 pos, vec2 size, bool *down, bool *clicked) {
    if (ui.fade_out.active) {
        *down = false;
        *clicked = false;
        return;
    }

    if (inputs->is_touch) {
        vec2 tp = inputs->touch_pos;
        vec2 tdp = inputs->touch_down_pos;

        bool touch_in_button = _golf_ui_pt_in_box(tp, pos, size);
        bool touch_down_in_button = _golf_ui_pt_in_box(tdp, pos, size);

        *down = inputs->touch_down && touch_in_button;
        *clicked = touch_in_button && touch_down_in_button && inputs->touch_ended;
    }
    else {
        vec2 mp = inputs->mouse_pos;
        vec2 mdp = inputs->mouse_down_pos;

        bool mouse_in_button = _golf_ui_pt_in_box(mp, pos, size);
        bool mouse_down_in_button = _golf_ui_pt_in_box(mdp, pos, size);

        *down = mouse_in_button;
        *clicked = mouse_in_button && mouse_down_in_button && inputs->mouse_clicked[SAPP_MOUSEBUTTON_LEFT];
    }
}

static bool _golf_ui_button_name(golf_ui_layout_t *layout, const char *name) {
    golf_ui_layout_entity_t *entity;
    if (!_golf_ui_layout_get_entity_of_type(layout, name, GOLF_UI_BUTTON, &entity)) {
        golf_log_warning("Could not find button entity %s.", name);
        return false;
    }

    float ui_scale = _get_ui_scale();
    vec2 pos = _golf_ui_layout_get_entity_pos(layout, *entity);
    vec2 size = vec2_scale(entity->size, ui_scale);
    vec_golf_ui_layout_entity_t entities;

    bool down, clicked;
    _golf_ui_button(pos, size, &down, &clicked);

    if (down) {
        entities = entity->button.down_entities;
    }
    else {
        entities = entity->button.up_entities;
    }

    for (int i = 0; i < entities.length; i++) {
        golf_ui_layout_entity_t entity = entities.data[i]; 
        switch (entity.type) {
            case GOLF_UI_BUTTON:
            case GOLF_UI_GIF_TEXTURE:
            case GOLF_UI_AIM_CIRCLE:
            case GOLF_UI_LEVEL_SELECT_SCROLL_BOX:
                break;
            case GOLF_UI_TEXT:
                _golf_ui_text(layout, entity, 1);
                break;
            case GOLF_UI_PIXEL_PACK_SQUARE:
                _golf_ui_pixel_pack_square(layout, entity);
                break;
            case GOLF_UI_TEXTURE:
                _golf_ui_texture(layout, entity);
                break;
            case GOLF_UI_PIXEL_PACK_ICON:
                _golf_ui_pixel_pack_icon(layout, entity);
                break;
        }
    }

    if (clicked) {
        golf_audio_start_sound("button_click", "data/audio/drop_003.ogg", 1, false, true);
    }

    return clicked;
}

static void _golf_ui_aim_circle_name(golf_ui_layout_t *layout, const char *name, bool draw_aimer, float dt, vec2 *aim_delta, bool *start_aiming) {
    golf_ui_layout_entity_t *entity;
    if (!_golf_ui_layout_get_entity_of_type(layout, name, GOLF_UI_AIM_CIRCLE, &entity)) {
        golf_log_warning("Could not find aim circle entity %s.", name);
        return;
    }

    entity->aim_circle.t += dt;

    float ui_scale = _get_ui_scale();
    vec2 pos = golf_graphics_world_to_screen(game->ball.pos);
    ui.aim_circle.pos = pos;

    if (!vec2_equal(graphics->viewport_size, ui.aim_circle.viewport_size_when_set)) {
        ui.aim_circle.viewport_size_when_set = graphics->viewport_size;
        vec3 dir = vec3_normalize(vec3_cross(graphics->cam_dir, graphics->cam_up));
        vec2 pos0 = golf_graphics_world_to_screen(vec3_add(game->ball.pos, vec3_scale(dir, 2 * game->ball.radius)));
        ui.aim_circle.size = vec2_distance(pos0, pos);
    }

    vec2 size = V2(ui.aim_circle.size, ui.aim_circle.size);
    vec2 square_size = vec2_scale(entity->aim_circle.square_size, ui_scale);
    int num_squares = entity->aim_circle.num_squares;
    golf_texture_t *texture = entity->aim_circle.texture;

    float circle_scale = 1;
    if (draw_aimer) {
        vec2 p = inputs->is_touch ? inputs->touch_pos : inputs->mouse_pos;
        float aimer_length = vec2_distance(p, pos);

        golf_texture_t *texture = golf_data_get_texture("data/textures/aimer.png");
        vec2 aimer_pos = vec2_scale(vec2_add(p, pos), 0.5f);
        vec2 aimer_size = V2(2 * ui.aim_circle.size, aimer_length);
        vec2 delta = vec2_normalize(vec2_sub(p, pos));
        float aimer_angle = acosf(vec2_dot(delta, V2(0, 1)));
        if (delta.x > 0) aimer_angle *= -1;

        float vert_scale = 1.777f * (reference_width / graphics->viewport_size.y);
        aimer_length = aimer_length * vert_scale;

        float min_length = golf_config_get_num(game_cfg, "aim_min_length");
        float max_length = golf_config_get_num(game_cfg, "aim_max_length");
        if (aimer_length > max_length) {
            aimer_size.y = max_length / vert_scale; 
            aimer_pos = vec2_add(pos, vec2_scale(delta, 0.5f * aimer_size.y));
        }
        game->aim_line.power = (aimer_length - min_length) / (max_length - min_length);

        vec4 wanted_color = V4(0, 0, 0, 1);
        float power = (aimer_size.y * vert_scale - min_length) / (max_length - min_length);
        if (power < golf_config_get_num(game_cfg, "aim_green_power")) {
            wanted_color = golf_config_get_vec4(game_cfg, "aim_green_color");
        }
        else if (power < golf_config_get_num(game_cfg, "aim_yellow_power")) {
            wanted_color = golf_config_get_vec4(game_cfg, "aim_yellow_color");
        }
        else if (power < golf_config_get_num(game_cfg, "aim_red_power")) {
            wanted_color = golf_config_get_vec4(game_cfg, "aim_red_color");
        }
        else {
            wanted_color = golf_config_get_vec4(game_cfg, "aim_dark_red_color");
        }
        vec4 color = ui.aim_circle.aimer_color;
        color = vec4_add(color, vec4_scale(vec4_sub(wanted_color, color), 0.05f));
        ui.aim_circle.aimer_color = color;
        float alpha = aimer_length / min_length;
        if (alpha > 1) alpha = 1;
        circle_scale = 1 - alpha;

        vec_push(&ui.draw_entities, _golf_ui_draw_entity(texture->sg_image, aimer_pos, aimer_size, aimer_angle, V2(0, 0), V2(1, 1), 0, color, alpha)); 

        if (aim_delta) {
            *aim_delta = delta;
        }

        float min_angle = golf_config_get_num(game_cfg, "aim_rotate_min_angle");
        float max_angle = golf_config_get_num(game_cfg, "aim_rotate_max_angle");
        float rotate_speed = golf_config_get_num(game_cfg, "aim_rotate_speed");
        if (aimer_angle > min_angle) {
            float a = 1.0f - (max_angle - aimer_angle) / (max_angle - min_angle);
            if (a > 1) a = 1;
            game->cam.auto_rotate = false;
            game->cam.angle -= rotate_speed * a * dt;
        }
        if (aimer_angle < -min_angle) {
            float a = 1.0f + (-max_angle - aimer_angle) / (max_angle - min_angle);
            if (a > 1) a = 1;
            game->cam.auto_rotate = false;
            game->cam.angle += rotate_speed * a * dt;
        }
    }

    for (int i = 0; i < num_squares; i++) {
        float a = entity->aim_circle.t / entity->aim_circle.total_time;
        float theta = 2.0f * MF_PI * i / num_squares + 2.0f * MF_PI * a;

        float min_scale = CFG_NUM(game_cfg, "aim_circle_min_scale");
        float s = min_scale + (1 - min_scale) * circle_scale;
        vec2 p = V2(pos.x + s * size.x * cosf(theta), pos.y + s * size.y * sinf(theta));

        float rotation = acosf(sinf(theta));
        if (cosf(theta) > 0) {
            rotation *= -1;
        }

        vec_push(&ui.draw_entities, _golf_ui_draw_entity(texture->sg_image, p, square_size, rotation, 
                    V2(0, 0), V2(1, 1), 0, V4(0, 0, 0, 0), circle_scale));
    }

    if (start_aiming) {
        bool down_in_aim_circle = false;
        if (inputs->is_touch) {
            vec2 tp = inputs->touch_down_pos;
            float tp_dx = pos.x - tp.x;
            float tp_dy = pos.y - tp.y;
            down_in_aim_circle = (tp_dx * tp_dx + tp_dy * tp_dy <= size.x * size.y);
        }
        else {
            vec2 mp = inputs->mouse_down_pos;
            float mp_dx = pos.x - mp.x;
            float mp_dy = pos.y - mp.y;
            down_in_aim_circle = (mp_dx * mp_dx + mp_dy * mp_dy <= size.x * size.y);
        }

        *start_aiming = down_in_aim_circle && (inputs->mouse_down[SAPP_MOUSEBUTTON_LEFT] || inputs->touch_down);
    }
}

static int _golf_ui_level_select_scroll_box_name(golf_ui_layout_t *layout, const char *name, float dt) {
    golf_ui_layout_entity_t *entity;
    if (!_golf_ui_layout_get_entity_of_type(layout, name, GOLF_UI_LEVEL_SELECT_SCROLL_BOX, &entity)) {
        golf_log_warning("Could not find level select scroll box %s.", name);
        return -1;
    }

    float ui_scale = _get_ui_scale();
    vec2 pos = _golf_ui_layout_get_entity_pos(layout, *entity);
    vec2 size = vec2_scale(entity->size, ui_scale);

    float scroll_bar_background_width = entity->level_select_scroll_box.scroll_bar_background_width * ui_scale;
    float scroll_bar_background_padding = entity->level_select_scroll_box.scroll_bar_background_padding * ui_scale;
    float scroll_bar_width = entity->level_select_scroll_box.scroll_bar_width * ui_scale;
    float scroll_bar_height = entity->level_select_scroll_box.scroll_bar_height * ui_scale;

    vec2 bg_pos = V2(pos.x - scroll_bar_background_width - 2 * scroll_bar_background_padding, pos.y);
    vec2 bg_size = V2(size.x - scroll_bar_background_width - 2 * scroll_bar_background_padding, size.y);

    golf_texture_t *texture = golf_data_get_texture("data/textures/colors/black.png");

    vec2 button_size = vec2_scale(entity->level_select_scroll_box.button_size, ui_scale);
    float button_tile_size = entity->level_select_scroll_box.button_tile_size * ui_scale;

    int buttons_per_row = (int) (bg_size.x / button_size.x);
    float button_padding = (bg_size.x - buttons_per_row * button_size.x) / (buttons_per_row + 1);
    float total_height = 4 * button_size.y + 4 * button_padding;

    bool was_scrolling = entity->level_select_scroll_box.is_scrolling;
    if (inputs->is_touch) {
        float down_delta = entity->level_select_scroll_box.down_delta;
        float leeway = entity->level_select_scroll_box.scroll_bar_leeway * ui_scale;

        bool touch_down_in_bg = _golf_ui_pt_in_box(inputs->touch_down_pos, bg_pos, bg_size);
        if (touch_down_in_bg && inputs->touch_down) {
            float scale = 1;
            if (down_delta >= 0) {
                scale = 1 - down_delta / leeway;
                if (scale > 1) scale = 1;
                if (scale < 0.1f) scale = 0.1f;
            }
            if (down_delta <= -total_height) {
                scale = 1 - (-down_delta - total_height) / leeway;
                if (scale > 1) scale = 1;
                if (scale < 0.1f) scale = 0.1f;
            }

            entity->level_select_scroll_box.down_delta_velocity = scale * scale * (inputs->touch_pos.y - inputs->prev_touch_pos.y);

            float movement = inputs->touch_pos.y - inputs->touch_down_pos.y;
            if (movement > 3) {
                entity->level_select_scroll_box.is_scrolling = true;
            }
        }
        else {
            float leeway_fix_speed = entity->level_select_scroll_box.scroll_bar_leeway_fix_speed;
            if (down_delta > 0) {
                down_delta -= leeway_fix_speed * dt;

                if (down_delta < 0) {
                    down_delta = 0;
                }
            }
            if (down_delta < -total_height) {
                down_delta += leeway_fix_speed * dt;

                if (down_delta > -total_height) {
                    down_delta = -total_height;
                }
            }
            entity->level_select_scroll_box.down_delta_velocity *= 0.9f;
            entity->level_select_scroll_box.is_scrolling = false;
        }

        down_delta += entity->level_select_scroll_box.down_delta_velocity;
        if (down_delta >= leeway) {
            down_delta = leeway;
            entity->level_select_scroll_box.down_delta_velocity = 0;
        }
        if (down_delta <= (-total_height - leeway)) {
            down_delta = (-total_height - leeway);
            entity->level_select_scroll_box.down_delta_velocity = 0;
        }

        entity->level_select_scroll_box.down_delta = down_delta;
    }
    else if (entity->level_select_scroll_box.is_scrolling) {
        float sb0 = pos.y - 0.5f * bg_size.y + 0.5f * scroll_bar_height;
        float sb1 = pos.y + 0.5f * bg_size.y - 0.5f * scroll_bar_height;
        float mouse_offset = entity->level_select_scroll_box.start_scrolling_mouse_offset;
        vec2 mouse_pos = inputs->mouse_pos;
        float a = (mouse_pos.y + mouse_offset - sb0) / (sb1 - sb0);
        if (a < 0) a = 0;
        if (a > 1) a = 1;
        entity->level_select_scroll_box.down_delta = -a * total_height;
    }

    float scroll_pct = -entity->level_select_scroll_box.down_delta / total_height;
    if (scroll_pct < 0) scroll_pct = 0;
    if (scroll_pct > 1) scroll_pct = 1;

    {
        vec2 sb_bg_pos = V2(pos.x + 0.5f * bg_size.x + scroll_bar_background_padding + 0.5f * scroll_bar_background_width, pos.y);
        vec2 sb_bg_size = V2(scroll_bar_background_width, bg_size.y);
        vec4 color = entity->level_select_scroll_box.scroll_bar_background_color;

        vec_push(&ui.draw_entities, _golf_ui_draw_entity(texture->sg_image, sb_bg_pos, sb_bg_size, 0, V2(0, 0), V2(1, 1), 0, color, 1));
    }

    {
        float down_delta = entity->level_select_scroll_box.down_delta;
        if (down_delta >= 0) {
            scroll_bar_height -= down_delta;
        }
        if (down_delta <= -total_height) {
            scroll_bar_height -= (-down_delta - total_height);
        }

        float y0 = pos.y - 0.5f * bg_size.y + 0.5f * scroll_bar_height;
        float y1 = pos.y + 0.5f * bg_size.y - 0.5f * scroll_bar_height;

        vec2 sb_pos = V2(pos.x + 0.5f * bg_size.x + scroll_bar_background_padding + 0.5f * scroll_bar_background_width, 
                y0 + (y1 - y0) * scroll_pct);
        vec2 sb_size = V2(scroll_bar_width, scroll_bar_height);

        bool button_down, button_clicked;
        _golf_ui_button(sb_pos, sb_size, &button_down, &button_clicked);

        if (!inputs->is_touch) {
            if (!entity->level_select_scroll_box.is_scrolling) {
                if (button_down && inputs->mouse_down[SAPP_MOUSEBUTTON_LEFT]) {
                    entity->level_select_scroll_box.is_scrolling = true;

                    float mouse_offset = sb_pos.y - inputs->mouse_pos.y;
                    entity->level_select_scroll_box.start_scrolling_mouse_offset = mouse_offset;
                }
            }
            else {
                if (!inputs->mouse_down[SAPP_MOUSEBUTTON_LEFT]) {
                    entity->level_select_scroll_box.is_scrolling = false;
                } 
            }
        }

        vec4 color = entity->level_select_scroll_box.scroll_bar_color;
        if (button_down || entity->level_select_scroll_box.is_scrolling) {
            color = entity->level_select_scroll_box.scroll_bar_down_color;
        }

        vec_push(&ui.draw_entities, _golf_ui_draw_entity(texture->sg_image, sb_pos, sb_size, 0, V2(0, 0), V2(1, 1), 0, color, 1));
    }

    vec_push(&ui.draw_entities, _golf_ui_draw_entity_apply_viewport(bg_pos, bg_size));

    bool down_in_bg, clicked_in_bg;
    _golf_ui_button(bg_pos, bg_size, &down_in_bg, &clicked_in_bg);
    int clicked_button_num = -1;

    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < buttons_per_row; col++) {
            int button_num = row * buttons_per_row + col;
            bool locked = button_num > 0;
            locked = false;

            vec2 button_pos = bg_pos;
            button_pos.x = button_pos.x - 0.5f * bg_size.x + col * button_size.x + 0.5f * button_size.x + (col + 1) * button_padding;
            button_pos.y = button_pos.y - 0.5f * bg_size.y + row * button_size.y + 0.5f * button_size.y + row * button_padding;
            button_pos.y += entity->level_select_scroll_box.down_delta;

            bool button_down, button_clicked;
            _golf_ui_button(button_pos, button_size, &button_down, &button_clicked);
            button_down = !locked && button_down && down_in_bg;
            button_clicked = !locked && button_clicked && clicked_in_bg;

            golf_pixel_pack_t *pixel_pack = entity->level_select_scroll_box.button_pixel_pack;
            const char *square_name;
            if (button_down) {
                square_name = entity->level_select_scroll_box.button_down_square_name;
            }
            else {
                square_name = entity->level_select_scroll_box.button_up_square_name;
            }

            _golf_ui_draw_pixel_pack_square(button_pos, button_size, button_tile_size, V4(0, 0, 0, 0), pixel_pack, square_name);

            golf_font_t *font = entity->level_select_scroll_box.button_font;
            vec2 button_down_text_offset = entity->level_select_scroll_box.button_down_text_offset;

            float best_text_size = entity->level_select_scroll_box.button_best_text_size * ui_scale;
            vec4 best_text_color = entity->level_select_scroll_box.button_best_text_color;
            vec2 best_text_offset = entity->level_select_scroll_box.button_best_text_offset;
            vec2 best_text_pos = vec2_add(button_pos, vec2_scale(best_text_offset, ui_scale));
            if (button_down) {
                best_text_pos = vec2_add(best_text_pos, vec2_scale(button_down_text_offset, ui_scale));
            }

            {
                char storage_key[256];
                snprintf(storage_key, 256, "stroke_count_level_%d", button_num);

                char text_buf[256];
                float stroke_count;
                if (golf_storage_get_num(storage_key, &stroke_count)) {
                    snprintf(text_buf, 256, "Best %d", (int)stroke_count);
                }
                else {
                    snprintf(text_buf, 256, "Best -");
                }
                _golf_ui_draw_text(font, best_text_pos, best_text_size, best_text_color, 0, 0, text_buf, 1);
            }

            float num_text_size = entity->level_select_scroll_box.button_num_text_size * ui_scale;
            vec4 num_text_color = entity->level_select_scroll_box.button_num_text_color;
            vec2 num_text_offset = entity->level_select_scroll_box.button_num_text_offset;
            vec2 num_text_pos = vec2_add(button_pos, vec2_scale(num_text_offset, ui_scale));
            if (button_down) {
                num_text_pos = vec2_add(num_text_pos, vec2_scale(button_down_text_offset, ui_scale));
            }
            char num_text[16];
            snprintf(num_text, 16, "%d", button_num + 1);
            _golf_ui_draw_text(font, num_text_pos, num_text_size, num_text_color, 0, 0, num_text, 1);
            if (button_clicked && !was_scrolling) {
                clicked_button_num = button_num;
            }

            if (locked) {
                {
                    vec4 overlay_color = V4(0, 0, 0, 1);
                    _golf_ui_texture_draw(texture, button_pos, button_size, overlay_color, 0.4f);
                }
                {
                    golf_texture_t *lock_texture = entity->level_select_scroll_box.button_lock_texture;
                    vec4 overlay_color = V4(0, 0, 0, 1);
                    _golf_ui_texture_draw(lock_texture, button_pos, button_size, overlay_color, 0.8f);
                }
            }
        }
    }

    vec_push(&ui.draw_entities, _golf_ui_draw_entity_undo_apply_viewport());
    
    return clicked_button_num;
}

static void _golf_ui_tutorial_name(golf_ui_layout_t *layout, const char *name, int step, float dt) {
    golf_ui_layout_entity_t *entity;
    if (!_golf_ui_layout_get_entity_of_type(layout, name, GOLF_UI_TUTORIAL, &entity)) {
        golf_log_warning("Could not find tutorial %s.", name);
        return;
    }

    golf_texture_t *black_texture = golf_data_get_texture("data/textures/colors/black.png");
    golf_texture_t *pointer_texture = entity->tutorial.pointer_texture;
    golf_font_t *font = entity->tutorial.font;

    float ui_scale = _get_ui_scale();
    vec2 pos = _golf_ui_layout_get_entity_pos(layout, *entity);
    vec2 size = vec2_scale(entity->size, ui_scale);
    float text_size = entity->tutorial.text_size * ui_scale;
    vec2 text_1_pos = vec2_add(pos, vec2_scale(entity->tutorial.text_1_pos, ui_scale));
    vec2 text_2_pos = vec2_add(pos, vec2_scale(entity->tutorial.text_2_pos, ui_scale));
    golf_string_t text_1 = entity->tutorial.text_1;
    golf_string_t text_2 = entity->tutorial.text_2;
    golf_string_t text_3 = entity->tutorial.text_3;
    golf_string_t text_4 = entity->tutorial.text_4;
    golf_string_t text_5 = entity->tutorial.text_5;
    vec4 text_color = entity->tutorial.text_color;
    vec4 bg_color = entity->tutorial.bg_color;

    _golf_ui_texture_draw(black_texture, pos, size, bg_color, bg_color.w);

    if (step == 0) {
        _golf_ui_draw_text(font, text_1_pos, text_size, text_color, 0, 0, text_1.cstr, 1);
        _golf_ui_draw_text(font, text_2_pos, text_size, text_color, 0, 0, text_2.cstr, 1);

        {
            float a = entity->tutorial.pointer_t / entity->tutorial.pointer_1_time;
            vec2 p0 = entity->tutorial.pointer_1_p0;
            vec2 p1 = entity->tutorial.pointer_1_p1;
            vec2 p = vec2_add(p0, vec2_scale(vec2_sub(p1, p0), a));

            vec2 pointer_pos = ui.aim_circle.pos;
            pointer_pos = vec2_add(pointer_pos, vec2_scale(p, ui_scale));
            vec2 pointer_size = vec2_scale(entity->tutorial.pointer_size, ui_scale);
            vec4 pointer_color = entity->tutorial.pointer_color;
            _golf_ui_texture_draw(pointer_texture, pointer_pos, pointer_size, pointer_color, 1);

            entity->tutorial.pointer_t += dt;
            if (entity->tutorial.pointer_t > entity->tutorial.pointer_1_time) {
                entity->tutorial.pointer_t = 0;
            }
        }
    }
    else if (step == 1) {
        _golf_ui_draw_text(font, pos, text_size, text_color, 0, 0, text_3.cstr, 1);
    }
    else if (step == 2) {
        _golf_ui_draw_text(font, text_1_pos, text_size, text_color, 0, 0, text_4.cstr, 1);
        _golf_ui_draw_text(font, text_2_pos, text_size, text_color, 0, 0, text_5.cstr, 1);

        {
            float a = entity->tutorial.pointer_t / entity->tutorial.pointer_2_time;
            vec2 p0 = entity->tutorial.pointer_2_p0;
            vec2 p1 = entity->tutorial.pointer_2_p1;
            vec2 p = vec2_add(p0, vec2_scale(vec2_sub(p1, p0), a));

            vec2 pointer_pos = ui.aim_circle.pos;
            pointer_pos = vec2_add(pointer_pos, vec2_scale(p, ui_scale));
            vec2 pointer_size = vec2_scale(entity->tutorial.pointer_size, ui_scale);
            vec4 pointer_color = entity->tutorial.pointer_color;
            _golf_ui_texture_draw(pointer_texture, pointer_pos, pointer_size, pointer_color, 1);

            entity->tutorial.pointer_t += dt;
            if (entity->tutorial.pointer_t > entity->tutorial.pointer_2_time) {
                entity->tutorial.pointer_t = 0;
            }
        }
    }
}

static void _golf_ui_loading_level(float dt) {
    golf_ui_layout_t *layout = golf_data_get_ui_layout("data/ui/ui.ui");
    _golf_ui_gif_texture_name(layout, "loading_icon", dt);
}

static void _golf_ui_title_screen(float dt) {
    golf_ui_layout_t *layout = golf_data_get_ui_layout("data/ui/ui.ui");
    _golf_ui_gif_texture_name(layout, "loading_icon", dt);
}

static void _golf_ui_main_menu(float dt) {
    golf_ui_layout_t *layout = golf_data_get_ui_layout("data/ui/ui.ui");
    _golf_ui_text_name(layout, "main_text", 1);
    _golf_ui_text_name(layout, "main2_text", 1);

    if (!ui.main_menu.is_level_select_open) {
        if (_golf_ui_button_name(layout, "play_button")) {
            ui.main_menu.is_level_select_open = true;
        }
    }
    else {
        _golf_ui_pixel_pack_square_name(layout, "level_select_background");
        _golf_ui_text_name(layout, "level_select_text", 1);
        if (_golf_ui_button_name(layout, "level_select_exit_button")) {
            ui.main_menu.is_level_select_open = false;
        }
        int clicked_button_num = _golf_ui_level_select_scroll_box_name(layout, "level_select_scroll_box", dt);
        clicked_button_num = clicked_button_num % 12;
        if (clicked_button_num >= 0 && clicked_button_num < 12) {
            golf_audio_start_sound("button_click", "data/audio/drop_003.ogg", 1, false, true);
            _golf_ui_start_fade_out(false, true, clicked_button_num, false);
        }
    }

    float fade_in_length = CFG_NUM(game_cfg, "game_fade_in_length");
    if (golf->main_menu.t < fade_in_length) {
        float alpha = 1 - (golf->main_menu.t / fade_in_length);
        _golf_ui_draw_fade(alpha);
    }
}

static void _golf_ui_camera_controls(float dt) {
    bool is_down;
    if (inputs->is_touch) {
        is_down = inputs->touch_down && !inputs->touch_ended;
    }
    else {
        is_down = inputs->mouse_down[SAPP_MOUSEBUTTON_LEFT];
    }
    if (is_down) {
        vec2 aim_circle_pos = vec2_scale(graphics->window_size, 0.5f);

        vec2 pos0, pos1;
        if (inputs->is_touch) {
            pos0 = inputs->touch_pos;
            pos1 = inputs->prev_touch_pos;
        }
        else
        {
            pos0 = inputs->mouse_pos;
            pos1 = inputs->prev_mouse_pos;
        }

        vec2 delta = vec2_sub(pos0, pos1);

        float angle0 = game->cam.angle;
        float angle1 = angle0;

        if (pos0.x < aim_circle_pos.x) {
            angle1 -= 3.0f * (delta.y / reference_width);
        }
        else {
            angle1 += 3.0f * (delta.y / reference_width);
        }

        if (pos0.y >= aim_circle_pos.y) {
            angle1 -= 3.0f * (delta.x / reference_width);
        }
        else {
            angle1 += 3.0f * (delta.x / reference_width);
        }

        game->cam.auto_rotate = false;
        game->cam.angle = angle1;
        game->cam.angle_velocity = (angle1 - angle0) / dt;
        game->cam.start_angle_velocity = game->cam.angle_velocity;
    }
    else {
        game->cam.angle += game->cam.angle_velocity * dt;
        game->cam.angle_velocity *= 0.9f;
    }

}

static void _golf_ui_in_game_waiting_for_aim(float dt) {
    golf_ui_layout_t *layout = golf_data_get_ui_layout("data/ui/ui.ui");

    bool start_aiming;
    _golf_ui_aim_circle_name(layout, "aim_circle", false, dt, NULL, &start_aiming);
    if (start_aiming) {
        ui.aim_circle.aimer_color = golf_config_get_vec4(game_cfg, "aim_green_color");
        golf_game_start_aiming();
    }
    else {
        _golf_ui_camera_controls(dt);
    }
}

static void _golf_ui_in_game_aiming(float dt) {
    golf_ui_layout_t *layout = golf_data_get_ui_layout("data/ui/ui.ui");
    vec2 aim_delta;
    _golf_ui_aim_circle_name(layout, "aim_circle", true, dt, &aim_delta, NULL);

    game->aim_line.aim_delta = aim_delta;

    bool hit_ball = false;
    if (inputs->is_touch) {
        hit_ball = !inputs->touch_down;
    }
    else {
        hit_ball = inputs->mouse_clicked[SAPP_MOUSEBUTTON_LEFT];
    }
    if (hit_ball) {
        if (game->aim_line.power > 0) {
            golf_game_hit_ball(aim_delta);
        }
        else {
            game->state = GOLF_GAME_STATE_WAITING_FOR_AIM;
        }
    }
}

static void _golf_ui_in_game_watching_ball(float dt) {
    _golf_ui_camera_controls(dt);
}
            
static void _golf_ui_in_game_paused(float dt) {
    GOLF_UNUSED(dt);
    golf_ui_layout_t *layout = golf_data_get_ui_layout("data/ui/ui.ui");

    _golf_ui_pixel_pack_square_name(layout, "pause_menu_background");
    if (_golf_ui_button_name(layout, "pause_menu_resume_button")) {
        golf_game_resume();
    }
    if (_golf_ui_button_name(layout, "pause_menu_retry_button")) {
        _golf_ui_start_fade_out(false, false, 0, true);
    }
    if (_golf_ui_button_name(layout, "pause_menu_exit_button")) {
        _golf_ui_start_fade_out(true, false, 0, false);
    }
    _golf_ui_text_name(layout, "pause_menu_text", 1);
}

static void _golf_ui_in_game_finished(float dt) {
    GOLF_UNUSED(dt);
    golf_ui_layout_t *layout = golf_data_get_ui_layout("data/ui/ui.ui");

    _golf_ui_pixel_pack_square_name(layout, "finished_menu_background");
    if (golf->level_num + 1 < 6) {
        if (_golf_ui_button_name(layout, "finished_menu_next_button")) {
            _golf_ui_start_fade_out(false, true, golf->level_num + 1, false);
        }
    }
    if (_golf_ui_button_name(layout, "finished_menu_retry_button")) {
        _golf_ui_start_fade_out(false, false, 0, true);
    }
    if (_golf_ui_button_name(layout, "finished_menu_exit_button")) {
        _golf_ui_start_fade_out(true, false, 0, false);
    }

    _golf_ui_text_name(layout, "finished_menu_header_text", 1);
    {
        golf_ui_layout_entity_t *entity;
        if (_golf_ui_layout_get_entity_of_type(layout, "finished_menu_sub_text", GOLF_UI_TEXT, &entity)) {
            entity->text.text.len = 0;
            if (game->stroke_count == 1) {
                golf_string_appendf(&entity->text.text, "%d STROKE", game->stroke_count);
            }
            else {
                golf_string_appendf(&entity->text.text, "%d STROKES", game->stroke_count);
            }
            _golf_ui_text(layout, *entity, 1);
        }
    }
}

static void _golf_ui_in_game(float dt) {
    golf_ui_layout_t *layout = golf_data_get_ui_layout("data/ui/ui.ui");
    if (game->state == GOLF_GAME_STATE_WAITING_FOR_AIM || game->state == GOLF_GAME_STATE_WATCHING_BALL) {
        if (_golf_ui_button_name(layout, "pause_button")) {
            golf_game_pause();
        }
    }

    float temp_val;
    if (!golf_storage_get_num("seen_tutorial_0", &temp_val)) {
        if (game->state == GOLF_GAME_STATE_WAITING_FOR_AIM) {
            _golf_ui_tutorial_name(layout, "tutorial", 0, dt);
        }
        else if (game->state == GOLF_GAME_STATE_AIMING) {
            if (game->aim_line.power > 0) {
                _golf_ui_tutorial_name(layout, "tutorial", 1, dt);
            }
            else {
                _golf_ui_tutorial_name(layout, "tutorial", 0, dt);
            }
        }
        else if (game->state == GOLF_GAME_STATE_WATCHING_BALL) {
            ui.tutorial.just_saw_tutorial_0 = true;
            golf_storage_set_num("seen_tutorial_0", 1);
            golf_storage_save();
        }
    }
    else if (!golf_storage_get_num("seen_tutorial_1", &temp_val)) {
        if (game->state == GOLF_GAME_STATE_WAITING_FOR_AIM) {
            ui.tutorial.just_saw_tutorial_0 = false;
            _golf_ui_tutorial_name(layout, "tutorial", 2, dt);
        }
        else if (!ui.tutorial.just_saw_tutorial_0 && game->state == GOLF_GAME_STATE_WATCHING_BALL) {
            golf_storage_set_num("seen_tutorial_1", 1);
            golf_storage_save();
        }
    }

    {
        float blink_time = CFG_NUM(game_cfg, "out_of_bounds_blink_time");
        float time_since_out_of_bounds = game->t - game->ball.time_out_of_bounds;
        if (time_since_out_of_bounds < blink_time) {
            float alpha = 1 - time_since_out_of_bounds / blink_time;
            golf_texture_t *texture = golf_data_get_texture("data/textures/colors/black.png");
            vec2 pos = vec2_scale(V2(graphics->viewport_size.x, graphics->viewport_size.y), 0.5f);
            vec2 size = graphics->viewport_size;
            float angle = 0;
            vec2 uv0 = V2(0, 0);
            vec2 uv1 = V2(1, 1);
            float is_font = 0;
            vec4 overlay_color = V4(1, 1, 1, 1);
            vec_push(&ui.draw_entities, _golf_ui_draw_entity(texture->sg_image, pos, size, angle, uv0, uv1, is_font, overlay_color, alpha));
        }
    }

    switch (game->state) {
        case GOLF_GAME_STATE_MAIN_MENU:
            break;
        case GOLF_GAME_STATE_WAITING_FOR_AIM:
            _golf_ui_in_game_waiting_for_aim(dt);
            break;
        case GOLF_GAME_STATE_AIMING:
            _golf_ui_in_game_aiming(dt);
            break;
        case GOLF_GAME_STATE_WATCHING_BALL:
            _golf_ui_in_game_watching_ball(dt);
            break;
        case GOLF_GAME_STATE_PAUSED:
            _golf_ui_in_game_paused(dt);
            break;
        case GOLF_GAME_STATE_FINISHED:
            _golf_ui_in_game_finished(dt);
            break;
    }

    float fade_in_length = CFG_NUM(game_cfg, "game_fade_in_length");
    float time_to_show_level_num = CFG_NUM(game_cfg, "game_time_to_show_level_num");

    if (golf->in_game.t < time_to_show_level_num + fade_in_length) {
        golf_ui_layout_entity_t *entity;
        if (_golf_ui_layout_get_entity_of_type(layout, "level_num_text", GOLF_UI_TEXT, &entity)) {
            float alpha = 1;
            if (golf->in_game.t > time_to_show_level_num) {
                alpha = 1 - (golf->in_game.t - time_to_show_level_num) / fade_in_length;
            }
            entity->text.text.len = 0;
            golf_string_appendf(&entity->text.text, "LEVEL %d", golf->level_num + 1);
            _golf_ui_text(layout, *entity, alpha);
        }
    }

    if (golf->in_game.t < fade_in_length) {
        float alpha = 1 - (golf->in_game.t / fade_in_length);
        _golf_ui_draw_fade(alpha);
    }
}

void golf_ui_update(float dt) {
    ui.draw_entities.length = 0;

    {
        golf_ui_layout_t *layout = golf_data_get_ui_layout("data/ui/ui.ui");
        golf_ui_layout_entity_t *entity;
        if (_golf_ui_layout_get_entity_of_type(layout, "fps_text", GOLF_UI_TEXT, &entity)) {
            entity->text.text.len = 0;
            golf_string_appendf(&entity->text.text, "%.1f", graphics->framerate);
            _golf_ui_text_name(layout, "fps_text", 1);
        }
    }

    switch (golf->state) {
        case GOLF_STATE_LOADING_LEVEL:
            _golf_ui_loading_level(dt);
            break;
        case GOLF_STATE_TITLE_SCREEN:
            _golf_ui_title_screen(dt);
            break;
        case GOLF_STATE_MAIN_MENU:
            _golf_ui_main_menu(dt);
            break;
        case GOLF_STATE_IN_GAME:
            _golf_ui_in_game(dt);
            break;
    }

    if (ui.fade_out.active) {
        float fade_out_length = CFG_NUM(game_cfg, "game_fade_in_length");
        float alpha = (ui.fade_out.t / fade_out_length);
        _golf_ui_draw_fade(alpha);

        if (ui.fade_out.t >= fade_out_length) {
            ui.fade_out.active = false;

            if (ui.fade_out.to_main_menu) {
                golf_goto_main_menu();
            }
            else if (ui.fade_out.to_loading_level) {
                golf_start_level(ui.fade_out.level);
            }
            else if (ui.fade_out.to_retry) {
                golf_game_start_level();
                golf->in_game.t = 0;
            }
        }

        ui.fade_out.t += dt;
    }
}
