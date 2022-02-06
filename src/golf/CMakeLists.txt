file(GLOB data_files "${CMAKE_SOURCE_DIR}/data/*")
add_custom_target(data_zip
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/data"
    COMMAND ${CMAKE_COMMAND} -E tar "cvf" "${CMAKE_SOURCE_DIR}/out/data.zip" --format=zip .
    DEPENDS ${data_files})
if(CMAKE_SYSTEM_NAME STREQUAL Android)
    add_custom_target(android_data_zip
        DEPENDS data_zip
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/out/data.zip" "${CMAKE_SOURCE_DIR}/build/android/OpenGolf/app/src/main/assets/data.zip")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL Android OR CMAKE_SYSTEM_NAME STREQUAL iOS)
    add_custom_target(embedded_data_zip_header
        DEPENDS data_zip
        COMMAND ${CMAKE_SOURCE_DIR}/tools/cembed/osx/cembed ${CMAKE_SOURCE_DIR}/out/data.zip > ${CMAKE_SOURCE_DIR}/src/common/data_zip.h)
endif()

set(GOLF_LIBRARIES
    common
    cimgui
    fast_obj
    mattiasgustavsson_libs
    miniz
    parson
    remotery
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
        main.c
        ui.c)
    target_link_libraries(golf PRIVATE ${GOLF_LIBRARIES})
    add_dependencies(golf android_data_zip)
else()
    add_executable(golf
        draw.c
        game.c
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
        add_dependencies(golf embedded_data_zip_header)
    else()
    endif()
    target_link_libraries(golf PRIVATE ${GOLF_LIBRARIES})
endif()
