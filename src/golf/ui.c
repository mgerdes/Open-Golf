#include "golf/ui.h"

#include <assert.h>
#include "3rd_party/parson/parson.h"
#include "golf/config.h"
#include "golf/data.h"
#include "golf/inputs.h"
#include "golf/log.h"

static golf_ui_t ui;

static void _golf_ui_pixel_pack_square(const char *pixel_pack_name, const char *square_name, vec2 pos, vec2 size) {
    golf_ui_pixel_pack_square_t pixel_pack_square;
    pixel_pack_square.pixel_pack = golf_data_get_pixel_pack(pixel_pack_name);
    pixel_pack_square.square = map_get(&pixel_pack_square.pixel_pack->squares, square_name);
    pixel_pack_square.pos = pos;
    pixel_pack_square.size = size;

    golf_ui_entity_t entity;
    entity.type = GOLF_UI_PIXEL_PACK_SQUARE;
    entity.pixel_pack_square = pixel_pack_square;
    vec_push(&ui.entities, entity);
}

golf_ui_t *golf_ui_get(void) {
    return &ui;
}

void golf_ui_init(void) {
    vec_init(&ui.entities);
    memset(&ui, 0, sizeof(ui));
    ui.state = GOLF_UI_MAIN_MENU;
}

void golf_ui_update(float dt) {
    ui.entities.length = 0;
    _golf_ui_pixel_pack_square("data/textures/pixel_pack.pixel_pack", "blue_background", V2(500, 500), V2(200, 200));

    if (ui.state == GOLF_UI_MAIN_MENU) {
    }
}
