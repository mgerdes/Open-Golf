#define _CRT_SECURE_NO_WARNINGS

#include <string.h>
#define  THREAD_IMPLEMENTATION
#include "thread.h"

#define  RND_IMPLEMENTATION
#include "rnd.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#if defined(_WIN32)
#include "glad.h"
#endif

#include "log.h"

#define SOKOL_WIN32_NO_GL_LOADER
#define SOKOL_IMPL
#define SOKOL_LOG m_log
#include "sokol_app.h"
#include "sokol_audio.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_time.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
