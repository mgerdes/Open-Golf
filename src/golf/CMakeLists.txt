
set(GOLF_LIBRARIES
    common
    cimgui
    fast_obj
    mattiasgustavsson_libs
    miniz
    parson
    sokol
    stb
    xatlas)

if(CMAKE_SYSTEM_NAME STREQUAL Android)
    set(GOLF_LIBRARIES
        ${GOLF_LIBRARIES}
        GLESv3
        EGL 
        log 
        android
        OpenSLES)
    add_library(golf SHARED
        draw.c
        game.c
        golf.c
        main.c
        ui.c)
    target_link_libraries(golf PRIVATE ${GOLF_LIBRARIES})
else()
    add_executable(golf
        draw.c
        game.c
        golf.c
        main.c
        ui.c)
    if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    elseif(CMAKE_SYSTEM_NAME STREQUAL Linux)
        set(GOLF_LIBRARIES 
            ${GOLF_LIBRARIES}
            -lX11
            -lXcursor
            -lXi
            -lGL
            -ldl
            -lpthread
            -lm
            -lasound)
    elseif(CMAKE_SYSTEM_NAME STREQUAL iOS)
        set(GOLF_LIBRARIES
            -lc++
            ${GOLF_LIBRARIES}
            "-framework Foundation"
            "-framework UIKit"
            "-framework OpenGLES"
            "-framework GLKit"
            "-framework AudioToolbox"
            "-framework AvFoundation")
        set_target_properties(golf PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/build/ios/plist.in"
            RESOURCE "${CMAKE_SOURCE_DIR}/build/ios/LaunchScreen.storyboard")
    else()
    endif()
    target_link_libraries(golf PRIVATE ${GOLF_LIBRARIES})
endif()

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    target_compile_options(golf PRIVATE /W3)
else()
    target_compile_options(golf PRIVATE -Wall -Wextra -Wpedantic)
endif()
