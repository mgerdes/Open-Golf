#include "editor_exe/editor.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "3rd_party/cimgui/cimgui.h"
#include "3rd_party/cimguizmo/cimguizmo.h"

#include "golf/data.h"
#include "golf/renderer.h"

void editor_init(void) {
    ImGuiIO *IO = igGetIO();
    IO->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

}

void editor_update(float dt) {
    ImGuiViewport* viewport = igGetMainViewport();
    igSetNextWindowPos(viewport->Pos, ImGuiCond_Always, (ImVec2){0, 0});
    igSetNextWindowSize(viewport->Size, ImGuiCond_Always);
    igSetNextWindowViewport(viewport->ID);
    igSetNextWindowBgAlpha(0);
    igPushStyleVar_Float(ImGuiStyleVar_WindowRounding, 0.0f);
    igPushStyleVar_Vec2(ImGuiStyleVar_WindowPadding, (ImVec2){0, 0});
    igBegin("Dockspace", 0, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);
    igPopStyleVar(2);

    if (igBeginMenuBar())
    {
        if (igBeginMenu("File", true))
        {
            if (igMenuItem_Bool("Open", NULL, false, true)) {
            }
            if (igMenuItem_Bool("Save", NULL, false, true)) {
            }
            if (igMenuItem_Bool("Close", NULL, false, true)) {
            }
            igEndMenu();
        }
        if (igBeginMenu("Edit", true))
        {
            if (igMenuItem_Bool("Undo", NULL, false, true)) {
            }
            if (igMenuItem_Bool("Redo", NULL, false, true)) {
            }
            igEndMenu();
        }
        igEndMenuBar();
    }

    ImGuiID dock_main_id = igGetID_Str("dockspace");
    igDockSpace(dock_main_id, (ImVec2){0, 0}, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoTabBar, NULL);

    static bool built_dockspace = false;
    if (!built_dockspace) {
        built_dockspace = true;

        igDockBuilderRemoveNode(dock_main_id); 
        igDockBuilderAddNode(dock_main_id, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_DockSpace);
        igDockBuilderSetNodeSize(dock_main_id, viewport->Size);

        ImGuiID dock_id_right = igDockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.30f, NULL, &dock_main_id);
        ImGuiID dock_id_top = igDockBuilderSplitNode(dock_main_id, ImGuiDir_Up, 0.05f, NULL, &dock_main_id);

        igDockBuilderDockWindow("Viewport", dock_main_id);
        igDockBuilderDockWindow("Top", dock_id_top);
        igDockBuilderDockWindow("Right", dock_id_right);
        igDockBuilderFinish(dock_main_id);
    }

    ImGuiDockNode *central_node = igDockBuilderGetCentralNode(dock_main_id);
    golf_renderer_t *renderer = golf_renderer_get();
    renderer->viewport_pos = V2(central_node->Pos.x, central_node->Pos.y);
    renderer->viewport_size = V2(central_node->Size.x, central_node->Size.y);

    {
        igBegin("Top", NULL, ImGuiWindowFlags_NoTitleBar);
        {
            golf_data_texture_t *tex = golf_data_get_texture("data/textures/translate.png");
            igImageButton((ImTextureID)(intptr_t)tex->sg_image.id, (ImVec2){ 15, 15 }, 
                    (ImVec2){ 0, 0 }, (ImVec2) { 1, 1 } , 2,
                    (ImVec4){ 0, 0, 0, 0} , (ImVec4){ 0, 0, 0, 1});
            if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                igBeginTooltip();
                igText("Translate Mode (Ctrl+W)");
                igEndTooltip();
            }
        }
        igSameLine(0, 5);
        {
            golf_data_texture_t *tex = golf_data_get_texture("data/textures/rotate.png");
            igImageButton((ImTextureID)(intptr_t)tex->sg_image.id, (ImVec2){ 15, 15 }, 
                    (ImVec2){ 0, 0 }, (ImVec2) { 1, 1 } , 2,
                    (ImVec4){ 0, 0, 0, 0} , (ImVec4){ 0, 0, 0, 1});
            if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                igBeginTooltip();
                igText("Rotate Mode (Ctrl+E}");
                igEndTooltip();
            }
        }
        igSameLine(0, 5);
        {
            golf_data_texture_t *tex = golf_data_get_texture("data/textures/scale.png");
            igImageButton((ImTextureID)(intptr_t)tex->sg_image.id, (ImVec2){ 15, 15 }, 
                    (ImVec2){ 0, 0 }, (ImVec2) { 1, 1 } , 2,
                    (ImVec4){ 1, 1, 1, 0.0} , (ImVec4){ 0, 0, 0, 1});
            if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                igBeginTooltip();
                igText("Scale Mode (Ctrl+R)");
                igEndTooltip();
            }
        }
        igEnd();
    }

    igBegin("Right", NULL, ImGuiWindowFlags_NoTitleBar);
    igEnd();

    igEnd();
}
