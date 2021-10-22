#include "golf/ui.h"

#include <assert.h>
#include "3rd_party/parson/parson.h"
#include "golf/config.h"
#include "golf/inputs.h"

static golf_ui_t ui;

golf_ui_t *golf_ui_get(void) {
    return &ui;
}

void golf_ui_init(void) {
    memset(&ui, 0, sizeof(ui));
    ui.state = GOLF_UI_MAIN_MENU;
}

void golf_ui_update(float dt) {
    if (ui.state == GOLF_UI_MAIN_MENU) {
    }
}
