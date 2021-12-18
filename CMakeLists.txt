cmake_minimum_required(VERSION 3.12)

project(minigolf)
set(CMAKE_BUILD_TYPE Debug)

add_executable(golf
    src/golf_exe/main.c)
target_link_libraries(golf PRIVATE
    golf_lib)
set_property(TARGET golf PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(editor
    src/editor_exe/main.c)
target_link_libraries(editor PRIVATE
    golf_lib)
set_property(TARGET editor PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

set(GOLF_BASE_LIBRARIES 
    cimgui
    cimguizmo
    cute_headers
    dbgtools
    fast_obj
    glad
    glfw
    lightmapper
    mattiasgustavsson_libs
    miniz
    parson
    sokol
    stb
    tinyobj_loader_c
    xatlas)

add_library(golf_lib STATIC
    src/golf/alloc.c
    src/golf/base64.c
    src/golf/config.c
    src/golf/data.c
    src/golf/debug_console.c
    src/golf/editor.c
    src/golf/file.c
    src/golf/game.c
    src/golf/gi.c
    src/golf/inputs.c
	src/golf/json.c
    src/golf/level.c
    src/golf/log.c
    src/golf/map.c
    src/golf/maths.c
    src/golf/renderer.c
    src/golf/string.c
    src/golf/ui.c
    src/golf/vec.c)
if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    target_link_libraries(golf_lib PUBLIC
        ${GOLF_BASE_LIBRARIES})
    target_compile_definitions(golf_lib PUBLIC
        GOLF_PLATFORM_WINDOWS)
    target_compile_options(golf_lib PRIVATE 
        /W3
        /w34061
        /w34062)
elseif(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    target_link_libraries(golf_lib PUBLIC
        ${GOLF_BASE_LIBRARIES}
        "-framework Cocoa"
        "-framework QuartzCore"
        "-framework OpenGL"
        "-framework AudioToolbox")
    target_compile_definitions(golf_lib PUBLIC
        GOLF_PLATFORM_MACOS)
elseif(CMAKE_SYSTEM_NAME STREQUAL Linux)
    target_link_libraries(golf_lib PUBLIC 
        ${GOLF_BASE_LIBRARIES}
        -lX11
        -lXcursor
        -lXi
        -lGL
        -ldl
        -lpthread
        -lm
        -lasound)
    target_compile_definitions(golf_lib PUBLIC
        GOLF_PLATFORM_LINUX)
    target_compile_options(golf_lib PRIVATE 
        -Wall
        -Wextra
        -Wwrite-strings
        -Winit-self
        -Wcast-align
        -Wcast-qual
        -Wpointer-arith
        -Wstrict-aliasing
        -Wformat=2
        -Wmissing-declarations
        -Wmissing-include-dirs
        -Wno-unused-parameter
        -Wuninitialized
        -Wold-style-definition
        -Wstrict-prototypes
        -Wmissing-prototypes)
elseif(CMAKE_SYSTEM_NAME STREQUAL iOS)
    target_link_libraries(golf_lib PUBLIC
        ${GOLF_BASE_LIBRARIES}
        "-framework Foundation"
        "-framework UIKit"
        "-framework OpenGLES"
        "-framework GLKit"
        "-framework AudioToolbox"
        "-framework AVFoundation")
    target_compile_definitions(golf_lib PUBLIC
        GOLF_PLATFORM_IOS)
    set_target_properties(golf_lib PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER me.mgerdes.golf
        MACOSX_BUNDLE_BUNDLE_VERSION 0.0.0
        MACOSX_BUNDLE_SHORT_VERSION_STRING 0.0.0)
    set(CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM RMF5MGUCT2 CACHE INTERNAL "")
endif()

add_library(cimgui STATIC
    src/3rd_party/cimgui/cimgui.cpp)
target_compile_options(cimgui PRIVATE 
    -std=c++11)
target_link_libraries(cimgui PRIVATE 
    imgui)

add_library(cimguizmo STATIC
    src/3rd_party/cimguizmo/cimguizmo.cpp)
target_compile_options(cimguizmo PRIVATE 
    -std=c++11)
target_link_libraries(cimguizmo PRIVATE 
    imgui
    imguizmo)

add_library(cute_headers STATIC
    src/3rd_party/cute_headers/impl.c)

add_library(dbgtools STATIC
    src/3rd_party/dbgtools/callstack.cpp)
target_compile_options(dbgtools PRIVATE 
    -std=c++11)
target_compile_definitions(dbgtools PRIVATE 
    _CRT_SECURE_NO_WARNINGS)

add_library(fast_obj STATIC
    src/3rd_party/fast_obj/fast_obj.c)
target_compile_definitions(fast_obj PRIVATE 
    _CRT_SECURE_NO_WARNINGS)

add_library(glad STATIC
    src/3rd_party/glad/glad.c)

add_library(glfw STATIC
    src/3rd_party/glfw/include/GLFW/glfw3.h
    src/3rd_party/glfw/include/GLFW/glfw3native.h
    src/3rd_party/glfw/src/internal.h 
    src/3rd_party/glfw/src/platform.h 
    src/3rd_party/glfw/src/mappings.h
    src/3rd_party/glfw/src/context.c 
    src/3rd_party/glfw/src/init.c 
    src/3rd_party/glfw/src/input.c 
    src/3rd_party/glfw/src/monitor.c 
    src/3rd_party/glfw/src/platform.c 
    src/3rd_party/glfw/src/vulkan.c 
    src/3rd_party/glfw/src/window.c
    src/3rd_party/glfw/src/egl_context.c 
    src/3rd_party/glfw/src/osmesa_context.c 
    src/3rd_party/glfw/src/null_platform.h 
    src/3rd_party/glfw/src/null_joystick.h
    src/3rd_party/glfw/src/null_init.c 
    src/3rd_party/glfw/src/null_monitor.c 
    src/3rd_party/glfw/src/null_window.c 
    src/3rd_party/glfw/src/null_joystick.c)
include_directories(SYSTEM src/3rd_party/glfw/include)
if (CMAKE_SYSTEM_NAME STREQUAL Windows)
    target_sources(glfw PRIVATE 
        src/3rd_party/glfw/src/win32_time.h
        src/3rd_party/glfw/src/win32_thread.h
        src/3rd_party/glfw/src/win32_module.c
        src/3rd_party/glfw/src/win32_time.c
        src/3rd_party/glfw/src/win32_thread.c
        src/3rd_party/glfw/src/win32_platform.h
        src/3rd_party/glfw/src/win32_joystick.h
        src/3rd_party/glfw/src/win32_init.c
        src/3rd_party/glfw/src/win32_joystick.c
        src/3rd_party/glfw/src/win32_monitor.c
        src/3rd_party/glfw/src/win32_window.c
        src/3rd_party/glfw/src/wgl_context.c)
    target_compile_definitions(glfw PRIVATE 
        _CRT_SECURE_NO_WARNINGS _GLFW_WIN32)
elseif(CMAKE_SYSTEM_NAME STREQUAL Linux)
    target_sources(glfw PRIVATE 
        src/3rd_party/glfw/src/posix_time.h
        src/3rd_party/glfw/src/posix_thread.h 
        src/3rd_party/glfw/src/posix_module.c
        src/3rd_party/glfw/src/posix_time.c 
        src/3rd_party/glfw/src/posix_thread.c
        src/3rd_party/glfw/src/linux_joystick.h 
        src/3rd_party/glfw/src/linux_joystick.c
        src/3rd_party/glfw/src/x11_platform.h
        src/3rd_party/glfw/src/xkb_unicode.h
        src/3rd_party/glfw/src/x11_init.c
        src/3rd_party/glfw/src/x11_monitor.c 
        src/3rd_party/glfw/src/x11_window.c
        src/3rd_party/glfw/src/xkb_unicode.c
        src/3rd_party/glfw/src/glx_context.c)
    target_compile_definitions(glfw PRIVATE _GLFW_X11)
endif()

add_library(imgui STATIC
    src/3rd_party/imgui/imgui.cpp
    src/3rd_party/imgui/imgui_demo.cpp
    src/3rd_party/imgui/imgui_draw.cpp
    src/3rd_party/imgui/imgui_tables.cpp
    src/3rd_party/imgui/imgui_widgets.cpp)
target_compile_options(imgui PRIVATE 
    -std=c++11)

add_library(imguizmo STATIC
    src/3rd_party/imguizmo/ImGuizmo.cpp)
target_compile_options(imguizmo PRIVATE 
    -std=c++11)
target_link_libraries(imguizmo PRIVATE 
    imgui)

add_library(lightmapper STATIC
    src/3rd_party/lightmapper/impl.c)

add_library(mattiasgustavsson_libs STATIC
    src/3rd_party/mattiasgustavsson_libs/impl.c)

add_library(miniz STATIC
    src/3rd_party/miniz/miniz.c)

add_library(parson STATIC
    src/3rd_party/parson/parson.c)

add_library(sokol STATIC
    src/3rd_party/sokol/impl.c)
if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    target_compile_definitions(sokol PUBLIC
        SOKOL_GLCORE33)
elseif(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    target_compile_options(sokol PRIVATE 
        -x objective-c)
    target_compile_definitions(sokol PUBLIC
        SOKOL_GLCORE33)
elseif(CMAKE_SYSTEM_NAME STREQUAL iOS)
    target_compile_options(sokol PRIVATE 
        -x objective-c)
    target_compile_definitions(sokol PUBLIC
        SOKOL_GLES3)
elseif(CMAKE_SYSTEM_NAME STREQUAL Linux)
    target_compile_definitions(sokol PUBLIC
        SOKOL_GLCORE33)
endif()
target_link_libraries(sokol PRIVATE 
    cimgui)

add_library(stb STATIC
    src/3rd_party/stb/impl.c)

add_library(tinyobj_loader_c STATIC
    src/3rd_party/tinyobj_loader_c/impl.c)

add_library(xatlas STATIC
    src/3rd_party/xatlas/xatlas.cpp)
target_compile_options(xatlas PRIVATE 
    -std=c++11)

include_directories(SYSTEM src/3rd_party)
include_directories(src)
