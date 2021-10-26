#include "editor_exe/editor.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "3rd_party/cimgui/cimgui.h"

void editor_init(void) {
    ImGuiIO *IO = igGetIO();
    IO->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

}

void editor_update(float dt) {
    ImGuiViewport* viewport = igGetMainViewport();
    igSetNextWindowPos(viewport->Pos, ImGuiCond_Always, (ImVec2){0, 0});
    igSetNextWindowSize(viewport->Size, ImGuiCond_Always);
    igSetNextWindowViewport(viewport->ID);
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
    //printf("%f, %f\n", central_node->Size.x, central_node->Size.y);

    igBegin("Top", NULL, ImGuiWindowFlags_NoTitleBar);
    igEnd();

    igBegin("Right", NULL, ImGuiWindowFlags_NoTitleBar);
    igEnd();

    igEnd();
}
