#include "golf2/debug_console.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "3rd_party/cimgui/cimgui.h"
#include "mcore/maths.h"
#include "golf2/inputs.h"

void golf_debug_console_init() {
}

void golf_debug_console_update(float dt) {
    static bool debug_console_open = false;
    if (golf_inputs_button_clicked(SAPP_KEYCODE_GRAVE_ACCENT)) {
        debug_console_open = !debug_console_open;
    }

    if (debug_console_open) {
        igSetNextWindowSize((ImVec2){500, 500}, ImGuiCond_FirstUseEver);
        igSetNextWindowPos((ImVec2){5, 5}, ImGuiCond_FirstUseEver, (ImVec2){0, 0});
        igBegin("Debug Console", &debug_console_open, ImGuiWindowFlags_None);
        igText("Frame Rate: %0.3f\n", igGetIO()->Framerate);
        igText("Mouse Pos: <%0.3f, %0.3f>\n", golf_inputs_window_mouse_pos().x, golf_inputs_window_mouse_pos().y);
        igEnd();
    }
}
