#include "golf/ui.h"

#include <assert.h>
#include "parson/parson.h"
#include "common/alloc.h"
#include "common/data.h"
#include "common/graphics.h"
#include "common/inputs.h"
#include "common/json.h"
#include "common/log.h"
#include "golf/game.h"
#include "golf/golf.h"

static golf_ui_t ui;
static golf_inputs_t *inputs; 
static golf_graphics_t *graphics; 
static golf_game_t *game; 
static golf_t *golf; 

static golf_ui_draw_entity_t _golf_ui_draw_entity(sg_image image, vec2 pos, vec2 size, float angle, vec2 uv0, vec2 uv1, float is_font, vec4 overlay_color) {
    golf_ui_draw_entity_t entity;
    entity.image = image;
    entity.pos = pos;
    entity.size = size;
    entity.angle = angle;
    entity.uv0 = uv0;
    entity.uv1 = uv1;
    entity.is_font = is_font;
    entity.overlay_color = overlay_color;
    return entity;
}

golf_ui_t *golf_ui_get(void) {
    return &ui;
}

void golf_ui_init(void) {
    memset(&ui, 0, sizeof(ui));
    vec_init(&ui.draw_entities, "ui");
    ui.state = GOLF_UI_MAIN_MENU;
    ui.aim_circle.viewport_size_when_set = V2(-1, -1);
    ui.aim_circle.size = -1;

    inputs = golf_inputs_get();
    graphics = golf_graphics_get();
    game = golf_game_get();
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
    float ui_scale = graphics->viewport_size.x / 720.0f;

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

    vec_push(&ui.draw_entities, _golf_ui_draw_entity(pixel_pack->texture->sg_image, V2(px, py), V2(sx, sy), 0,
                uv0, uv1, 0, overlay_color));
}

static void _golf_ui_pixel_pack_square(golf_ui_layout_t *layout, golf_ui_layout_entity_t entity) {
    golf_pixel_pack_t *pixel_pack = entity.pixel_pack_square.pixel_pack;
    golf_pixel_pack_square_t *pixel_pack_square = map_get(&pixel_pack->squares, entity.pixel_pack_square.square_name);
    if (!pixel_pack_square) {
        golf_log_warning("Could not find pixel pack square %s", entity.pixel_pack_square.square_name);
        return;
    }

    vec2 vp_size = graphics->viewport_size;
    float ui_scale = vp_size.x / 720.0f;
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
    golf_ui_layout_entity_t *entity;
    if (!_golf_ui_layout_get_entity_of_type(layout, name, GOLF_UI_PIXEL_PACK_SQUARE, &entity)) {
        golf_log_warning("Could not find pixel pack square entity %s.", name);
        return;
    }

    _golf_ui_pixel_pack_square(layout, *entity);
}

static void _golf_ui_text(golf_ui_layout_t *layout, golf_ui_layout_entity_t entity) {
    golf_font_t *font = entity.text.font;
    vec2 vp_size = graphics->viewport_size;
    float ui_scale = vp_size.x / 720.0f;
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

        vec_push(&ui.draw_entities, _golf_ui_draw_entity(atlas.sg_image, V2(px, py), V2(sx, sy), 0,
                    uv0, uv1, 1, overlay_color));

        cur_x += sz_scale * xadvance;

        i++;
    }
}

static void _golf_ui_text_name(golf_ui_layout_t *layout, const char *name) {
    golf_ui_layout_entity_t *entity;
    if (!_golf_ui_layout_get_entity_of_type(layout, name, GOLF_UI_TEXT, &entity)) {
        golf_log_warning("Could not find text entity %s.", name);
        return;
    }
    _golf_ui_text(layout, *entity);
}

