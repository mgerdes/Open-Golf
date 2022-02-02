file(GLOB data_files "${CMAKE_SOURCE_DIR}/data/*")
add_custom_target(data_zip
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/data"
    COMMAND ${CMAKE_COMMAND} -E tar "cvf" "${CMAKE_SOURCE_DIR}/out/data.zip" --format=zip .
    DEPENDS ${data_files})
add_custom_target(android_data_zip
    DEPENDS data_zip
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/out/data.zip" "${CMAKE_SOURCE_DIR}/build/android/OpenGolf/app/src/main/assets/data.zip")

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
    add_library(golf SHARED
        main.c
        game.c
        ui.c)
    target_link_libraries(golf PRIVATE
        common)
    add_dependencies(golf android_data_zip)
else()
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
    else()
    endif()
    add_executable(golf
        draw.c
        game.c
        main.c
        ui.c)
    target_link_libraries(golf PRIVATE ${GOLF_LIBRARIES})
endif()
