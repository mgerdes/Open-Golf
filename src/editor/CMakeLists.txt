set(EDITOR_LIBRARIES
    common
    cimgui
    fast_obj
    glad
    glfw
    lightmapper
    mattiasgustavsson_libs
    miniz
    parson
    sokol
    stb
    xatlas)

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
elseif(CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(EDITOR_LIBRARIES 
        ${EDITOR_LIBRARIES}
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
add_executable(editor
    draw.c
    editor.c
    gi.c
    gizmo.c
    main.c)
target_link_libraries(editor PRIVATE ${EDITOR_LIBRARIES})

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
  target_compile_options(editor PRIVATE /W4 /WX)
else()
  target_compile_options(editor PRIVATE -Wall -Wextra -Wpedantic)
endif()
