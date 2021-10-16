#include "golf2/ui.h"

#include "golf2/config.h"

static golf_ui_t ui;

static void _pixel_pack_square(const char *ui_pixel_pack, const char *square_name, vec2 pos, vec2 size) {
    golf_ui_pixel_pack_square_t pixel_pack_square;
    pixel_pack_square.ui_pixel_pack = ui_pixel_pack;
    pixel_pack_square.square_name = square_name;
    pixel_pack_square.pos = pos;
    pixel_pack_square.size = size;

    golf_ui_entity_t entity;
    entity.type = GOLF_UI_PIXEL_PACK_SQUARE;
    entity.pixel_pack_square = pixel_pack_square;
    vec_push(&ui.entities, entity);
}

static void _text(const char *font, const char *string, vec2 pos, float size, vec4 color) {
    golf_ui_text_t text;
    text.font = font;
    text.string = string;
    text.pos = pos;
    text.size = size;
    text.color = color;

    golf_ui_entity_t entity;  
    entity.type = GOLF_UI_TEXT;
    entity.text = text;
    vec_push(&ui.entities, entity);
}

golf_ui_t *golf_ui_get(void) {
    return &ui;
}

void golf_ui_init(void) {
    vec_init(&ui.entities);
}

void golf_ui_update(float dt) {
    ui.entities.length = 0;

    golf_config_push_cur_file("ui/main_menu.cfg");
    _pixel_pack_square(CFG_STR("background_pixel_pack"), CFG_STR("background_pixel_pack_square"), CFG_V2("background_pos"), CFG_V2("background_size"));
    _text(CFG_STR("main_text_font"), CFG_STR("main_text_string"), CFG_V2("main_text_pos"),
            CFG_NUM("main_text_size"), CFG_V4("main_text_color"));
    golf_config_pop_cur_file();
}