static bool _golf_ui_button_name(golf_ui_layout_t *layout, const char *name) {
    golf_ui_layout_entity_t *entity;
    if (!_golf_ui_layout_get_entity_of_type(layout, name, GOLF_UI_BUTTON, &entity)) {
        golf_log_warning("Could not find button entity %s.", name);
        return false;
    }

    float ui_scale = graphics->viewport_size.x / 720.0f;
    vec2 pos = _golf_ui_layout_get_entity_pos(layout, *entity);
    vec2 size = vec2_scale(entity->size, ui_scale);
    vec2 mp = inputs->mouse_pos;
    vec2 tp = inputs->touch_pos;
    vec_golf_ui_layout_entity_t entities;

    bool button_down = (pos.x - 0.5f * size.x <= mp.x && pos.x + 0.5f * size.x >= mp.x &&
            pos.y - 0.5f * size.y <= mp.y && pos.y + 0.5f * size.y >= mp.y) ||
        ((pos.x - 0.5f * size.x <= tp.x && pos.x + 0.5f * size.x >= tp.x &&
         pos.y - 0.5f * size.y <= tp.y && pos.y + 0.5f * size.y >= tp.y) && inputs->touch_down);

    if (button_down) {
        entities = entity->button.down_entities;
    }
    else {
        entities = entity->button.up_entities;
    }

    for (int i = 0; i < entities.length; i++) {
        golf_ui_layout_entity_t entity = entities.data[i]; 
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

    return button_down && (inputs->mouse_clicked[SAPP_MOUSEBUTTON_LEFT] || inputs->touch_ended);
}

static void _golf_ui_gif_texture_name(golf_ui_layout_t *layout, const char *name, float dt) {
    golf_ui_layout_entity_t *entity;
    if (!_golf_ui_layout_get_entity_of_type(layout, name, GOLF_UI_GIF_TEXTURE, &entity)) {
        golf_log_warning("Could not find gif texture entity %s.", name);
        return;
    }

    float ui_scale = graphics->viewport_size.x / 720.0f;
    vec2 pos = _golf_ui_layout_get_entity_pos(layout, *entity);
    vec2 size = vec2_scale(entity->size, ui_scale);

    golf_gif_texture_t *texture = entity->gif_texture.texture;

    entity->gif_texture.t += dt;
    float a = fmodf(entity->gif_texture.t, entity->gif_texture.total_time) / entity->gif_texture.total_time;
    int frame = (int) (a * texture->num_frames);
    if (frame < 0) frame = 0;
    if (frame >= texture->num_frames) frame = texture->num_frames - 1;

    vec_push(&ui.draw_entities, _golf_ui_draw_entity(texture->sg_images[frame], pos, size, 0,
                V2(0, 0), V2(1, 1), 0, V4(0, 0, 0, 0)));
}

static bool _golf_ui_aim_circle_name(golf_ui_layout_t *layout, const char *name, bool draw_aimer, float dt, vec2 *aim_delta) {
    golf_ui_layout_entity_t *entity;
    if (!_golf_ui_layout_get_entity_of_type(layout, name, GOLF_UI_AIM_CIRCLE, &entity)) {
        golf_log_warning("Could not find aim circle entity %s.", name);
        return false;
    }


    float ui_scale = graphics->viewport_size.x / 720.0f;
    vec2 pos = golf_graphics_world_to_screen(game->ball.pos);

    if (!vec2_equal(graphics->viewport_size, ui.aim_circle.viewport_size_when_set)) {
        ui.aim_circle.viewport_size_when_set = graphics->viewport_size;
        vec3 dir = vec3_normalize(vec3_cross(graphics->cam_dir, graphics->cam_up));
        vec2 pos0 = golf_graphics_world_to_screen(vec3_add(game->ball.pos, vec3_scale(dir, 4 * game->ball.radius)));
        ui.aim_circle.size = vec2_distance(pos0, pos);
    }

    vec2 size = V2(ui.aim_circle.size, ui.aim_circle.size);
    vec2 square_size = entity->aim_circle.square_size;
    int num_squares = entity->aim_circle.num_squares;
    golf_texture_t *texture = entity->aim_circle.texture;

    for (int i = 0; i < num_squares; i++) {
        float theta = 2.0f * MF_PI * i / num_squares;
        vec2 p = V2(pos.x + size.x * cosf(theta), pos.y + size.y * sinf(theta));
        vec_push(&ui.draw_entities, _golf_ui_draw_entity(texture->sg_image, p, square_size, 0, 
                    V2(0, 0), V2(1, 1), 0, V4(0, 0, 0, 0)));
    }



    bool in_aim_circle = false;
    if (inputs->is_touch) {
        vec2 tp = inputs->touch_pos;
        float tp_dx = pos.x - tp.x;
        float tp_dy = pos.y - tp.y;
        in_aim_circle = (tp_dx * tp_dx + tp_dy * tp_dy <= size.x * size.y);
    }
    else {
        vec2 mp = inputs->mouse_pos;
        float mp_dx = pos.x - mp.x;
        float mp_dy = pos.y - mp.y;
        in_aim_circle = (mp_dx * mp_dx + mp_dy * mp_dy <= size.x * size.y);
    }

    if (draw_aimer) {
        vec2 p = inputs->is_touch ? inputs->touch_pos : inputs->mouse_pos;

        golf_texture_t *white = golf_data_get_texture("data/textures/colors/white.png");
        vec2 aimer_pos = vec2_scale(vec2_add(p, pos), 0.5f);
        vec2 aimer_size = V2(vec2_distance(p, pos), 10);
        vec2 delta = vec2_normalize(vec2_sub(p, pos));
        float aimer_angle = acosf(vec2_dot(delta, V2(0, 1)));
        if (delta.x > 0) aimer_angle *= -1;
        aimer_angle += 0.5f * MF_PI;
        vec_push(&ui.draw_entities, _golf_ui_draw_entity(white->sg_image, aimer_pos, aimer_size, aimer_angle, V2(0, 0), V2(1, 1), 0, V4(0, 0, 0, 0))); 

        if (aim_delta) {
            *aim_delta = delta;
        }
    }

    return in_aim_circle && (inputs->mouse_down[SAPP_MOUSEBUTTON_LEFT] || inputs->touch_down);
}

static void _golf_ui_title_screen(float dt) {
    golf_ui_layout_t *layout = golf_data_get_ui_layout("data/ui/main_menu.ui");
    _golf_ui_text_name(layout, "main_text");
    _golf_ui_text_name(layout, "main2_text");
    _golf_ui_gif_texture_name(layout, "loading_icon", dt);
}

static void _golf_ui_main_menu(float dt) {
    golf_ui_layout_t *layout = golf_data_get_ui_layout("data/ui/main_menu.ui");
    _golf_ui_text_name(layout, "main_text");
    _golf_ui_text_name(layout, "main2_text");
    if (_golf_ui_button_name(layout, "play_button")) {
        golf_start_level();
    }
    if (_golf_ui_button_name(layout, "courses_button")) {
    }
}

static void _golf_ui_in_game_waiting_for_aim(float dt) {
    golf_ui_layout_t *layout = golf_data_get_ui_layout("data/ui/main_menu.ui");

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
            angle1 -= 1.5f * (delta.y / graphics->window_size.x);
        }
        else {
            angle1 += 1.5f * (delta.y / graphics->window_size.x);
        }

        if (pos0.y >= aim_circle_pos.y) {
            angle1 -= 1.5f * (delta.x / graphics->window_size.x);
        }
        else {
            angle1 += 1.5f * (delta.x / graphics->window_size.x);
        }

        game->cam.angle = angle1;
        game->cam.angle_velocity = (angle1 - angle0) / dt;
        game->cam.start_angle_velocity = game->cam.angle_velocity;
    }
    else {
        game->cam.angle += game->cam.angle_velocity * dt;
        game->cam.angle_velocity *= 0.9f;
    }

    //vec3 cam_delta = vec3_rotate_y(V3(2.6f, 1.5f, 0), game->cam.angle);
    //graphics->cam_pos = vec3_add(game->ball.pos, cam_delta);
    //graphics->cam_dir = vec3_normalize(vec3_sub(game->ball.pos, graphics->cam_pos));

    if (_golf_ui_aim_circle_name(layout, "aim_circle", false, dt, NULL)) {
        golf_game_start_aiming();
    }
}

