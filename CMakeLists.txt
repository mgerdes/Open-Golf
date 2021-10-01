cmake_minimum_required(VERSION 3.12)

project(minigolf)

add_executable(minigolf
	src/golf/assets.c 
	src/golf/audio.c 
	src/golf/config.c 
	src/golf/controls.c 
	src/golf/data_stream.c 
	src/golf/game.c 
	src/golf/game_editor.c 
	src/golf/hole.c 
	src/golf/lightmaps.c 
	src/golf/log.c 
	src/golf/main.c 
	src/golf/minigolf.c 
	src/golf/obj_converter.c 
	src/golf/profiler.c 
	src/golf/renderer.c 
	src/golf/shaders.c 
	src/golf/single_file_libs.c 
	src/golf/ui.c
    src/membedder/membedded_files.h)
set(GOLF_BASE_LIBRARIES 
    mcore
    cimgui
    cute_headers
    dbgtools
    glad
    imgui
    lightmapper
    map
    mattiasgustavsson_libs
    miniz
    #s7
    sokol
    stb
    tinyobj_loader_c
    vec
    xatlas)
target_compile_definitions(minigolf PRIVATE
    MEMBED_FILES)
if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    target_link_libraries(minigolf PRIVATE
        ${GOLF_BASE_LIBRARIES})
elseif(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    target_link_libraries(minigolf PRIVATE
        ${GOLF_BASE_LIBRARIES}
        "-framework Cocoa"
        "-framework QuartzCore"
        "-framework OpenGL"
        "-framework AudioToolbox")
elseif(CMAKE_SYSTEM_NAME STREQUAL Linux)
    target_link_libraries(minigolf PRIVATE 
        ${GOLF_BASE_LIBRARIES}
        -lX11
        -lXcursor
        -lXi
        -lGL
        -ldl
        -lpthread
        -lm
        -lasound)
elseif(CMAKE_SYSTEM_NAME STREQUAL iOS)
    target_link_libraries(minigolf PRIVATE
        ${GOLF_BASE_LIBRARIES}
        "-framework Foundation"
        "-framework UIKit"
        "-framework OpenGLES"
        "-framework GLKit"
        "-framework AudioToolbox"
        "-framework AVFoundation")
    set_target_properties(minigolf PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER me.mgerdes.golf
        MACOSX_BUNDLE_BUNDLE_VERSION 0.0.0
        MACOSX_BUNDLE_SHORT_VERSION_STRING 0.0.0)
    set(CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM RMF5MGUCT2 CACHE INTERNAL "")
endif()

if (CMAKE_SYSTEM_NAME STREQUAL Windows OR CMAKE_SYSTEM_NAME STREQUAL Darwin OR CMAKE_SYSTEM_NAME STREQUAL Linux)
    add_executable(membedder
        src/membedder/main.c)
    target_link_libraries(membedder PRIVATE 
        mcore
        cute_headers
        dbgtools
        vec
        map)
    file(GLOB_RECURSE DATA_FILES CONFIGURE_DEPENDS "${CMAKE_SRC_DIR}/data/*")
    add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/src/membedder/membedded_files.h
        COMMAND membedder 
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS ${DATA_FILES})
endif()

add_library(mcore STATIC
    src/mcore/maths.c
    src/mcore/mcommon.c
    src/mcore/mfile.c
    src/mcore/mimport.c
    src/mcore/mlog.c
    src/mcore/mscript.c
    src/mcore/mstring.c)

add_library(cimgui STATIC
    src/3rd_party/cimgui/cimgui.cpp)
target_compile_options(cimgui PRIVATE 
    -std=c++11)
target_link_libraries(cimgui PRIVATE 
    imgui)

add_library(cute_headers STATIC
    src/3rd_party/cute_headers/impl.c)

add_library(dbgtools STATIC
    src/3rd_party/dbgtools/callstack.cpp)
target_compile_options(dbgtools PRIVATE 
    -std=c++11)

add_library(glad STATIC
    src/3rd_party/glad/glad.c)

add_library(imgui STATIC
    src/3rd_party/imgui/imgui.cpp
    src/3rd_party/imgui/imgui_demo.cpp
    src/3rd_party/imgui/imgui_draw.cpp
    src/3rd_party/imgui/imgui_widgets.cpp)
target_compile_options(imgui PRIVATE 
    -std=c++11)

add_library(lightmapper STATIC
    src/3rd_party/lightmapper/impl.c)

add_library(map STATIC
    src/3rd_party/map/map.c)

add_library(mattiasgustavsson_libs STATIC
    src/3rd_party/mattiasgustavsson_libs/impl.c)

add_library(miniz STATIC
    src/3rd_party/miniz/miniz.c)

#add_library(s7 STATIC
#src/3rd_party/s7/s7.c)

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

add_library(vec STATIC
    src/3rd_party/vec/vec.c)

add_library(xatlas STATIC
    src/3rd_party/xatlas/xatlas.cpp)
target_compile_options(xatlas PRIVATE 
    -std=c++11)

include_directories(src)
