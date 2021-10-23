#include "golf/debug_console.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "3rd_party/cimgui/cimgui.h"
#include "golf/config.h"
#include "golf/data.h"
#include "golf/inputs.h"
#include "golf/maths.h"
#include "golf/renderer.h"

void golf_debug_console_init() {
}

static void _debug_console_main_tab() {
    igText("Frame Rate: %0.3f\n", igGetIO()->Framerate);
    igText("Mouse Pos: <%0.3f, %0.3f>\n", golf_inputs_window_mouse_pos().x, golf_inputs_window_mouse_pos().y);
}

void golf_debug_console_update(float dt) {
    static bool debug_console_open = false;
    if (golf_inputs_button_clicked(SAPP_KEYCODE_GRAVE_ACCENT)) {
        debug_console_open = !debug_console_open;
    }

    if (debug_console_open) {
        igSetNextWindowSize((ImVec2){500, 500}, ImGuiCond_FirstUseEver);
        igSetNextWindowPos((ImVec2){5, 5}, ImGuiCond_FirstUseEver, (ImVec2){0, 0});
        if (igBegin("Debug Console", &debug_console_open, ImGuiWindowFlags_None)) {
            if (igBeginTabBar("##Tabs", ImGuiTabBarFlags_None)) {
                if (igBeginTabItem("Main", NULL, ImGuiTabItemFlags_None)) {
                    _debug_console_main_tab();
                    igEndTabItem();
                }
                if (igBeginTabItem("Data", NULL, ImGuiTabItemFlags_None)) {
                    golf_data_debug_console_tab();
                    igEndTabItem();
                }
                igEndTabBar();
            }
            igEnd();
        }
    }
}