static void _golf_ui_in_game_aiming(float dt) {
    //if (!inputs->mouse_down[SAPP_MOUSEBUTTON_LEFT]) {
    //golf_game_stop_aiming();
    //}

    golf_ui_layout_t *layout = golf_data_get_ui_layout("data/ui/main_menu.ui");
    vec2 aim_delta;
    _golf_ui_aim_circle_name(layout, "aim_circle", true, dt, &aim_delta);

    bool hit_ball = false;
    if (inputs->is_touch) {
        hit_ball = !inputs->touch_down;
    }
    else {
        hit_ball = inputs->mouse_clicked[SAPP_MOUSEBUTTON_LEFT];
    }
    if (hit_ball) {
        golf_game_hit_ball(aim_delta);
    }
}

static void _golf_ui_in_game(float dt) {
    switch (game->state) {
        case GOLF_GAME_STATE_MAIN_MENU:
            break;
        case GOLF_GAME_STATE_WAITING_FOR_AIM:
            _golf_ui_in_game_waiting_for_aim(dt);
            break;
        case GOLF_GAME_STATE_AIMING:
            _golf_ui_in_game_aiming(dt);
            break;
    }
}

void golf_ui_update(float dt) {
    ui.draw_entities.length = 0;

    {
        golf_ui_layout_t *layout = golf_data_get_ui_layout("data/ui/main_menu.ui");
        golf_ui_layout_entity_t *entity;
        if (_golf_ui_layout_get_entity_of_type(layout, "fps_text", GOLF_UI_TEXT, &entity)) {
            entity->text.text.len = 0;
            golf_string_appendf(&entity->text.text, "%.1f", graphics->framerate);
            _golf_ui_text_name(layout, "fps_text");
        }
    }

    switch (golf->state) {
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
}
