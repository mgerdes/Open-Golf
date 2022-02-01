#define SOKOL_WIN32_NO_GL_LOADER
#define SOKOL_IMPL
#include "sokol/sokol_audio.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_time.h"

#define SOKOL_WIN32_FORCE_MAIN
#include "sokol/sokol_app.h"
#include "sokol/sokol_glue.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"

#define SOKOL_IMGUI_IMPL
#include "sokol/sokol_imgui.h"
