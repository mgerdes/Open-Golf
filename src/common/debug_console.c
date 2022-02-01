#include "common/debug_console.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include "common/data.h"
#include "common/graphics.h"
#include "common/inputs.h"
#include "common/maths.h"

static golf_inputs_t *inputs; 

void golf_debug_console_init(void) {
    inputs = golf_inputs_get();
}

static void _debug_console_main_tab(void) {
    igText("Frame Rate: %0.3f\n", igGetIO()->Framerate);
    igText("Mouse Pos: <%0.3f, %0.3f>\n", inputs->mouse_pos.x, inputs->mouse_pos.y);
}

void golf_debug_console_update(float dt) {
    static bool debug_console_open = false;
    if (inputs->button_clicked[SAPP_KEYCODE_GRAVE_ACCENT]) {
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
                if (igBeginTabItem("Graphics", NULL, ImGuiTabItemFlags_None)) {
                    golf_graphics_debug_console_tab();
                    igEndTabItem();
                }
                igEndTabBar();
            }
            igEnd();
        }
    }
}
