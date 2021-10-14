#include "golf2/debug_console.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "3rd_party/cimgui/cimgui.h"
#include "mcore/maths.h"
#include "golf2/inputs.h"
#include "golf2/renderer.h"

void golf_debug_console_init() {
}

static void _debug_console_main_tab() {
    igText("Frame Rate: %0.3f\n", igGetIO()->Framerate);
    igText("Mouse Pos: <%0.3f, %0.3f>\n", golf_inputs_window_mouse_pos().x, golf_inputs_window_mouse_pos().y);
}

static void _debug_console_renderer_tab() {
    golf_renderer_t *renderer = golf_renderer_get();
    if (igCollapsingHeaderTreeNodeFlags("Fonts", ImGuiTreeNodeFlags_None)) {
        const char *key;
        map_iter_t iter = map_iter(&renderer->fonts_map);

        while ((key = map_next(&renderer->fonts_map, &iter))) {
            golf_renderer_font_t *font = map_get(&renderer->fonts_map, key);
            mdata_font_t *font_data = font->font_data;

            if (igCollapsingHeaderTreeNodeFlags(key, ImGuiTreeNodeFlags_None)) {
                for (int i = 0; i < 3; i++) {
                    igText("Image Size: %d", font_data->atlases[i].bmp_size);
                    igImage((ImTextureID)(intptr_t)font->atlas_images[i].id, (ImVec2){font_data->atlases[i].bmp_size, font_data->atlases[i].bmp_size}, (ImVec2){0, 0}, (ImVec2){1, 1}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 1});
                }
            }
        }
    }
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
                if (igBeginTabItem("Renderer", NULL, ImGuiTabItemFlags_None)) {
                    _debug_console_renderer_tab();
                    igEndTabItem();
                }
                igEndTabBar();
            }
            igEnd();
        }
    }
}
